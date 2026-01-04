// Copyright Your Name. All Rights Reserved.

#include "MCPTool_CleanupScripts.h"
#include "ScriptExecutionManager.h"
#include "UnrealClaudeModule.h"

FMCPToolResult FMCPTool_CleanupScripts::Execute(const TSharedRef<FJsonObject>& Params)
{
	UE_LOG(LogUnrealClaude, Log, TEXT("Cleaning up all generated scripts and history"));

	FString ResultMessage = FScriptExecutionManager::Get().CleanupAll();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("message"), ResultMessage);

	return FMCPToolResult::Success(ResultMessage, ResultData);
}
