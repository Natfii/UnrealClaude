// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"
#include "MCP/MCPTaskQueue.h"

/**
 * MCP Tool: Get status of an async task
 *
 * Returns the current status and progress of a task submitted via task_submit.
 */
class FMCPTool_TaskStatus : public FMCPToolBase
{
public:
	FMCPTool_TaskStatus(TSharedPtr<FMCPTaskQueue> InTaskQueue)
		: TaskQueue(InTaskQueue)
	{
	}

	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("task_status");
		Info.Description = TEXT(
			"Get the status of an async task.\n\n"
			"Returns the current status, progress, and timing information for a task.\n\n"
			"Status values:\n"
			"- 'pending': Task is queued but not yet started\n"
			"- 'running': Task is currently executing\n"
			"- 'completed': Task finished successfully\n"
			"- 'failed': Task encountered an error\n"
			"- 'cancelled': Task was cancelled\n"
			"- 'timed_out': Task exceeded its timeout\n\n"
			"For completed tasks, use task_result to get the full output."
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

		// Build response
		TSharedPtr<FJsonObject> ResultData = Task->ToJson(false); // Don't include full result

		FString StatusStr = FMCPAsyncTask::StatusToString(Task->Status.Load());
		return FMCPToolResult::Success(
			FString::Printf(TEXT("Task %s: %s"), *TaskIdString, *StatusStr),
			ResultData);
	}

private:
	TSharedPtr<FMCPTaskQueue> TaskQueue;
};
