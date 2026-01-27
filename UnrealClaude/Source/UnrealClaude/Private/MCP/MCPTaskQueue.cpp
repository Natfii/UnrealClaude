// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTaskQueue.h"
#include "MCPToolRegistry.h"
#include "UnrealClaudeModule.h"
#include "Async/Async.h"

FMCPTaskQueue::FMCPTaskQueue(FMCPToolRegistry* InToolRegistry)
	: ToolRegistry(InToolRegistry)
	, RunningTaskCount(0)
	, WorkerThread(nullptr)
	, bShouldStop(false)
	, WakeUpEvent(nullptr)
	, LastCleanupTime(FDateTime::UtcNow())
{
	WakeUpEvent = FPlatformProcess::GetSynchEventFromPool();
	if (!WakeUpEvent)
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to get sync event from pool for task queue"));
	}
}

FMCPTaskQueue::~FMCPTaskQueue()
{
	Shutdown();
	if (WakeUpEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(WakeUpEvent);
		WakeUpEvent = nullptr;
	}
}

void FMCPTaskQueue::Start()
{
	if (WorkerThread)
	{
		return; // Already running
	}

	// Reset stop flag before creating thread
	bShouldStop = false;

	// Create the thread with default parameters for stability
	WorkerThread = FRunnableThread::Create(
		this,
		TEXT("MCPTaskQueue"),
		0,  // Default stack size
		TPri_BelowNormal
	);

	if (WorkerThread)
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("MCP Task Queue started"));
	}
	else
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to create MCP Task Queue thread"));
	}
}

void FMCPTaskQueue::Stop()
{
	// This is called by FRunnableThread when Kill() is invoked
	// Set flag to stop the Run() loop
	bShouldStop = true;

	// Trigger event to wake up the thread if it's waiting
	if (WakeUpEvent)
	{
		WakeUpEvent->Trigger();
	}
}

void FMCPTaskQueue::Shutdown()
{
	FRunnableThread* ThreadToKill = WorkerThread;
	if (!ThreadToKill)
	{
		return;
	}

	// Clear the pointer first to prevent re-entry
	WorkerThread = nullptr;

	UE_LOG(LogUnrealClaude, Log, TEXT("MCP Task Queue shutting down..."));

	// Set stop flag explicitly (in case Kill doesn't call Stop properly)
	bShouldStop = true;

	// Trigger wake event to unblock any waits
	if (WakeUpEvent)
	{
		WakeUpEvent->Trigger();
	}

	// Kill the thread - this calls our Stop() method then waits
	ThreadToKill->Kill(true);

	delete ThreadToKill;

	UE_LOG(LogUnrealClaude, Log, TEXT("MCP Task Queue stopped"));
}

FGuid FMCPTaskQueue::SubmitTask(const FString& ToolName, TSharedPtr<FJsonObject> Parameters, uint32 TimeoutMs)
{
	// Validate tool exists
	if (ToolRegistry && !ToolRegistry->HasTool(ToolName))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Cannot submit task: Tool '%s' not found"), *ToolName);
		return FGuid();
	}

	// Create new task
	TSharedPtr<FMCPAsyncTask> Task = MakeShared<FMCPAsyncTask>();
	Task->ToolName = ToolName;
	Task->Parameters = Parameters;
	Task->TimeoutMs = TimeoutMs > 0 ? TimeoutMs : Config.DefaultTimeoutMs;

	// Add to task map and queue
	{
		FScopeLock Lock(&TasksLock);

		// Check if we're at capacity
		int32 ActiveTasks = 0;
		for (const auto& Pair : Tasks)
		{
			if (!Pair.Value->IsComplete())
			{
				ActiveTasks++;
			}
		}

		if (ActiveTasks >= Config.MaxHistorySize)
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Task queue at capacity (%d tasks), rejecting new task"), Config.MaxHistorySize);
			return FGuid();
		}

		Tasks.Add(Task->TaskId, Task);
		PendingQueue.Enqueue(Task->TaskId);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Task submitted: %s (tool: %s)"), *Task->TaskId.ToString(), *ToolName);

	// Wake up worker thread
	if (WakeUpEvent)
	{
		WakeUpEvent->Trigger();
	}

	return Task->TaskId;
}

TSharedPtr<FMCPAsyncTask> FMCPTaskQueue::GetTask(const FGuid& TaskId) const
{
	FScopeLock Lock(&TasksLock);
	const TSharedPtr<FMCPAsyncTask>* Found = Tasks.Find(TaskId);
	return Found ? *Found : nullptr;
}

bool FMCPTaskQueue::GetTaskResult(const FGuid& TaskId, FMCPToolResult& OutResult) const
{
	TSharedPtr<FMCPAsyncTask> Task = GetTask(TaskId);
	if (!Task.IsValid() || !Task->IsComplete())
	{
		return false;
	}

	OutResult = Task->Result;
	return true;
}

