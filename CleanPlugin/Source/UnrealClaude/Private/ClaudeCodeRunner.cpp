// Copyright Your Name. All Rights Reserved.

#include "ClaudeCodeRunner.h"
#include "UnrealClaudeModule.h"
#include "ProjectContext.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

// Only compile on Windows
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

FClaudeCodeRunner::FClaudeCodeRunner()
	: Thread(nullptr)
	, bIsExecuting(false)
	, ProcessHandle(nullptr)
	, ReadPipe(nullptr)
	, WritePipe(nullptr)
{
}

FClaudeCodeRunner::~FClaudeCodeRunner()
{
	// Signal stop FIRST (thread-safe) before touching anything
	StopTaskCounter.Set(1);

	// Wait for thread to exit BEFORE touching handles
	if (Thread)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	// NOW safe to cleanup handles (thread has exited)
	CleanupHandles();
}

void FClaudeCodeRunner::CleanupHandles()
{
#if PLATFORM_WINDOWS
	if (ProcessHandle)
	{
		CloseHandle((HANDLE)ProcessHandle);
		ProcessHandle = nullptr;
	}
	if (ReadPipe)
	{
		CloseHandle((HANDLE)ReadPipe);
		ReadPipe = nullptr;
	}
	if (WritePipe)
	{
		CloseHandle((HANDLE)WritePipe);
		WritePipe = nullptr;
	}
#endif
}

bool FClaudeCodeRunner::IsClaudeAvailable()
{
#if PLATFORM_WINDOWS
	FString ClaudePath = GetClaudePath();
	return !ClaudePath.IsEmpty();
#else
	return false;
#endif
}

FString FClaudeCodeRunner::GetClaudePath()
{
#if PLATFORM_WINDOWS
	// Cache the path to avoid repeated lookups and log spam
	static FString CachedClaudePath;
	static bool bHasSearched = false;

	if (bHasSearched)
	{
		return CachedClaudePath;
	}
	bHasSearched = true;

	// Check common locations for claude CLI
	TArray<FString> PossiblePaths;
	
	// npm global install location
	FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	if (!AppData.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(AppData, TEXT("npm"), TEXT("claude.cmd")));
	}
	
	// Local AppData npm
	FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
	if (!LocalAppData.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(LocalAppData, TEXT("npm"), TEXT("claude.cmd")));
	}
	
	// User profile npm
	FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	if (!UserProfile.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT("AppData"), TEXT("Roaming"), TEXT("npm"), TEXT("claude.cmd")));
	}
	
	// Check PATH - try to find claude.cmd or claude.exe
	FString PathEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	TArray<FString> PathDirs;
	PathEnv.ParseIntoArray(PathDirs, TEXT(";"), true);
	
	for (const FString& Dir : PathDirs)
	{
		PossiblePaths.Add(FPaths::Combine(Dir, TEXT("claude.cmd")));
		PossiblePaths.Add(FPaths::Combine(Dir, TEXT("claude.exe")));
	}
	
	// Check each path
	for (const FString& Path : PossiblePaths)
	{
		if (IFileManager::Get().FileExists(*Path))
		{
			UE_LOG(LogUnrealClaude, Log, TEXT("Found Claude CLI at: %s"), *Path);
			CachedClaudePath = Path;
			return CachedClaudePath;
		}
	}

	// Try using 'where' command as fallback
	FString WhereOutput;
	FString WhereErrors;
	int32 ReturnCode;

	if (FPlatformProcess::ExecProcess(TEXT("where"), TEXT("claude"), &ReturnCode, &WhereOutput, &WhereErrors) && ReturnCode == 0)
	{
		WhereOutput.TrimStartAndEndInline();
		TArray<FString> Lines;
		WhereOutput.ParseIntoArrayLines(Lines);
		if (Lines.Num() > 0)
		{
			UE_LOG(LogUnrealClaude, Log, TEXT("Found Claude CLI via 'where': %s"), *Lines[0]);
			CachedClaudePath = Lines[0];
			return CachedClaudePath;
		}
	}

	UE_LOG(LogUnrealClaude, Warning, TEXT("Claude CLI not found. Please install with: npm install -g @anthropic-ai/claude-code"));
