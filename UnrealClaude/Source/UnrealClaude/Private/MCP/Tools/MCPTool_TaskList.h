// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"
#include "MCP/MCPTaskQueue.h"

/**
 * MCP Tool: List all async tasks
 *
 * Returns a list of all tasks in the queue with their status.
 * Useful for debugging and monitoring.
 */
class FMCPTool_TaskList : public FMCPToolBase
{
public:
	FMCPTool_TaskList(TSharedPtr<FMCPTaskQueue> InTaskQueue)
		: TaskQueue(InTaskQueue)
	{
	}

	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("task_list");
		Info.Description = TEXT(
			"List all async tasks in the queue.\n\n"
			"Returns task IDs, tool names, status, and timing information for all tasks. "
			"Useful for monitoring and debugging async operations.\n\n"
			"Options:\n"
			"- include_completed: Whether to include finished tasks (default: true)\n"
			"- limit: Maximum number of tasks to return (default: 50)"
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("include_completed"), TEXT("boolean"),
				TEXT("Include completed/failed/cancelled tasks (default: true)"), false, TEXT("true")),
			FMCPToolParameter(TEXT("limit"), TEXT("number"),
				TEXT("Maximum number of tasks to return (default: 50)"), false, TEXT("50"))
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

		bool bIncludeCompleted = ExtractOptionalBool(Params, TEXT("include_completed"), true);
		int32 Limit = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("limit"), 50), 1, 500);

		// Get all tasks
		TArray<TSharedPtr<FMCPAsyncTask>> AllTasks = TaskQueue->GetAllTasks(bIncludeCompleted);

		// Get stats
		int32 Pending, Running, Completed;
		TaskQueue->GetStats(Pending, Running, Completed);

		// Build task array
		TArray<TSharedPtr<FJsonValue>> TaskArray;
		int32 Count = 0;
		for (const TSharedPtr<FMCPAsyncTask>& Task : AllTasks)
		{
			if (Count >= Limit)
			{
				break;
			}
			TaskArray.Add(MakeShared<FJsonValueObject>(Task->ToJson(false)));
			Count++;
		}

		// Build result
		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetArrayField(TEXT("tasks"), TaskArray);
		ResultData->SetNumberField(TEXT("count"), TaskArray.Num());
		ResultData->SetNumberField(TEXT("total_pending"), Pending);
		ResultData->SetNumberField(TEXT("total_running"), Running);
		ResultData->SetNumberField(TEXT("total_completed"), Completed);

		FString Message = FString::Printf(
			TEXT("Found %d tasks (pending: %d, running: %d, completed: %d)"),
			TaskArray.Num(), Pending, Running, Completed);

		return FMCPToolResult::Success(Message, ResultData);
	}

private:
	TSharedPtr<FMCPTaskQueue> TaskQueue;
};