bool FMCPTaskQueue::CancelTask(const FGuid& TaskId)
{
	TSharedPtr<FMCPAsyncTask> Task = GetTask(TaskId);
	if (!Task.IsValid())
	{
		return false;
	}

	EMCPTaskStatus CurrentStatus = Task->Status.Load();
	if (CurrentStatus == EMCPTaskStatus::Pending)
	{
		// Can immediately cancel pending tasks
		Task->Status.Store(EMCPTaskStatus::Cancelled);
		Task->CompletedTime = FDateTime::UtcNow();
		Task->Result = FMCPToolResult::Error(TEXT("Task cancelled before execution"));
		UE_LOG(LogUnrealClaude, Log, TEXT("Task cancelled (pending): %s"), *TaskId.ToString());
		return true;
	}
	else if (CurrentStatus == EMCPTaskStatus::Running)
	{
		// Request cancellation for running tasks
		Task->bCancellationRequested = true;
		UE_LOG(LogUnrealClaude, Log, TEXT("Task cancellation requested (running): %s"), *TaskId.ToString());
		return true;
	}

	return false; // Already complete
}

TArray<TSharedPtr<FMCPAsyncTask>> FMCPTaskQueue::GetAllTasks(bool bIncludeCompleted) const
{
	TArray<TSharedPtr<FMCPAsyncTask>> Result;

	FScopeLock Lock(&TasksLock);
	for (const auto& Pair : Tasks)
	{
		if (bIncludeCompleted || !Pair.Value->IsComplete())
		{
			Result.Add(Pair.Value);
		}
	}

	// Sort by submitted time (newest first)
	Result.Sort([](const TSharedPtr<FMCPAsyncTask>& A, const TSharedPtr<FMCPAsyncTask>& B)
	{
		return A->SubmittedTime > B->SubmittedTime;
	});

	return Result;
}

void FMCPTaskQueue::GetStats(int32& OutPending, int32& OutRunning, int32& OutCompleted) const
{
	OutPending = 0;
	OutRunning = 0;
	OutCompleted = 0;

	FScopeLock Lock(&TasksLock);
	for (const auto& Pair : Tasks)
	{
		switch (Pair.Value->Status.Load())
		{
		case EMCPTaskStatus::Pending:
			OutPending++;
			break;
		case EMCPTaskStatus::Running:
			OutRunning++;
			break;
		default:
			OutCompleted++;
			break;
		}
	}
}

bool FMCPTaskQueue::Init()
{
	return true;
}

uint32 FMCPTaskQueue::Run()
{
	while (!bShouldStop)
	{
		// Check for pending tasks
		FGuid TaskId;
		bool bHasTask = false;

		{
			FScopeLock Lock(&TasksLock);
			if (RunningTaskCount.Load() < Config.MaxConcurrentTasks)
			{
				// Find next non-cancelled pending task
				while (PendingQueue.Dequeue(TaskId))
				{
					TSharedPtr<FMCPAsyncTask>* Found = Tasks.Find(TaskId);
					if (Found && (*Found)->Status.Load() == EMCPTaskStatus::Pending)
					{
						bHasTask = true;
						break;
					}
				}
			}
		}

		if (bHasTask)
		{
			TSharedPtr<FMCPAsyncTask> Task = GetTask(TaskId);
			if (Task.IsValid())
			{
				RunningTaskCount++;

				// Execute task asynchronously
				AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, Task]()
				{
					ExecuteTask(Task);
					RunningTaskCount--;
				});
			}
		}

		// Periodic cleanup (only every 60 seconds)
		FDateTime Now = FDateTime::UtcNow();
		if ((Now - LastCleanupTime).GetTotalSeconds() >= Config.CleanupIntervalSeconds)
		{
			CleanupOldTasks();
			CheckTimeouts();
			LastCleanupTime = Now;
		}

		// Sleep to avoid consuming 100% CPU and allow responsive shutdown
		FPlatformProcess::Sleep(0.01f);
	}

	return 0;
}

void FMCPTaskQueue::Exit()
{
	// Cancel all running tasks
	FScopeLock Lock(&TasksLock);
	for (auto& Pair : Tasks)
	{
		if (!Pair.Value->IsComplete())
		{
			Pair.Value->bCancellationRequested = true;
		}
	}
}

