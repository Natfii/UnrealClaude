// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTaskQueue.h"
#include "MCPToolRegistry.h"
#include "UnrealClaudeModule.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"

FMCPTaskQueue::FMCPTaskQueue(FMCPToolRegistry* InToolRegistry)
	: ToolRegistry(InToolRegistry)
	, RunningTaskCount(0)
	, WorkerThread(nullptr)
	, bShouldStop(false)
	, WakeUpEvent(nullptr)
	, LastCleanupTime(FDateTime::UtcNow())
{
	ActiveAsyncJobs = MakeShared<TAtomic<int32>, ESPMode::ThreadSafe>(0);
	AsyncJobsIdleEvent = MakeShareable(
		FPlatformProcess::GetSynchEventFromPool(),
		[](FEvent* Event) { FPlatformProcess::ReturnSynchEventToPool(Event); });
	if (AsyncJobsIdleEvent)
	{
		AsyncJobsIdleEvent->Trigger();
	}

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
	AsyncJobsIdleEvent.Reset();
	ActiveAsyncJobs.Reset();
}

void FMCPTaskQueue::Start()
{
	if (WorkerThread)
	{
		return; // Already running
	}

	bShouldStop = false;

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
	// Called by FRunnableThread when Kill() is invoked. Serialize the stop
	// transition with task insertion so a submit racing with shutdown cannot
	// enqueue a task after the worker has been stopped.
	{
		FScopeLock Lock(&TasksLock);
		bShouldStop = true;
	}

	if (WakeUpEvent)
	{
		WakeUpEvent->Trigger();
	}
}

void FMCPTaskQueue::Shutdown()
{
	FRunnableThread* ThreadToKill = WorkerThread;

	UE_LOG(LogUnrealClaude, Log, TEXT("MCP Task Queue shutting down..."));
	{
		FScopeLock Lock(&TasksLock);
		bShouldStop = true;

		// Mark pending tasks cancelled while holding the same lock used by
		// SubmitTask(). This closes the shutdown/submit race and prevents a
		// pending task from being stranded after the worker exits.
		for (auto& Pair : Tasks)
		{
			TSharedPtr<FMCPAsyncTask>& Task = Pair.Value;
			FScopeLock StateScopeLock(&Task->StateLock);
			if (Task->Status.Load() == EMCPTaskStatus::Pending)
			{
				Task->bCancellationRequested = true;
				Task->Status.Store(EMCPTaskStatus::Cancelled);
				Task->CompletedTime = FDateTime::UtcNow();
				Task->Result = FMCPToolResult::Error(TEXT("Task cancelled because the task queue is shutting down"));
			}
		}
	}
	if (WakeUpEvent)
	{
		WakeUpEvent->Trigger();
	}

	// Running tasks might be blocked waiting for a game-thread ticker. Cancel
	// those callbacks before waiting for the detached workers, otherwise shutdown
	// from the game thread could deadlock waiting for itself.
	CancelGameThreadExecutions();

	if (ThreadToKill)
	{
		WorkerThread = nullptr;
		ThreadToKill->Kill(true);
		delete ThreadToKill;
	}

	// Detached jobs capture task data, not the queue lifetime. Wait until every
	// job has finished before the registry destroys the tools they execute.
	if (ActiveAsyncJobs.IsValid() && AsyncJobsIdleEvent.IsValid() && ActiveAsyncJobs->Load() > 0)
	{
		AsyncJobsIdleEvent->Wait();
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("MCP Task Queue stopped"));
}

