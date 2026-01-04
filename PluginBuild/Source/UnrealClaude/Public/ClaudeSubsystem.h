// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IClaudeRunner.h"

// Forward declarations
class FClaudeSessionManager;
class FClaudeCodeRunner;

/**
 * Options for sending a prompt to Claude
 * Reduces parameter count in SendPrompt method
 */
struct UNREALCLAUDE_API FClaudePromptOptions
{
	/** Include UE5.7 engine context in system prompt */
	bool bIncludeEngineContext = true;

	/** Include project-specific context in system prompt */
	bool bIncludeProjectContext = true;

	/** Optional callback for streaming output progress */
	FOnClaudeProgress OnProgress;

	/** Default constructor with sensible defaults */
	FClaudePromptOptions() = default;

	/** Convenience constructor for common case */
	FClaudePromptOptions(bool bEngineContext, bool bProjectContext)
		: bIncludeEngineContext(bEngineContext)
		, bIncludeProjectContext(bProjectContext)
	{}
};

/**
 * Subsystem for managing Claude Code interactions
 * Orchestrates runner, session management, and prompt building
 */
class UNREALCLAUDE_API FClaudeCodeSubsystem
{
public:
	static FClaudeCodeSubsystem& Get();

	/** Destructor - must be defined in cpp where full types are available */
	~FClaudeCodeSubsystem();

	/** Send a prompt to Claude with optional context (new API with options struct) */
	void SendPrompt(
		const FString& Prompt,
		FOnClaudeResponse OnComplete,
		const FClaudePromptOptions& Options = FClaudePromptOptions()
	);

	/** Send a prompt to Claude with optional context (legacy API for backward compatibility) */
	void SendPrompt(
		const FString& Prompt,
		FOnClaudeResponse OnComplete,
		bool bIncludeUE57Context,
		FOnClaudeProgress OnProgress,
		bool bIncludeProjectContext = true
	);

	/** Get the default UE5.7 system prompt */
	FString GetUE57SystemPrompt() const;

	/** Get the project context prompt */
	FString GetProjectContextPrompt() const;

	/** Set custom system prompt additions */
	void SetCustomSystemPrompt(const FString& InCustomPrompt);

	/** Get conversation history (delegates to session manager) */
	const TArray<TPair<FString, FString>>& GetHistory() const;

	/** Clear conversation history */
	void ClearHistory();

	/** Cancel current request */
	void CancelCurrentRequest();

	/** Save current session to disk */
	bool SaveSession();

	/** Load previous session from disk */
	bool LoadSession();

	/** Check if a previous session exists */
	bool HasSavedSession() const;

	/** Get session file path */
	FString GetSessionFilePath() const;

	/** Get the runner interface (for testing/mocking) */
	IClaudeRunner* GetRunner() const;

private:
	FClaudeCodeSubsystem();

	/** Build prompt with conversation history context */
	FString BuildPromptWithHistory(const FString& NewPrompt) const;

	TUniquePtr<FClaudeCodeRunner> Runner;
	TUniquePtr<FClaudeSessionManager> SessionManager;
	FString CustomSystemPrompt;
};
