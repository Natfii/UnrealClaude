// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IClaudeRunner.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

/**
 * Async runner for Claude Code CLI commands (Windows implementation)
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

private:
	FString BuildCommandLine(const FClaudeRequestConfig& Config);
	void ExecuteProcess();
	void CleanupHandles();

#if PLATFORM_WINDOWS
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
#endif

	FClaudeRequestConfig CurrentConfig;
	FOnClaudeResponse OnCompleteDelegate;
	FOnClaudeProgress OnProgressDelegate;

	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	TAtomic<bool> bIsExecuting;

	// Process handles for cancellation (Windows HANDLE stored as void*)
	void* ProcessHandle;
	void* ReadPipe;
	void* WritePipe;
};
