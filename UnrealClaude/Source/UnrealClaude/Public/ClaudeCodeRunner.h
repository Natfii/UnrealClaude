// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IClaudeRunner.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"

/**
 * Async runner for Claude Code CLI commands (cross-platform implementation)
 * Executes 'claude -p' in print mode and captures output
 * Implements IClaudeRunner interface for abstraction
 */
class UNREALCLAUDE_API FClaudeCodeRunner : public IClaudeRunner, public FRunnable
{
public:
	FClaudeCodeRunner();
	virtual ~FClaudeCodeRunner();

	// IClaudeRunner interface
	virtual bool ExecuteAsync(
		const FClaudeRequestConfig& Config,
		FOnClaudeResponse OnComplete,
		FOnClaudeProgress OnProgress = FOnClaudeProgress()
	) override;

	virtual bool ExecuteSync(const FClaudeRequestConfig& Config, FString& OutResponse) override;
	virtual void Cancel() override;
	virtual bool IsExecuting() const override { return bIsExecuting; }
	virtual bool IsAvailable() const override { return IsClaudeAvailable(); }

	/** Check if Claude CLI is available on this system (static for backward compatibility) */
	static bool IsClaudeAvailable();

	/** Get the Claude CLI path */
	static FString GetClaudePath();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	/** Build stream-json NDJSON payload with text + base64 image content blocks */
	FString BuildStreamJsonPayload(const FString& TextPrompt, const TArray<FString>& ImagePaths);

	/** Parse stream-json NDJSON output to extract the response text */
	FString ParseStreamJsonOutput(const FString& RawOutput);

private:
	FString BuildCommandLine(const FClaudeRequestConfig& Config);
	void ExecuteProcess();
	void CleanupHandles();

	/** Parse a single NDJSON line and emit structured events */
	void ParseAndEmitNdjsonLine(const FString& JsonLine);

	/** Buffer for accumulating incomplete NDJSON lines across read chunks */
	FString NdjsonLineBuffer;

	/** Accumulated text from assistant messages for the final response */
	FString AccumulatedResponseText;

	/** Create pipes for process stdout/stderr capture */
	bool CreateProcessPipes();

	/** Launch the Claude process with given command */
	bool LaunchProcess(const FString& FullCommand, const FString& WorkingDir);

	/** Read output from process until completion or cancellation */
	FString ReadProcessOutput();

	/** Report error to callback on game thread */
	void ReportError(const FString& ErrorMessage);

	/** Report completion to callback on game thread */
	void ReportCompletion(const FString& Output, bool bSuccess);

	FClaudeRequestConfig CurrentConfig;
	FOnClaudeResponse OnCompleteDelegate;
	FOnClaudeProgress OnProgressDelegate;

	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	TAtomic<bool> bIsExecuting;

	// Process handle (FProcHandle stored as void* for atomic exchange compatibility)
	FProcHandle ProcessHandle;

	// Pipe handles (UE cross-platform pipe handles)
	void* ReadPipe;
	void* WritePipe;
	void* StdInReadPipe;
	void* StdInWritePipe;

	// Temp file paths for prompts (to avoid command line length limits)
	FString SystemPromptFilePath;
	FString PromptFilePath;
};