FGuid FMCPTaskQueue::SubmitTask(const FString& ToolName, TSharedPtr<FJsonObject> Parameters, uint32 TimeoutMs)
{
	if (bShouldStop)
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Cannot submit task while task queue is stopped"));
		return FGuid();
	}

	if (ToolRegistry && !ToolRegistry->HasTool(ToolName))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Cannot submit task: Tool '%s' not found"), *ToolName);
		return FGuid();
	}

	TSharedPtr<FMCPAsyncTask> Task = MakeShared<FMCPAsyncTask>();
	Task->ToolName = ToolName;
	Task->Parameters = Parameters;
	Task->TimeoutMs = TimeoutMs > 0 ? TimeoutMs : Config.DefaultTimeoutMs;

	{
		FScopeLock Lock(&TasksLock);

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
	if (!Task.IsValid())
	{
		return false;
	}

	FScopeLock StateScopeLock(&Task->StateLock);
	if (!Task->IsComplete())
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

	FScopeLock StateScopeLock(&Task->StateLock);
	const EMCPTaskStatus CurrentStatus = Task->Status.Load();
	if (CurrentStatus == EMCPTaskStatus::Pending)
	{
		Task->bCancellationRequested = true;
		Task->Status.Store(EMCPTaskStatus::Cancelled);
		Task->CompletedTime = FDateTime::UtcNow();
		Task->Result = FMCPToolResult::Error(TEXT("Task cancelled before execution"));
		UE_LOG(LogUnrealClaude, Log, TEXT("Task cancelled (pending): %s"), *TaskId.ToString());
		return true;
	}
	if (CurrentStatus == EMCPTaskStatus::Running)
	{
		Task->bCancellationRequested = true;
		UE_LOG(LogUnrealClaude, Log, TEXT("Task cancellation requested (running): %s"), *TaskId.ToString());
		return true;
	}

	return false;
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
		FGuid TaskId;
		bool bHasTask = false;

		{
			FScopeLock Lock(&TasksLock);
			if (RunningTaskCount.Load() < Config.MaxConcurrentTasks)
			{
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
			if (Task.IsValid() && ActiveAsyncJobs.IsValid())
			{
				RunningTaskCount++;
				if (AsyncJobsIdleEvent.IsValid() && ActiveAsyncJobs->Load() == 0)
				{
					AsyncJobsIdleEvent->Reset();
				}
				ActiveAsyncJobs->IncrementExchange();

				// Shutdown waits for ActiveAsyncJobs to reach zero before destroying
				// this queue or the registry and tools it owns.
				TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> SharedActiveJobs = ActiveAsyncJobs;
				TSharedPtr<FEvent, ESPMode::ThreadSafe> SharedIdleEvent = AsyncJobsIdleEvent;
				AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, Task, SharedActiveJobs, SharedIdleEvent]()
				{
					ExecuteTask(Task);
					RunningTaskCount--;
					const int32 Remaining = SharedActiveJobs->DecrementExchange();
					if (Remaining == 0 && SharedIdleEvent.IsValid())
					{
						SharedIdleEvent->Trigger();
					}
				});
			}
		}

		FDateTime Now = FDateTime::UtcNow();
		if ((Now - LastCleanupTime).GetTotalSeconds() >= Config.CleanupIntervalSeconds)
		{
			CleanupOldTasks();
			CheckTimeouts();
			LastCleanupTime = Now;
		}

		// Block on the wake event instead of busy-polling at 10 ms.
		// SubmitTask / Stop / Shutdown trigger WakeUpEvent; cleanup interval is the max wait.
		if (WakeUpEvent)
		{
			const uint32 WaitMs = static_cast<uint32>(FMath::Max(100.0f, Config.CleanupIntervalSeconds * 1000.0f));
			WakeUpEvent->Wait(WaitMs);
		}
		else
		{
			FPlatformProcess::Sleep(0.05f);
		}
	}

	return 0;
}