#endif

	// CachedClaudePath remains empty if not found
	return CachedClaudePath;
}

bool FClaudeCodeRunner::ExecuteAsync(
	const FClaudeRequestConfig& Config,
	FOnClaudeResponse OnComplete,
	FOnClaudeProgress OnProgress)
{
	// Use atomic compare-exchange for thread-safe check-and-set
	bool Expected = false;
	if (!bIsExecuting.CompareExchange(Expected, true))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Claude is already executing a request"));
		return false;
	}

	if (!IsClaudeAvailable())
	{
		bIsExecuting = false;
		OnComplete.ExecuteIfBound(TEXT("Claude CLI not found. Please install with: npm install -g @anthropic-ai/claude-code"), false);
		return false;
	}

	// Clean up old thread if exists (from previous completed execution)
	if (Thread)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	CurrentConfig = Config;
	OnCompleteDelegate = OnComplete;
	OnProgressDelegate = OnProgress;

	// Start the execution thread
	Thread = FRunnableThread::Create(this, TEXT("ClaudeCodeRunner"), 0, TPri_Normal);

	if (!Thread)
	{
		bIsExecuting = false;
		return false;
	}
	return true;
}

bool FClaudeCodeRunner::ExecuteSync(const FClaudeRequestConfig& Config, FString& OutResponse)
{
	if (!IsClaudeAvailable())
	{
		OutResponse = TEXT("Claude CLI not found. Please install with: npm install -g @anthropic-ai/claude-code");
		return false;
	}
	
	FString ClaudePath = GetClaudePath();
	FString CommandLine = BuildCommandLine(Config);
	
	UE_LOG(LogUnrealClaude, Log, TEXT("Executing Claude: %s %s"), *ClaudePath, *CommandLine);
	
	FString StdOut;
	FString StdErr;
	int32 ReturnCode;
	
	// Set working directory
	FString WorkingDir = Config.WorkingDirectory;
	if (WorkingDir.IsEmpty())
	{
		WorkingDir = FPaths::ProjectDir();
	}
	
	bool bSuccess = FPlatformProcess::ExecProcess(
		*ClaudePath,
		*CommandLine,
		&ReturnCode,
		&StdOut,
		&StdErr,
		*WorkingDir
	);
	
	if (bSuccess && ReturnCode == 0)
	{
		OutResponse = StdOut;
		return true;
	}
	else
	{
		OutResponse = StdErr.IsEmpty() ? StdOut : StdErr;
		UE_LOG(LogUnrealClaude, Error, TEXT("Claude execution failed: %s"), *OutResponse);
		return false;
	}
}

// Helper function to properly escape command line arguments for Windows cmd.exe
// This handles all shell metacharacters to prevent command injection
static FString EscapeCommandLineArg(const FString& Arg)
{
	FString Escaped = Arg;

	// First, escape backslashes that precede quotes (Windows cmd escaping rules)
	// Must be done before escaping quotes themselves
	Escaped = Escaped.Replace(TEXT("\\\""), TEXT("\\\\\""));

	// Escape double quotes with backslash
	Escaped = Escaped.Replace(TEXT("\""), TEXT("\\\""));

	// Escape Windows cmd.exe shell metacharacters with caret (^)
	// These characters have special meaning in cmd.exe
	Escaped = Escaped.Replace(TEXT("^"), TEXT("^^"));  // Caret itself (escape char)
	Escaped = Escaped.Replace(TEXT("&"), TEXT("^&"));  // Command separator
	Escaped = Escaped.Replace(TEXT("|"), TEXT("^|"));  // Pipe
	Escaped = Escaped.Replace(TEXT("<"), TEXT("^<"));  // Input redirect
	Escaped = Escaped.Replace(TEXT(">"), TEXT("^>"));  // Output redirect
	Escaped = Escaped.Replace(TEXT("("), TEXT("^("));  // Grouping
	Escaped = Escaped.Replace(TEXT(")"), TEXT("^)"));  // Grouping

	// Escape percent signs (environment variable expansion)
	// Need to double them to escape in cmd.exe
	Escaped = Escaped.Replace(TEXT("%"), TEXT("%%"));

	// Escape backticks (used in some shells, safer to escape)
	Escaped = Escaped.Replace(TEXT("`"), TEXT("^`"));

	// Escape exclamation marks (delayed expansion in cmd.exe)
	Escaped = Escaped.Replace(TEXT("!"), TEXT("^^!"));

	return Escaped;
}

