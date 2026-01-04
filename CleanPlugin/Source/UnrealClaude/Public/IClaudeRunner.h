// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Forward declare delegates for the interface
DECLARE_DELEGATE_TwoParams(FOnClaudeResponse, const FString& /*Response*/, bool /*bSuccess*/);
DECLARE_DELEGATE_OneParam(FOnClaudeProgress, const FString& /*PartialOutput*/);

/**
 * Configuration for Claude Code CLI execution
 */
struct UNREALCLAUDE_API FClaudeRequestConfig
{
	/** The prompt to send to Claude */
	FString Prompt;

	/** Optional system prompt to append (for UE5.7 context) */
	FString SystemPrompt;

	/** Working directory for Claude (usually project root) */
	FString WorkingDirectory;

	/** Use JSON output format for structured responses */
	bool bUseJsonOutput = false;

	/** Skip permission prompts (--dangerously-skip-permissions) */
	bool bSkipPermissions = true;

	/** Timeout in seconds (0 = no timeout) */
	float TimeoutSeconds = 300.0f;

	/** Allow Claude to use tools (Read, Write, Bash, etc.) */
	TArray<FString> AllowedTools;
};

/**
 * Abstract interface for Claude Code CLI runners
 * Allows for different implementations (real, mock, cached, etc.)
 */
class UNREALCLAUDE_API IClaudeRunner
{
public:
	virtual ~IClaudeRunner() = default;

	/**
	 * Execute a Claude Code CLI command asynchronously
	 * @param Config - Request configuration
	 * @param OnComplete - Callback when execution completes
	 * @param OnProgress - Optional callback for streaming output
	 * @return true if execution started successfully
	 */
	virtual bool ExecuteAsync(
		const FClaudeRequestConfig& Config,
		FOnClaudeResponse OnComplete,
		FOnClaudeProgress OnProgress = FOnClaudeProgress()
	) = 0;

	/**
	 * Execute a Claude Code CLI command synchronously (blocking)
	 * @param Config - Request configuration
	 * @param OutResponse - Output response string
	 * @return true if execution succeeded
	 */
	virtual bool ExecuteSync(const FClaudeRequestConfig& Config, FString& OutResponse) = 0;

	/** Cancel the current execution */
	virtual void Cancel() = 0;

	/** Check if currently executing */
	virtual bool IsExecuting() const = 0;

	/** Check if the runner is available (CLI installed, etc.) */
	virtual bool IsAvailable() const = 0;
};
