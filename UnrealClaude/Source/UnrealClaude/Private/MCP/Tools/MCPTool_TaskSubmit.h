// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"
#include "MCP/MCPTaskQueue.h"

/**
 * MCP Tool: Submit a tool for async execution
 *
 * Submits any MCP tool for background execution, returning a task ID
 * that can be used to poll for status and retrieve results.
 */
class FMCPTool_TaskSubmit : public FMCPToolBase
{
public:
	FMCPTool_TaskSubmit(TSharedPtr<FMCPTaskQueue> InTaskQueue)
		: TaskQueue(InTaskQueue)
	{
	}

	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("task_submit");
		Info.Description = TEXT(
			"Submit an MCP tool for async background execution.\n\n"
			"Use this for long-running operations that might timeout with synchronous execution. "
			"Returns a task_id that you can use with task_status and task_result to track progress "
			"and retrieve results.\n\n"
			"Workflow:\n"
			"1. Call task_submit with tool name and parameters\n"
			"2. Poll task_status with the returned task_id\n"
			"3. When status is 'completed', call task_result to get output\n\n"
			"Example:\n"
			"  task_submit(tool_name='asset_search', params={class_filter: 'Blueprint'})\n"
			"  -> Returns: {task_id: '...'}\n"
			"  task_status(task_id='...')\n"
			"  -> Returns: {status: 'running', progress: 50}\n"
			"  task_result(task_id='...')\n"
			"  -> Returns: {success: true, data: {...}}"
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("tool_name"), TEXT("string"),
				TEXT("Name of the MCP tool to execute asynchronously"), true),
			FMCPToolParameter(TEXT("params"), TEXT("object"),
				TEXT("Parameters to pass to the tool (same as calling the tool directly)"), false),
			FMCPToolParameter(TEXT("timeout_ms"), TEXT("number"),
				TEXT("Custom timeout in milliseconds (default: 120000 = 2 minutes)"), false, TEXT("120000"))
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

		// Extract tool name
		FString ToolName;
		TOptional<FMCPToolResult> Error;
		if (!ExtractRequiredString(Params, TEXT("tool_name"), ToolName, Error))
		{
			return Error.GetValue();
		}

		// Extract parameters for the tool
		TSharedPtr<FJsonObject> ToolParams;
		const TSharedPtr<FJsonObject>* ParamsObj;
		if (Params->TryGetObjectField(TEXT("params"), ParamsObj))
		{
			ToolParams = *ParamsObj;
		}
		else
		{
			ToolParams = MakeShared<FJsonObject>();
		}

		// Extract timeout
		uint32 TimeoutMs = static_cast<uint32>(ExtractOptionalNumber<int32>(Params, TEXT("timeout_ms"), 120000));

		// Submit the task
		FGuid TaskId = TaskQueue->SubmitTask(ToolName, ToolParams, TimeoutMs);

		if (!TaskId.IsValid())
		{
			return FMCPToolResult::Error(TEXT("Failed to submit task - queue may be at capacity or tool not found"));
		}

		// Return task ID
		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetStringField(TEXT("task_id"), TaskId.ToString());
		ResultData->SetStringField(TEXT("tool_name"), ToolName);
		ResultData->SetStringField(TEXT("status"), TEXT("pending"));
		ResultData->SetNumberField(TEXT("timeout_ms"), TimeoutMs);

		return FMCPToolResult::Success(
			FString::Printf(TEXT("Task submitted: %s"), *TaskId.ToString()),
			ResultData);
	}

private:
	TSharedPtr<FMCPTaskQueue> TaskQueue;
};
