// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPToolRegistry.h"
#include "MCPTaskQueue.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"
#include "Containers/Ticker.h"

#include "Tools/MCPTool_SpawnActor.h"
#include "Tools/MCPTool_GetLevelActors.h"
#include "Tools/MCPTool_SetProperty.h"
#include "Tools/MCPTool_RunConsoleCommand.h"
#include "Tools/MCPTool_DeleteActors.h"
#include "Tools/MCPTool_MoveActor.h"
#include "Tools/MCPTool_GetOutputLog.h"
#include "Tools/MCPTool_ExecuteScript.h"
#include "Tools/MCPTool_CleanupScripts.h"
#include "Tools/MCPTool_GetScriptHistory.h"
#include "Tools/MCPTool_CaptureViewport.h"
#include "Tools/MCPTool_BlueprintQuery.h"
#include "Tools/MCPTool_BlueprintModify.h"
#include "Tools/MCPTool_AnimBlueprintModify.h"
#include "Tools/MCPTool_AssetSearch.h"
#include "Tools/MCPTool_AssetDependencies.h"
#include "Tools/MCPTool_AssetReferencers.h"
#include "Tools/MCPTool_EnhancedInput.h"
#include "Tools/MCPTool_Character.h"
#include "Tools/MCPTool_CharacterData.h"
#include "Tools/MCPTool_Material.h"
#include "Tools/MCPTool_Asset.h"
#include "Tools/MCPTool_OpenLevel.h"

#include "Tools/MCPTool_TaskSubmit.h"
#include "Tools/MCPTool_TaskStatus.h"
#include "Tools/MCPTool_TaskResult.h"
#include "Tools/MCPTool_TaskList.h"
#include "Tools/MCPTool_TaskCancel.h"

FMCPToolRegistry::FMCPToolRegistry()
{
	RegisterBuiltinTools();
}

FMCPToolRegistry::~FMCPToolRegistry()
{
	StopTaskQueue();
	Tools.Empty();
}

void FMCPToolRegistry::StartTaskQueue()
{
	if (TaskQueue.IsValid())
	{
		TaskQueue->Start();
	}
}

void FMCPToolRegistry::StopTaskQueue()
{
	if (TaskQueue.IsValid())
	{
		TaskQueue->Shutdown();
	}
}

void FMCPToolRegistry::RegisterBuiltinTools()
{
	UE_LOG(LogUnrealClaude, Log, TEXT("Registering MCP tools..."));

	RegisterTool(MakeShared<FMCPTool_SpawnActor>());
	RegisterTool(MakeShared<FMCPTool_GetLevelActors>());
	RegisterTool(MakeShared<FMCPTool_SetProperty>());
	RegisterTool(MakeShared<FMCPTool_RunConsoleCommand>());
	RegisterTool(MakeShared<FMCPTool_DeleteActors>());
	RegisterTool(MakeShared<FMCPTool_MoveActor>());
	RegisterTool(MakeShared<FMCPTool_GetOutputLog>());

	RegisterTool(MakeShared<FMCPTool_ExecuteScript>());
	RegisterTool(MakeShared<FMCPTool_CleanupScripts>());
	RegisterTool(MakeShared<FMCPTool_GetScriptHistory>());

	RegisterTool(MakeShared<FMCPTool_CaptureViewport>());

	RegisterTool(MakeShared<FMCPTool_BlueprintQuery>());
	RegisterTool(MakeShared<FMCPTool_BlueprintModify>());
	RegisterTool(MakeShared<FMCPTool_AnimBlueprintModify>());

	RegisterTool(MakeShared<FMCPTool_AssetSearch>());
	RegisterTool(MakeShared<FMCPTool_AssetDependencies>());
	RegisterTool(MakeShared<FMCPTool_AssetReferencers>());

	RegisterTool(MakeShared<FMCPTool_EnhancedInput>());

	RegisterTool(MakeShared<FMCPTool_Character>());
	RegisterTool(MakeShared<FMCPTool_CharacterData>());

	RegisterTool(MakeShared<FMCPTool_Material>());
	RegisterTool(MakeShared<FMCPTool_Asset>());

	RegisterTool(MakeShared<FMCPTool_OpenLevel>());

	// Task queue takes a raw pointer since the registry always outlives it
	TaskQueue = MakeShared<FMCPTaskQueue>(this);

	// Wire up execute_script to use the task queue for async execution
	// This allows script execution to handle permission dialogs without timing out
	if (TSharedPtr<IMCPTool>* ExecuteScriptToolPtr = Tools.Find(TEXT("execute_script")))
	{
		if (FMCPTool_ExecuteScript* ExecuteScriptTool = static_cast<FMCPTool_ExecuteScript*>(ExecuteScriptToolPtr->Get()))
		{
			ExecuteScriptTool->SetTaskQueue(TaskQueue);
			UE_LOG(LogUnrealClaude, Log, TEXT("  Wired up execute_script to task queue for async execution"));
		}
	}

	RegisterTool(MakeShared<FMCPTool_TaskSubmit>(TaskQueue));
	RegisterTool(MakeShared<FMCPTool_TaskStatus>(TaskQueue));
	RegisterTool(MakeShared<FMCPTool_TaskResult>(TaskQueue));
	RegisterTool(MakeShared<FMCPTool_TaskList>(TaskQueue));
	RegisterTool(MakeShared<FMCPTool_TaskCancel>(TaskQueue));

	UE_LOG(LogUnrealClaude, Log, TEXT("Registered %d MCP tools"), Tools.Num());
}

