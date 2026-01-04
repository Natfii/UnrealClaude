// Copyright Your Name. All Rights Reserved.

#include "ClaudeSubsystem.h"
#include "ClaudeCodeRunner.h"
#include "ClaudeSessionManager.h"
#include "ProjectContext.h"
#include "ScriptExecutionManager.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"

// Cached system prompt - static to avoid recreation on each call
static const FString CachedUE57SystemPrompt = TEXT(R"(You are an expert Unreal Engine 5.7 developer assistant integrated directly into the UE Editor.

CONTEXT:
- You are helping with an Unreal Engine 5.7 project
- The user is working in the Unreal Editor and expects UE5.7-specific guidance
- Focus on current UE5.7 APIs, patterns, and best practices

KEY UE5.7 FEATURES TO BE AWARE OF:
- Enhanced Nanite and Lumen for next-gen rendering
- World Partition for open world streaming
- Mass Entity (experimental) for large-scale simulations
- Enhanced Input System (preferred over legacy input)
- Common UI for cross-platform interfaces
- Gameplay Ability System (GAS) for complex ability systems
- MetaSounds for procedural audio
- Chaos physics engine (default)
- Control Rig for animation
- Niagara for VFX

CODING STANDARDS:
- Use UPROPERTY, UFUNCTION, UCLASS macros properly
- Follow Unreal naming conventions (F for structs, U for UObject, A for Actor, E for enums)
- Prefer BlueprintCallable/BlueprintPure for BP-exposed functions
- Use TObjectPtr<> for object pointers in headers (UE5+)
- Use Forward declarations in headers, includes in cpp
- Properly use GENERATED_BODY() macro

WHEN PROVIDING CODE:
- Always specify the correct includes
- Use proper UE5.7 API calls (not deprecated ones)
- Include both .h and .cpp when showing class implementations
- Explain any engine-specific gotchas or limitations

RESPONSE FORMAT:
- Be concise but thorough
- Provide code examples when helpful
- Mention relevant documentation or resources
- Warn about common pitfalls)");

FClaudeCodeSubsystem& FClaudeCodeSubsystem::Get()
{
	static FClaudeCodeSubsystem Instance;
	return Instance;
}

FClaudeCodeSubsystem::FClaudeCodeSubsystem()
{
	Runner = MakeUnique<FClaudeCodeRunner>();
	SessionManager = MakeUnique<FClaudeSessionManager>();
}

FClaudeCodeSubsystem::~FClaudeCodeSubsystem()
{
	// Destructor defined here where full types are available
	// TUniquePtr will properly destroy the objects
}

IClaudeRunner* FClaudeCodeSubsystem::GetRunner() const
{
	return Runner.Get();
}

void FClaudeCodeSubsystem::SendPrompt(
	const FString& Prompt,
	FOnClaudeResponse OnComplete,
	const FClaudePromptOptions& Options)
{
	FClaudeRequestConfig Config;

	// Build prompt with conversation history context
	Config.Prompt = BuildPromptWithHistory(Prompt);
	Config.WorkingDirectory = FPaths::ProjectDir();
	Config.bSkipPermissions = true;
	Config.AllowedTools = { TEXT("Read"), TEXT("Write"), TEXT("Edit"), TEXT("Grep"), TEXT("Glob"), TEXT("Bash") };

	if (Options.bIncludeEngineContext)
	{
		Config.SystemPrompt = GetUE57SystemPrompt();
	}

	if (Options.bIncludeProjectContext)
	{
		Config.SystemPrompt += GetProjectContextPrompt();
	}

	if (!CustomSystemPrompt.IsEmpty())
	{
		Config.SystemPrompt += TEXT("\n\n") + CustomSystemPrompt;
	}

	// Wrap completion to store history and save session
	FOnClaudeResponse WrappedComplete;
	WrappedComplete.BindLambda([this, Prompt, OnComplete](const FString& Response, bool bSuccess)
	{
		if (bSuccess && SessionManager.IsValid())
		{
			SessionManager->AddExchange(Prompt, Response);
			SessionManager->SaveSession();
		}
		OnComplete.ExecuteIfBound(Response, bSuccess);
	});

	Runner->ExecuteAsync(Config, WrappedComplete, Options.OnProgress);
}

