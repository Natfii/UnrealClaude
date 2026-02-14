// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"
#include "MCP/MCPTaskQueue.h"

/**
 * MCP Tool: Get result of a completed async task
 *
 * Returns the full result data of a completed task.
 * For pending/running tasks, use task_status instead.
 */
class FMCPTool_TaskResult : public FMCPToolBase
{
public:
	FMCPTool_TaskResult(TSharedPtr<FMCPTaskQueue> InTaskQueue)
		: TaskQueue(InTaskQueue)
	{
	}

	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("task_result");
		Info.Description = TEXT(
			"Get the result of a completed async task.\n\n"
			"Returns the full result data including success/failure status, message, and output data.\n"
			"Only works for tasks in a terminal state (completed, failed, cancelled, timed_out).\n\n"
			"For pending or running tasks, this will return an error - use task_status to poll."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("task_id"), TEXT("string"),
				TEXT("Task ID returned from task_submit"), true)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override
	{
		if (!TaskQueue.IsValid())
		{
			return FMCPToolResult::Error(TEXT("Task queue not initialized"));
		}

		// Extract task ID
		FString TaskIdString;
		TOptional<FMCPToolResult> Error;
		if (!ExtractRequiredString(Params, TEXT("task_id"), TaskIdString, Error))
		{
			return Error.GetValue();
		}

		// Parse GUID
		FGuid TaskId;
		if (!FGuid::Parse(TaskIdString, TaskId))
		{
			return FMCPToolResult::Error(TEXT("Invalid task_id format"));
		}

		// Get task
		TSharedPtr<FMCPAsyncTask> Task = TaskQueue->GetTask(TaskId);
		if (!Task.IsValid())
		{
			return FMCPToolResult::Error(
				FString::Printf(TEXT("Task not found: %s"), *TaskIdString));
		}

		// Check if task is complete
		if (!Task->IsComplete())
		{
			EMCPTaskStatus Status = Task->Status.Load();
			return FMCPToolResult::Error(
				FString::Printf(TEXT("Task is still %s - use task_status to poll"),
					*FMCPAsyncTask::StatusToString(Status)));
		}

		// Build response with full result
		TSharedPtr<FJsonObject> ResultData = Task->ToJson(true); // Include full result

		FMCPToolResult Result;
		Result.bSuccess = Task->Result.bSuccess;
		Result.Message = Task->Result.Message;
		Result.Data = ResultData;
		return Result;
	}

private:
	TSharedPtr<FMCPTaskQueue> TaskQueue;
};