void FMCPTaskQueue::Exit()
{
	FScopeLock Lock(&TasksLock);
	for (auto& Pair : Tasks)
	{
		FScopeLock StateScopeLock(&Pair.Value->StateLock);
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

	{
		FScopeLock StateScopeLock(&Task->StateLock);
		if (Task->Status.Load() != EMCPTaskStatus::Pending)
		{
			return;
		}
		Task->Status.Store(EMCPTaskStatus::Running);
		Task->StartedTime = FDateTime::UtcNow();
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Task started: %s (tool: %s)"), *Task->TaskId.ToString(), *Task->ToolName);

	{
		FScopeLock StateScopeLock(&Task->StateLock);
		if (Task->bCancellationRequested)
		{
			Task->Status.Store(EMCPTaskStatus::Cancelled);
			Task->CompletedTime = FDateTime::UtcNow();
			Task->Result = FMCPToolResult::Error(TEXT("Task cancelled"));
			return;
		}
	}

	TSharedRef<FJsonObject> Params = Task->Parameters.IsValid()
		? Task->Parameters.ToSharedRef()
		: MakeShared<FJsonObject>();

	FMCPToolResult Result;

	IMCPTool* Tool = ToolRegistry->FindTool(Task->ToolName);
	if (!Tool)
	{
		Result = FMCPToolResult::Error(FString::Printf(TEXT("Tool '%s' not found"), *Task->ToolName));
	}
	else
	{
		// Execute every queued tool through the same ticker-based game-thread
		// path. This avoids the registry's fixed 30-second wait and gives the
		// task its own timeout. The ticker handle is retained on the task so
		// shutdown can cancel it before waiting for worker jobs.
		TSharedPtr<FMCPToolResult> SharedResult = MakeShared<FMCPToolResult>();
		TSharedPtr<FEvent, ESPMode::ThreadSafe> CompletionEvent = MakeShareable(
			FPlatformProcess::GetSynchEventFromPool(),
			[](FEvent* Event) { FPlatformProcess::ReturnSynchEventToPool(Event); });
		TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> bCompleted = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);

		FDelegateHandle TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("MCPTask_Execute"), 0.0f,
			[SharedResult, Tool, Params, CompletionEvent, bCompleted, Task](float) -> bool
		{
			{
				FScopeLock StateScopeLock(&Task->StateLock);
				if (Task->bCancellationRequested)
				{
					CompletionEvent->Trigger();
					return false;
				}
			}

			*SharedResult = Tool->Execute(Params);
			*bCompleted = true;
			CompletionEvent->Trigger();
			return false;
		});

		bool bCancelImmediately = false;
		{
			FScopeLock StateScopeLock(&Task->StateLock);
			Task->GameThreadCompletionEvent = CompletionEvent;
			Task->GameThreadTickerHandle = TickerHandle;
			bCancelImmediately = Task->bCancellationRequested;
		}

		// Shutdown/cancellation might have won the race before the ticker handle
		// was stored. Remove the callback and wake the worker immediately.
		if (bCancelImmediately)
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			CompletionEvent->Trigger();
		}

		const bool bSignaled = CompletionEvent->Wait(Task->TimeoutMs);
		{
			FScopeLock StateScopeLock(&Task->StateLock);
			if (Task->GameThreadTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(Task->GameThreadTickerHandle);
				Task->GameThreadTickerHandle.Reset();
			}
			Task->GameThreadCompletionEvent.Reset();
		}

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

	{
		FScopeLock StateScopeLock(&Task->StateLock);
		if (Task->Status.Load() == EMCPTaskStatus::TimedOut)
		{
			// Timeout is a terminal state and wins the race with task completion.
		}
		else if (Task->bCancellationRequested)
		{
			Task->Status.Store(EMCPTaskStatus::Cancelled);
			Task->Result = FMCPToolResult::Error(TEXT("Task cancelled during execution"));
			Task->CompletedTime = FDateTime::UtcNow();
		}
		else
		{
			Task->Status.Store(Result.bSuccess ? EMCPTaskStatus::Completed : EMCPTaskStatus::Failed);
			Task->Result = Result;
			Task->CompletedTime = FDateTime::UtcNow();
		}
	}

	{
		FScopeLock StateScopeLock(&Task->StateLock);
		if (Task->Status.Load() == EMCPTaskStatus::Completed ||
			Task->Status.Load() == EMCPTaskStatus::Failed)
		{
			Task->Progress.Store(100);
		}
	}

	FTimespan Duration = Task->CompletedTime - Task->StartedTime;
	UE_LOG(LogUnrealClaude, Log, TEXT("Task completed: %s (status: %s, duration: %.2fs)"),
		*Task->TaskId.ToString(),
		*FMCPAsyncTask::StatusToString(Task->Status.Load()),
		Duration.GetTotalSeconds());
}

void FMCPTaskQueue::CancelGameThreadExecutions()
{
	FScopeLock TasksScopeLock(&TasksLock);
	for (auto& Pair : Tasks)
	{
		TSharedPtr<FMCPAsyncTask>& Task = Pair.Value;
		FScopeLock StateScopeLock(&Task->StateLock);
		if (Task->Status.Load() != EMCPTaskStatus::Running)
		{
			continue;
		}

		Task->bCancellationRequested = true;
		if (Task->GameThreadTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(Task->GameThreadTickerHandle);
			Task->GameThreadTickerHandle.Reset();
		}
		if (Task->GameThreadCompletionEvent.IsValid())
		{
			Task->GameThreadCompletionEvent->Trigger();
		}
	}
}

void FMCPTaskQueue::CleanupOldTasks()
{
	FDateTime CutoffTime = FDateTime::UtcNow() - FTimespan::FromSeconds(Config.ResultRetentionSeconds);

	TArray<FGuid> TasksToRemove;

	{
		FScopeLock Lock(&TasksLock);
		for (const auto& Pair : Tasks)
		{
			FScopeLock StateScopeLock(&Pair.Value->StateLock);
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
		FScopeLock StateScopeLock(&Task->StateLock);
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
				// Wake a worker waiting for game-thread execution.
				if (Task->GameThreadCompletionEvent.IsValid())
				{
					Task->GameThreadCompletionEvent->Trigger();
				}
				UE_LOG(LogUnrealClaude, Warning, TEXT("Task timed out: %s"), *Task->TaskId.ToString());
			}
		}
	}
}