// Get the plugin directory path
static FString GetPluginDirectory()
{
	// Try engine plugins first (installed location)
	FString EnginePluginPath = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("Marketplace"), TEXT("UnrealClaude"));
	if (FPaths::DirectoryExists(EnginePluginPath))
	{
		return EnginePluginPath;
	}

	// Try project plugins
	FString ProjectPluginPath = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UnrealClaude"));
	if (FPaths::DirectoryExists(ProjectPluginPath))
	{
		return ProjectPluginPath;
	}

	return FString();
}

FString FClaudeCodeRunner::BuildCommandLine(const FClaudeRequestConfig& Config)
{
	FString CommandLine;

	// Print mode (non-interactive)
	CommandLine += TEXT("-p ");

	// Verbose mode to show thinking
	CommandLine += TEXT("--verbose ");

	// Skip permissions if requested
	if (Config.bSkipPermissions)
	{
		CommandLine += TEXT("--dangerously-skip-permissions ");
	}

	// JSON output if requested
	if (Config.bUseJsonOutput)
	{
		CommandLine += TEXT("--output-format json ");
	}

	// MCP config for editor tools
	FString PluginDir = GetPluginDirectory();
	if (!PluginDir.IsEmpty())
	{
		FString MCPBridgePath = FPaths::Combine(PluginDir, TEXT("Resources"), TEXT("mcp-bridge"), TEXT("index.js"));
		FPaths::NormalizeFilename(MCPBridgePath);
		MCPBridgePath = FPaths::ConvertRelativePathToFull(MCPBridgePath);

		if (FPaths::FileExists(MCPBridgePath))
		{
			// Write MCP config to temp file (Claude CLI needs a file path)
			FString MCPConfigDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealClaude"));
			IFileManager::Get().MakeDirectory(*MCPConfigDir, true);

			FString MCPConfigPath = FPaths::Combine(MCPConfigDir, TEXT("mcp-config.json"));
			FString MCPConfigContent = FString::Printf(
				TEXT("{\n  \"mcpServers\": {\n    \"unrealclaude\": {\n      \"command\": \"node\",\n      \"args\": [\"%s\"],\n      \"env\": {\n        \"UNREAL_MCP_URL\": \"http://localhost:3000\"\n      }\n    }\n  }\n}"),
				*MCPBridgePath.Replace(TEXT("\\"), TEXT("/"))
			);

			if (FFileHelper::SaveStringToFile(MCPConfigContent, *MCPConfigPath))
			{
				FString EscapedConfigPath = MCPConfigPath.Replace(TEXT("\\"), TEXT("/"));
				CommandLine += FString::Printf(TEXT("--mcp-config \"%s\" "), *EscapedConfigPath);
				UE_LOG(LogUnrealClaude, Log, TEXT("MCP config written to: %s"), *MCPConfigPath);
			}
			else
			{
				UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to write MCP config to: %s"), *MCPConfigPath);
			}
		}
		else
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("MCP bridge not found at: %s"), *MCPBridgePath);
		}
	}

	// Allowed tools - add MCP tools
	TArray<FString> AllTools = Config.AllowedTools;
	AllTools.Add(TEXT("mcp__unrealclaude__*")); // Allow all unrealclaude MCP tools
	if (AllTools.Num() > 0)
	{
		CommandLine += FString::Printf(TEXT("--allowedTools \"%s\" "), *FString::Join(AllTools, TEXT(",")));
	}

	// System prompt (append mode to keep Claude Code's built-in context)
	if (!Config.SystemPrompt.IsEmpty())
	{
		FString EscapedSystemPrompt = EscapeCommandLineArg(Config.SystemPrompt);
		CommandLine += FString::Printf(TEXT("--append-system-prompt \"%s\" "), *EscapedSystemPrompt);
	}

	// The prompt itself - escape and normalize whitespace
	FString EscapedPrompt = EscapeCommandLineArg(Config.Prompt);
	EscapedPrompt = EscapedPrompt.Replace(TEXT("\n"), TEXT(" "));
	EscapedPrompt = EscapedPrompt.Replace(TEXT("\r"), TEXT(" "));
	CommandLine += FString::Printf(TEXT("\"%s\""), *EscapedPrompt);

	return CommandLine;
}

