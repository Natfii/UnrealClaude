// Copyright Your Name. All Rights Reserved.

#include "MCPTool_ExecuteScript.h"
#include "ScriptExecutionManager.h"
#include "ScriptTypes.h"
#include "UnrealClaudeModule.h"

FMCPToolResult FMCPTool_ExecuteScript::Execute(const TSharedRef<FJsonObject>& Params)
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
	ResultData->SetStringField(TEXT("output"), Result.Output);
	ResultData->SetNumberField(TEXT("retry_count"), Result.RetryCount);

	if (Result.bSuccess)
	{
		return FMCPToolResult::Success(Result.Message, ResultData);
	}
	else
	{
		// Include error output for Claude to fix
		ResultData->SetStringField(TEXT("error"), Result.ErrorOutput);

		// Return error with detailed info for auto-retry
		FMCPToolResult ErrorResult;
		ErrorResult.bSuccess = false;
		ErrorResult.Message = Result.Message;
		ErrorResult.Data = ResultData;
		return ErrorResult;
	}
}