void FClaudeCodeSubsystem::SendPrompt(
	const FString& Prompt,
	FOnClaudeResponse OnComplete,
	bool bIncludeUE57Context,
	FOnClaudeProgress OnProgress,
	bool bIncludeProjectContext)
{
	// Legacy API - delegate to new API
	FClaudePromptOptions Options;
	Options.bIncludeEngineContext = bIncludeUE57Context;
	Options.bIncludeProjectContext = bIncludeProjectContext;
	Options.OnProgress = OnProgress;
	SendPrompt(Prompt, OnComplete, Options);
}

FString FClaudeCodeSubsystem::GetUE57SystemPrompt() const
{
	// Return cached static prompt to avoid string recreation
	return CachedUE57SystemPrompt;
}

FString FClaudeCodeSubsystem::GetProjectContextPrompt() const
{
	FString Context = FProjectContextManager::Get().FormatContextForPrompt();

	// Add script execution history (last 10 scripts)
	FString ScriptHistory = FScriptExecutionManager::Get().FormatHistoryForContext(10);
	if (!ScriptHistory.IsEmpty())
	{
		Context += TEXT("\n\n") + ScriptHistory;
	}

	return Context;
}

void FClaudeCodeSubsystem::SetCustomSystemPrompt(const FString& InCustomPrompt)
{
	CustomSystemPrompt = InCustomPrompt;
}

const TArray<TPair<FString, FString>>& FClaudeCodeSubsystem::GetHistory() const
{
	static TArray<TPair<FString, FString>> EmptyHistory;
	if (SessionManager.IsValid())
	{
		return SessionManager->GetHistory();
	}
	return EmptyHistory;
}

void FClaudeCodeSubsystem::ClearHistory()
{
	if (SessionManager.IsValid())
	{
		SessionManager->ClearHistory();
	}
}

void FClaudeCodeSubsystem::CancelCurrentRequest()
{
	if (Runner.IsValid())
	{
		Runner->Cancel();
	}
}

bool FClaudeCodeSubsystem::SaveSession()
{
	if (SessionManager.IsValid())
	{
		return SessionManager->SaveSession();
	}
	return false;
}

bool FClaudeCodeSubsystem::LoadSession()
{
	if (SessionManager.IsValid())
	{
		return SessionManager->LoadSession();
	}
	return false;
}

bool FClaudeCodeSubsystem::HasSavedSession() const
{
	if (SessionManager.IsValid())
	{
		return SessionManager->HasSavedSession();
	}
	return false;
}

FString FClaudeCodeSubsystem::GetSessionFilePath() const
{
	if (SessionManager.IsValid())
	{
		return SessionManager->GetSessionFilePath();
	}
	return FString();
}

FString FClaudeCodeSubsystem::BuildPromptWithHistory(const FString& NewPrompt) const
{
	if (!SessionManager.IsValid())
	{
		return NewPrompt;
	}

	const TArray<TPair<FString, FString>>& History = SessionManager->GetHistory();
	if (History.Num() == 0)
	{
		return NewPrompt;
	}

	FString PromptWithHistory;

	// Include recent history (limit to last N exchanges)
	int32 StartIndex = FMath::Max(0, History.Num() - UnrealClaudeConstants::Session::MaxHistoryInPrompt);

	for (int32 i = StartIndex; i < History.Num(); ++i)
	{
		const TPair<FString, FString>& Exchange = History[i];
		PromptWithHistory += FString::Printf(TEXT("Human: %s\n\nAssistant: %s\n\n"), *Exchange.Key, *Exchange.Value);
	}

	PromptWithHistory += FString::Printf(TEXT("Human: %s"), *NewPrompt);

	return PromptWithHistory;
}
