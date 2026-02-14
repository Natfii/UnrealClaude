// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCPAsyncTask.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"

// Forward declarations
class FMCPToolRegistry;

/**
 * Configuration for the task queue
 */
struct FMCPTaskQueueConfig
{
	/** Maximum number of concurrent tasks */
	int32 MaxConcurrentTasks = 4;

	/** Maximum number of tasks to keep in history */
	int32 MaxHistorySize = 100;

	/** How long to keep completed task results (in seconds) */
	int32 ResultRetentionSeconds = 300; // 5 minutes

	/** Default timeout for tasks in milliseconds */
	uint32 DefaultTimeoutMs = 120000; // 2 minutes

	/** How often to clean up old tasks (in seconds) */
	int32 CleanupIntervalSeconds = 60;
};

/**
 * Manages async execution of MCP tools
 *
 * Allows tools to be submitted for background execution with:
 * - Task ID for tracking
 * - Status polling
 * - Result retrieval
 * - Cancellation support
 * - Automatic cleanup of old tasks
 */
class FMCPTaskQueue : public FRunnable
{
public:
	/** Constructor - takes a raw pointer since registry always outlives the queue */
	FMCPTaskQueue(FMCPToolRegistry* InToolRegistry);
	virtual ~FMCPTaskQueue();

	/** Start the task queue worker thread */
	void Start();

	/** Shutdown the task queue and cancel pending tasks (call from main thread) */
	void Shutdown();

	/**
	 * Submit a tool for async execution
	 * @param ToolName Name of the tool to execute
	 * @param Parameters Tool parameters
	 * @param TimeoutMs Optional timeout (0 = use default)
	 * @return Task ID for tracking, or invalid GUID on failure
	 */
	FGuid SubmitTask(const FString& ToolName, TSharedPtr<FJsonObject> Parameters, uint32 TimeoutMs = 0);

	/**
	 * Get the status of a task
	 * @param TaskId Task identifier
	 * @return Task info, or nullptr if not found
	 */
	TSharedPtr<FMCPAsyncTask> GetTask(const FGuid& TaskId) const;

	/**
	 * Get the result of a completed task
	 * @param TaskId Task identifier
	 * @param OutResult Output result
	 * @return true if task exists and is complete
	 */
	bool GetTaskResult(const FGuid& TaskId, FMCPToolResult& OutResult) const;

	/**
	 * Request cancellation of a task
	 * @param TaskId Task identifier
	 * @return true if cancellation was requested
	 */
	bool CancelTask(const FGuid& TaskId);

	/**
	 * Get all tasks (for debugging/listing)
	 * @param bIncludeCompleted Include completed tasks in the list
	 * @return Array of task info
	 */
	TArray<TSharedPtr<FMCPAsyncTask>> GetAllTasks(bool bIncludeCompleted = true) const;

	/**
	 * Get queue statistics
	 */
	void GetStats(int32& OutPending, int32& OutRunning, int32& OutCompleted) const;

	/** Configuration */
	FMCPTaskQueueConfig Config;

protected:
	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	/** Execute a single task */
	void ExecuteTask(TSharedPtr<FMCPAsyncTask> Task);

	/** Clean up old completed tasks */
	void CleanupOldTasks();

	/** Check for timed out tasks */
	void CheckTimeouts();

	/** Tool registry for executing tools (raw pointer - registry outlives queue) */
	FMCPToolRegistry* ToolRegistry;

	/** All tasks indexed by ID */
	TMap<FGuid, TSharedPtr<FMCPAsyncTask>> Tasks;

	/** Queue of pending task IDs */
	TQueue<FGuid, EQueueMode::Mpsc> PendingQueue;

	/** Currently running task count */
	TAtomic<int32> RunningTaskCount;

	/** Lock for task map access */
	mutable FCriticalSection TasksLock;

	/** Worker thread */
	FRunnableThread* WorkerThread;

	/** Flag to stop the worker */
	FThreadSafeBool bShouldStop;

	/** Event to wake up worker when new task is submitted */
	FEvent* WakeUpEvent;

	/** Last cleanup time */
	FDateTime LastCleanupTime;
};