void FClaudeCodeRunner::Cancel()
{
	StopTaskCounter.Set(1);

#if PLATFORM_WINDOWS
	// Use atomic exchange for thread-safe handle extraction
	// This prevents double-close race conditions with ExecuteProcess()
	HANDLE LocalProcess = (HANDLE)FPlatformAtomics::InterlockedExchangePtr(&ProcessHandle, nullptr);
	HANDLE LocalRead = (HANDLE)FPlatformAtomics::InterlockedExchangePtr(&ReadPipe, nullptr);
	HANDLE LocalWrite = (HANDLE)FPlatformAtomics::InterlockedExchangePtr(&WritePipe, nullptr);

	if (LocalProcess)
	{
		TerminateProcess(LocalProcess, 1);
		CloseHandle(LocalProcess);
	}
	if (LocalRead)
	{
		CloseHandle(LocalRead);
	}
	if (LocalWrite)
	{
		CloseHandle(LocalWrite);
	}
#endif
}

bool FClaudeCodeRunner::Init()
{
	// bIsExecuting is already set by ExecuteAsync (thread-safe)
	StopTaskCounter.Reset();
	return true;
}

uint32 FClaudeCodeRunner::Run()
{
	ExecuteProcess();
	return 0;
}

void FClaudeCodeRunner::Stop()
{
	StopTaskCounter.Increment();
}

void FClaudeCodeRunner::Exit()
{
	bIsExecuting = false;
}