void FMCPToolRegistry::RegisterTool(TSharedPtr<IMCPTool> Tool)
{
	if (!Tool.IsValid())
	{
		return;
	}

	FMCPToolInfo Info = Tool->GetInfo();
	if (Info.Name.IsEmpty())
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Cannot register tool with empty name"));
		return;
	}

	if (Tools.Contains(Info.Name))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Tool '%s' is already registered, replacing"), *Info.Name);
	}

	Tools.Add(Info.Name, Tool);
	UE_LOG(LogUnrealClaude, Log, TEXT("  Registered tool: %s"), *Info.Name);
}

void FMCPToolRegistry::UnregisterTool(const FString& ToolName)
{
	if (Tools.Remove(ToolName) > 0)
	{
		InvalidateToolCache();
		UE_LOG(LogUnrealClaude, Log, TEXT("Unregistered tool: %s"), *ToolName);
	}
}

void FMCPToolRegistry::InvalidateToolCache()
{
	bCacheValid = false;
	CachedToolInfo.Empty();
}

TArray<FMCPToolInfo> FMCPToolRegistry::GetAllTools() const
{
	if (bCacheValid)
	{
		return CachedToolInfo;
	}

	CachedToolInfo.Empty(Tools.Num());
	for (const auto& Pair : Tools)
	{
		if (Pair.Value.IsValid())
		{
			CachedToolInfo.Add(Pair.Value->GetInfo());
		}
	}
	bCacheValid = true;

	return CachedToolInfo;
}

FMCPToolResult FMCPToolRegistry::ExecuteTool(const FString& ToolName, const TSharedRef<FJsonObject>& Params)
{
	TSharedPtr<IMCPTool>* FoundTool = Tools.Find(ToolName);
	if (!FoundTool || !FoundTool->IsValid())
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Tool '%s' not found"), *ToolName));
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Executing MCP tool: %s"), *ToolName);

	// Execute on game thread to ensure safe access to engine objects
	FMCPToolResult Result;

	if (IsInGameThread())
	{
		Result = (*FoundTool)->Execute(Params);
	}
	else
	{
		// If called from non-game thread, dispatch to game thread and wait with timeout
		// Use shared pointers for all state to avoid use-after-free if timeout occurs
		TSharedPtr<FMCPToolResult> SharedResult = MakeShared<FMCPToolResult>();
		TSharedPtr<FEvent, ESPMode::ThreadSafe> CompletionEvent = MakeShareable(FPlatformProcess::GetSynchEventFromPool(),
			[](FEvent* Event) { FPlatformProcess::ReturnSynchEventToPool(Event); });
		TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> bTaskCompleted = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);

		// Hold a shared reference to the tool inside the ticker callback.
		// Capturing FoundTool would retain a pointer into the registry's TMap.
		// The registry may be destroyed or the entry replaced while the callback
		// is waiting on the game thread, leaving the callback with a dangling pointer.
		TSharedPtr<IMCPTool> Tool = *FoundTool;

		// Use FTSTicker to dispatch to game thread at a safe point between subsystem ticks.
		// AsyncTask(GameThread) can fire during streaming manager iteration, causing
		// re-entrancy into LevelRenderAssetManagersLock (assertion crash).
		FDelegateHandle TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("MCPTool_Execute"), 0.0f,
			[SharedResult, Tool, Params, CompletionEvent, bTaskCompleted](float) -> bool
		{
			if (!Tool.IsValid())
			{
				*SharedResult = FMCPToolResult::Error(TEXT("MCP tool is no longer available"));
				*bTaskCompleted = true;
				CompletionEvent->Trigger();
				return false;
			}

			*SharedResult = Tool->Execute(Params);
			*bTaskCompleted = true;
			CompletionEvent->Trigger();
			return false; // One-shot, don't reschedule
		});

		// Wait with timeout to prevent indefinite hangs
		const uint32 TimeoutMs = UnrealClaudeConstants::MCPServer::GameThreadTimeoutMs;
		const bool bSignaled = CompletionEvent->Wait(TimeoutMs);

		if (!bSignaled || !(*bTaskCompleted))
		{
			// Prevent a timed-out request from leaving a pending ticker callback
			// behind. Removing the handle does not interrupt a callback already
			// running on the game thread, so the callback still uses shared state.
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			UE_LOG(LogUnrealClaude, Error, TEXT("Tool '%s' execution timed out after %d ms"), *ToolName, TimeoutMs);
			return FMCPToolResult::Error(FString::Printf(TEXT("Tool execution timed out after %d seconds"), TimeoutMs / 1000));
		}

		Result = *SharedResult;
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Tool '%s' execution %s: %s"),
		*ToolName,
		Result.bSuccess ? TEXT("succeeded") : TEXT("failed"),
		*Result.Message);

	return Result;
}

bool FMCPToolRegistry::HasTool(const FString& ToolName) const
{
	return Tools.Contains(ToolName);
}