void FMCPTaskQueue::ExecuteTask(TSharedPtr<FMCPAsyncTask> Task)
{
	if (!Task.IsValid() || !ToolRegistry)
	{
		return;
	}

	// Mark as running
	Task->Status.Store(EMCPTaskStatus::Running);
	Task->StartedTime = FDateTime::UtcNow();

	UE_LOG(LogUnrealClaude, Log, TEXT("Task started: %s (tool: %s)"), *Task->TaskId.ToString(), *Task->ToolName);

	// Check for early cancellation
	if (Task->bCancellationRequested)
	{
		Task->Status.Store(EMCPTaskStatus::Cancelled);
		Task->CompletedTime = FDateTime::UtcNow();
		Task->Result = FMCPToolResult::Error(TEXT("Task cancelled"));
		return;
	}

	// Prepare parameters
	TSharedRef<FJsonObject> Params = Task->Parameters.IsValid()
		? Task->Parameters.ToSharedRef()
		: MakeShared<FJsonObject>();

	FMCPToolResult Result;

	// For long-running tools (execute_script), dispatch to game thread directly
	// with the task's own timeout instead of the registry's 30-second default.
	// This allows permission dialogs + Live Coding compilation to take their time.
	IMCPTool* Tool = ToolRegistry->FindTool(Task->ToolName);
	if (!Tool)
	{
		Result = FMCPToolResult::Error(FString::Printf(TEXT("Tool '%s' not found"), *Task->ToolName));
	}
	else if (Task->TimeoutMs > 30000)
	{
		// Long-running task: bypass registry's 30s game thread timeout
		TSharedPtr<FMCPToolResult> SharedResult = MakeShared<FMCPToolResult>();
		TSharedPtr<FEvent, ESPMode::ThreadSafe> CompletionEvent = MakeShareable(
			FPlatformProcess::GetSynchEventFromPool(),
			[](FEvent* Event) { FPlatformProcess::ReturnSynchEventToPool(Event); });
		TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> bCompleted = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);

		AsyncTask(ENamedThreads::GameThread, [SharedResult, Tool, Params, CompletionEvent, bCompleted]()
		{
			*SharedResult = Tool->Execute(Params);
			*bCompleted = true;
			CompletionEvent->Trigger();
		});

		const bool bSignaled = CompletionEvent->Wait(Task->TimeoutMs);
		if (!bSignaled || !(*bCompleted))
		{
			UE_LOG(LogUnrealClaude, Error, TEXT("Task '%s' timed out after %d ms on game thread"),
				*Task->ToolName, Task->TimeoutMs);
			Result = FMCPToolResult::Error(FString::Printf(
				TEXT("Task execution timed out after %d seconds"), Task->TimeoutMs / 1000));
		}
		else
		{
			Result = *SharedResult;
		}
	}
	else
	{
		// Normal tools: use registry's standard game thread dispatch (30s timeout)
		Result = ToolRegistry->ExecuteTool(Task->ToolName, Params);
	}

	// Check for cancellation after execution
	if (Task->bCancellationRequested)
	{
		Task->Status.Store(EMCPTaskStatus::Cancelled);
		Task->Result = FMCPToolResult::Error(TEXT("Task cancelled during execution"));
	}
	else
	{
		Task->Status.Store(Result.bSuccess ? EMCPTaskStatus::Completed : EMCPTaskStatus::Failed);
		Task->Result = Result;
	}

	Task->CompletedTime = FDateTime::UtcNow();
	Task->Progress.Store(100);

	FTimespan Duration = Task->CompletedTime - Task->StartedTime;
	UE_LOG(LogUnrealClaude, Log, TEXT("Task completed: %s (status: %s, duration: %.2fs)"),
		*Task->TaskId.ToString(),
		*FMCPAsyncTask::StatusToString(Task->Status.Load()),
		Duration.GetTotalSeconds());
}

void FMCPTaskQueue::CleanupOldTasks()
{
	FDateTime CutoffTime = FDateTime::UtcNow() - FTimespan::FromSeconds(Config.ResultRetentionSeconds);

	TArray<FGuid> TasksToRemove;

	{
		FScopeLock Lock(&TasksLock);
		for (const auto& Pair : Tasks)
		{
			if (Pair.Value->IsComplete() && Pair.Value->CompletedTime < CutoffTime)
			{
				TasksToRemove.Add(Pair.Key);
			}
		}

		for (const FGuid& Id : TasksToRemove)
		{
			Tasks.Remove(Id);
		}
	}

	if (TasksToRemove.Num() > 0)
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("Cleaned up %d old tasks"), TasksToRemove.Num());
	}
}

void FMCPTaskQueue::CheckTimeouts()
{
	FDateTime Now = FDateTime::UtcNow();

	FScopeLock Lock(&TasksLock);
	for (auto& Pair : Tasks)
	{
		TSharedPtr<FMCPAsyncTask>& Task = Pair.Value;
		if (Task->Status.Load() == EMCPTaskStatus::Running)
		{
			FTimespan Elapsed = Now - Task->StartedTime;
			if (Elapsed.GetTotalMilliseconds() > Task->TimeoutMs)
			{
				Task->bCancellationRequested = true;
				Task->Status.Store(EMCPTaskStatus::TimedOut);
				Task->CompletedTime = Now;
				Task->Result = FMCPToolResult::Error(
					FString::Printf(TEXT("Task timed out after %d ms"), Task->TimeoutMs));
				UE_LOG(LogUnrealClaude, Warning, TEXT("Task timed out: %s"), *Task->TaskId.ToString());
			}
		}
	}
}
