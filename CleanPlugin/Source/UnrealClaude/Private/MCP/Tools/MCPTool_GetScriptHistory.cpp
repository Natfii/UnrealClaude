// Copyright Your Name. All Rights Reserved.

#include "MCPTool_GetScriptHistory.h"
#include "ScriptExecutionManager.h"
#include "ScriptTypes.h"
#include "UnrealClaudeModule.h"

FMCPToolResult FMCPTool_GetScriptHistory::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Get count parameter
	int32 Count = 10;
	double CountDouble;
	if (Params->TryGetNumberField(TEXT("count"), CountDouble))
	{
		Count = FMath::Clamp(static_cast<int32>(CountDouble), 1, 50);
	}

	// Get recent scripts
	TArray<FScriptHistoryEntry> RecentScripts = FScriptExecutionManager::Get().GetRecentScripts(Count);

	// Build result array
	TArray<TSharedPtr<FJsonValue>> ScriptsArray;
	for (const FScriptHistoryEntry& Entry : RecentScripts)
	{
		TSharedPtr<FJsonObject> ScriptJson = MakeShared<FJsonObject>();
		ScriptJson->SetStringField(TEXT("type"), ScriptTypeToString(Entry.ScriptType));
		ScriptJson->SetStringField(TEXT("filename"), Entry.Filename);
		ScriptJson->SetStringField(TEXT("description"), Entry.Description);
		ScriptJson->SetBoolField(TEXT("success"), Entry.bSuccess);
		ScriptJson->SetStringField(TEXT("result"), Entry.ResultMessage);
		ScriptJson->SetStringField(TEXT("timestamp"), Entry.Timestamp.ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")));

		ScriptsArray.Add(MakeShared<FJsonValueObject>(ScriptJson));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("scripts"), ScriptsArray);
	ResultData->SetNumberField(TEXT("count"), RecentScripts.Num());

	// Also include formatted context string
	FString FormattedContext = FScriptExecutionManager::Get().FormatHistoryForContext(Count);
	ResultData->SetStringField(TEXT("formatted_context"), FormattedContext);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved %d recent script executions"), RecentScripts.Num()),
		ResultData
	);
}
