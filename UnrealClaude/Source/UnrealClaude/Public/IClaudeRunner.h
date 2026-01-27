// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Forward declare delegates for the interface
DECLARE_DELEGATE_TwoParams(FOnClaudeResponse, const FString& /*Response*/, bool /*bSuccess*/);
DECLARE_DELEGATE_OneParam(FOnClaudeProgress, const FString& /*PartialOutput*/);

/**
 * Types of structured events from Claude stream-json NDJSON output.
 * Maps to the message types documented in Claude Code SDK spec.
 */
enum class EClaudeStreamEventType : uint8
{
	/** Session initialization (system.init) */
	SessionInit,
	/** Text content from assistant */
	TextContent,
	/** Tool use block from assistant (tool invocation) */
	ToolUse,
	/** Tool result returned to Claude (user message with tool_result) */
	ToolResult,
	/** Final result with stats and cost */
	Result,
	/** Raw assistant message (full message, not parsed into sub-events) */
	AssistantMessage,
	/** Unknown or unparsed event type */
	Unknown
};

/**
 * Structured event parsed from Claude CLI stream-json NDJSON output.
 * Each NDJSON line becomes one of these events.
 */
struct UNREALCLAUDE_API FClaudeStreamEvent
{
	/** Event type */
	EClaudeStreamEventType Type = EClaudeStreamEventType::Unknown;

	/** Text content (for TextContent events) */
	FString Text;

	/** Tool name (for ToolUse events) */
	FString ToolName;

	/** Tool input JSON string (for ToolUse events) */
	FString ToolInput;

	/** Tool call ID (for ToolUse/ToolResult events) */
	FString ToolCallId;

	/** Tool result content (for ToolResult events) */
	FString ToolResultContent;

	/** Session ID (for SessionInit/Result events) */
	FString SessionId;

	/** Whether this is an error event */
	bool bIsError = false;

	/** Duration in ms (for Result events) */
	int32 DurationMs = 0;

	/** Number of turns (for Result events) */
	int32 NumTurns = 0;

	/** Total cost in USD (for Result events) */
	float TotalCostUsd = 0.0f;

	/** Result text (for Result events) */
	FString ResultText;

	/** Raw JSON line for debugging */
	FString RawJson;
};

/** Delegate for structured stream events */
DECLARE_DELEGATE_OneParam(FOnClaudeStreamEvent, const FClaudeStreamEvent& /*Event*/);

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

	/** Optional paths to attached clipboard images (PNG) for Claude to read */
	TArray<FString> AttachedImagePaths;

	/** Optional callback for structured NDJSON stream events */
	FOnClaudeStreamEvent OnStreamEvent;
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