void FClaudeCodeRunner::ExecuteProcess()
{
#if PLATFORM_WINDOWS
	FString ClaudePath = GetClaudePath();
	FString CommandLine = BuildCommandLine(CurrentConfig);
	
	UE_LOG(LogUnrealClaude, Log, TEXT("Async executing Claude: %s %s"), *ClaudePath, *CommandLine);
	
	// Set working directory
	FString WorkingDir = CurrentConfig.WorkingDirectory;
	if (WorkingDir.IsEmpty())
	{
		WorkingDir = FPaths::ProjectDir();
	}
	
	// Create pipes for stdout
	SECURITY_ATTRIBUTES SecurityAttributes;
	SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	SecurityAttributes.bInheritHandle = true;
	SecurityAttributes.lpSecurityDescriptor = NULL;
	
	HANDLE StdOutReadPipe = NULL;
	HANDLE StdOutWritePipe = NULL;
	
	if (!CreatePipe(&StdOutReadPipe, &StdOutWritePipe, &SecurityAttributes, 0))
	{
		// Copy delegate to avoid use-after-free
		FOnClaudeResponse CompleteCopy = OnCompleteDelegate;
		AsyncTask(ENamedThreads::GameThread, [CompleteCopy]()
		{
			CompleteCopy.ExecuteIfBound(TEXT("Failed to create pipe for Claude process"), false);
		});
		return;
	}
	
	// Ensure read handle is not inherited
	SetHandleInformation(StdOutReadPipe, HANDLE_FLAG_INHERIT, 0);
	
	ReadPipe = StdOutReadPipe;
	WritePipe = StdOutWritePipe;
	
	// Setup process
	STARTUPINFOW StartupInfo;
	ZeroMemory(&StartupInfo, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.hStdError = StdOutWritePipe;
	StartupInfo.hStdOutput = StdOutWritePipe;
	StartupInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	StartupInfo.wShowWindow = SW_HIDE;
	
	PROCESS_INFORMATION ProcessInfo;
	ZeroMemory(&ProcessInfo, sizeof(ProcessInfo));
	
	// Build full command
	FString FullCommand = FString::Printf(TEXT("\"%s\" %s"), *ClaudePath, *CommandLine);
	
	// Create process
	BOOL bCreated = CreateProcessW(
		NULL,
		const_cast<LPWSTR>(*FullCommand),
		NULL,
		NULL,
		true,
		CREATE_NO_WINDOW,
		NULL,
		*WorkingDir,
		&StartupInfo,
		&ProcessInfo
	);
	
	if (!bCreated)
	{
		CloseHandle(StdOutReadPipe);
		CloseHandle(StdOutWritePipe);
		// Clear member variables to prevent double-close
		ReadPipe = nullptr;
		WritePipe = nullptr;

		// Copy delegate to avoid use-after-free
		FOnClaudeResponse CompleteCopy = OnCompleteDelegate;
		AsyncTask(ENamedThreads::GameThread, [CompleteCopy]()
		{
			CompleteCopy.ExecuteIfBound(TEXT("Failed to start Claude process"), false);
		});
		return;
	}
	
	ProcessHandle = ProcessInfo.hProcess;
	CloseHandle(ProcessInfo.hThread);
	CloseHandle(StdOutWritePipe);
	WritePipe = nullptr;
	
	// Read output
	FString FullOutput;
	char Buffer[4096];
	DWORD BytesRead;
	
	while (!StopTaskCounter.GetValue())
	{
		// Check if process is done
		DWORD WaitResult = WaitForSingleObject(ProcessInfo.hProcess, 100);
		
		// Read any available output
		while (ReadFile(StdOutReadPipe, Buffer, sizeof(Buffer) - 1, &BytesRead, NULL) && BytesRead > 0)
		{
			Buffer[BytesRead] = '\0';
			FString OutputChunk = UTF8_TO_TCHAR(Buffer);
			FullOutput += OutputChunk;
			
			// Report progress
			if (OnProgressDelegate.IsBound())
			{
				// Copy delegate and data to avoid use-after-free
				FOnClaudeProgress ProgressCopy = OnProgressDelegate;
				FString ProgressChunk = OutputChunk;
				AsyncTask(ENamedThreads::GameThread, [ProgressCopy, ProgressChunk]()
				{
					ProgressCopy.ExecuteIfBound(ProgressChunk);
				});
			}
		}
		
		if (WaitResult == WAIT_OBJECT_0)
		{
			// Process finished - read any remaining output
			while (ReadFile(StdOutReadPipe, Buffer, sizeof(Buffer) - 1, &BytesRead, NULL) && BytesRead > 0)
			{
				Buffer[BytesRead] = '\0';
				FullOutput += UTF8_TO_TCHAR(Buffer);
			}
			break;
		}
	}
	
	// Get exit code
	DWORD ExitCode;
	GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode);
	
	// Cleanup
	CloseHandle(StdOutReadPipe);
	CloseHandle(ProcessInfo.hProcess);
	ReadPipe = nullptr;
	ProcessHandle = nullptr;
	
	// Report completion on game thread
	// Copy delegate and data to avoid use-after-free
	bool bSuccess = (ExitCode == 0) && !StopTaskCounter.GetValue();
	FString FinalOutput = FullOutput;
	FOnClaudeResponse CompleteCopy = OnCompleteDelegate;

	AsyncTask(ENamedThreads::GameThread, [CompleteCopy, FinalOutput, bSuccess]()
	{
		CompleteCopy.ExecuteIfBound(FinalOutput, bSuccess);
	});
	
#endif // PLATFORM_WINDOWS
}

// FClaudeCodeSubsystem is now in ClaudeSubsystem.cpp
