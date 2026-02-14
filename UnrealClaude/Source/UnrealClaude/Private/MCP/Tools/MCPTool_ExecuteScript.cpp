// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_ExecuteScript.h"
#include "MCP/MCPTaskQueue.h"
#include "ScriptExecutionManager.h"
#include "ScriptTypes.h"
#include "UnrealClaudeModule.h"

FMCPToolResult FMCPTool_ExecuteScript::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Check if this is a synchronous call from the task queue
	bool bSync = false;
	Params->TryGetBoolField(TEXT("_sync"), bSync);

	if (bSync)
	{
		// Called from task queue - execute synchronously
		return ExecuteSync(Params);
	}

	// Async execution - submit to task queue
	if (!TaskQueue.IsValid())
	{
		// Fallback to sync if no task queue (shouldn't happen in practice)
		UE_LOG(LogUnrealClaude, Warning, TEXT("execute_script: No task queue available, falling back to sync execution"));
		return ExecuteSync(Params);
	}

	// Clone params and add _sync flag for when task queue executes
	TSharedPtr<FJsonObject> AsyncParams = MakeShared<FJsonObject>();
	for (const auto& Field : Params->Values)
	{
		AsyncParams->SetField(Field.Key, FJsonValue::Duplicate(Field.Value));
	}
	AsyncParams->SetBoolField(TEXT("_sync"), true);

	// Submit with 10-minute timeout for permission dialogs
	constexpr uint32 ScriptTimeoutMs = 600000; // 10 minutes
	FGuid TaskId = TaskQueue->SubmitTask(TEXT("execute_script"), AsyncParams, ScriptTimeoutMs);

	if (!TaskId.IsValid())
	{
		return FMCPToolResult::Error(TEXT("Failed to submit script execution task - queue may be at capacity"));
	}

	// Return task ID for polling
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("task_id"), TaskId.ToString());
	ResultData->SetStringField(TEXT("status"), TEXT("pending"));
	ResultData->SetStringField(TEXT("message"), TEXT("Script submitted for execution. Use task_status/task_result to check progress."));
	ResultData->SetNumberField(TEXT("timeout_ms"), ScriptTimeoutMs);

	// Include script info
	FString ScriptType, Description;
	Params->TryGetStringField(TEXT("script_type"), ScriptType);
	Params->TryGetStringField(TEXT("description"), Description);
	ResultData->SetStringField(TEXT("script_type"), ScriptType);
	if (!Description.IsEmpty())
	{
		ResultData->SetStringField(TEXT("description"), Description);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Script execution queued. Task ID: %s. Poll task_status('%s') for progress."),
			*TaskId.ToString(), *TaskId.ToString()),
		ResultData);
}

FMCPToolResult FMCPTool_ExecuteScript::ExecuteSync(const TSharedRef<FJsonObject>& Params)
{
	// Get script type
	FString ScriptTypeStr;
	if (!Params->TryGetStringField(TEXT("script_type"), ScriptTypeStr))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: script_type"));
	}

	// Get script content
	FString ScriptContent;
	if (!Params->TryGetStringField(TEXT("script_content"), ScriptContent))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: script_content"));
	}

	// Get optional description
	FString Description;
	Params->TryGetStringField(TEXT("description"), Description);

	// Parse script type
	EScriptType ScriptType = StringToScriptType(ScriptTypeStr);

	// Validate that script has @Description in header
	FString HeaderDescription = ScriptHeader::ParseDescription(ScriptContent);
	if (HeaderDescription == TEXT("No description provided") && Description.IsEmpty())
	{
		return FMCPToolResult::Error(
			TEXT("Script MUST include @Description in header comment, or provide 'description' parameter. ")
			TEXT("Example header:\n")
			TEXT("/**\n")
			TEXT(" * @UnrealClaude Script\n")
			TEXT(" * @Description: What this script does\n")
			TEXT(" */")
		);
	}

	// Use header description if no parameter provided
	if (Description.IsEmpty())
	{
		Description = HeaderDescription;
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Executing %s script: %s"), *ScriptTypeStr, *Description);

	// Execute script via manager
	FScriptExecutionResult Result = FScriptExecutionManager::Get().ExecuteScript(
		ScriptType,
		ScriptContent,
		Description
	);

	// Build result data
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("script_type"), ScriptTypeStr);
	ResultData->SetStringField(TEXT("description"), Description);
	ResultData->SetNumberField(TEXT("retry_count"), Result.RetryCount);

	// Add compile_status for C++ scripts
	if (ScriptTypeStr.ToLower() == TEXT("cpp"))
	{
		ResultData->SetStringField(TEXT("compile_status"),
			Result.bSuccess ? TEXT("success") : TEXT("failed"));
	}

	if (Result.bSuccess)
	{
		ResultData->SetStringField(TEXT("output"), Result.Output);
		return FMCPToolResult::Success(Result.Message, ResultData);
	}
	else
	{
		// Always include full output so errors aren't lost
		ResultData->SetStringField(TEXT("output"),
			Result.Output.IsEmpty() ? Result.ErrorOutput : Result.Output);
		ResultData->SetStringField(TEXT("error"), Result.ErrorOutput);

		// Classify error type for easier handling
		FString ErrorType = TEXT("execution_error");
		if (ScriptTypeStr.ToLower() == TEXT("cpp"))
		{
			ErrorType = TEXT("compile_error");
		}
		else if (Result.ErrorOutput.Contains(TEXT("SyntaxError")) ||
				 Result.ErrorOutput.Contains(TEXT("IndentationError")))
		{
			ErrorType = TEXT("syntax_error");
		}
		else if (Result.ErrorOutput.Contains(TEXT("Traceback")))
		{
			ErrorType = TEXT("runtime_error");
		}
		ResultData->SetStringField(TEXT("error_type"), ErrorType);

		// Return error with detailed info for auto-retry
		FMCPToolResult ErrorResult;
		ErrorResult.bSuccess = false;
		ErrorResult.Message = Result.Message;
		ErrorResult.Data = ResultData;
		return ErrorResult;
	}
}
