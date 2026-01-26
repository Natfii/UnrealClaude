// Copyright Natali Caggiano. All Rights Reserved.

#include "ClaudeCodeRunner.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"
#include "ProjectContext.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Base64.h"
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
	, StdInReadPipe(nullptr)
	, StdInWritePipe(nullptr)
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
	if (StdInReadPipe)
	{
		CloseHandle((HANDLE)StdInReadPipe);
		StdInReadPipe = nullptr;
	}
	if (StdInWritePipe)
	{
		CloseHandle((HANDLE)StdInWritePipe);
		StdInWritePipe = nullptr;
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

	if (bHasSearched && !CachedClaudePath.IsEmpty())
	{
		// Only return cached path if it's valid
		return CachedClaudePath;
	}
	// Allow re-search if previous search failed (CachedClaudePath is empty)
	bHasSearched = true;

	// Check common locations for claude CLI
	TArray<FString> PossiblePaths;

	// User profile .local/bin (Claude Code native installer location)
	FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	if (!UserProfile.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT(".local"), TEXT("bin"), TEXT("claude.exe")));
	}

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

// Helper function to get a human-readable Windows error message
static FString GetWindowsErrorMessage(uint32 ErrorCode)
{
#if PLATFORM_WINDOWS
	LPWSTR MessageBuffer = nullptr;
	DWORD Size = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&MessageBuffer,
		0,
		NULL
	);

	FString Message;
	if (Size > 0 && MessageBuffer)
	{
		Message = FString(MessageBuffer);
		Message.TrimEndInline();
		LocalFree(MessageBuffer);
	}

	// Add common explanations for frequent errors
	switch (ErrorCode)
	{
	case 2:
		Message += TEXT(" (The executable was not found at the specified path)");
		break;
	case 3:
		Message += TEXT(" (The working directory does not exist)");
		break;
	case 5:
		Message += TEXT(" (Access denied - check permissions or antivirus)");
		break;
	case 87:
		Message += TEXT(" (Command line may be too long or malformed)");
		break;
	case 193:
		Message += TEXT(" (Not a valid Windows executable)");
		break;
	case 740:
		Message += TEXT(" (Requires elevation/admin rights)");
		break;
	}

	return Message;
#else
	return FString::Printf(TEXT("Error code %d"), ErrorCode);
#endif
}

// Helper function to escape command line arguments for direct CreateProcessW calls
// This does NOT escape for cmd.exe - only handles Windows argument parsing
static FString EscapeCommandLineArg(const FString& Arg)
{
	FString Escaped = Arg;

	// For CreateProcessW (not going through cmd.exe), we only need to:
	// 1. Escape backslashes that precede quotes
	// 2. Escape double quotes
	//
	// Shell metacharacters (&, |, <, >, ^, etc.) do NOT need escaping when
	// calling CreateProcessW directly - they're only special to cmd.exe

	// Count trailing backslashes - they need special handling
	// A backslash only needs escaping if it precedes a quote

	// Simple approach: escape all quotes with backslash
	// This works for most cases with CreateProcessW argument parsing
	Escaped = Escaped.Replace(TEXT("\\\""), TEXT("\\\\\""));  // \\" -> \\\\"
	Escaped = Escaped.Replace(TEXT("\""), TEXT("\\\""));       // " -> \"

	return Escaped;
}

// Get the plugin directory path
static FString GetPluginDirectory()
{
	// Try engine plugins directly (manual install location)
	FString EnginePluginPath = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("UnrealClaude"));
	if (FPaths::DirectoryExists(EnginePluginPath))
	{
		return EnginePluginPath;
	}

	// Try engine Marketplace plugins (Epic marketplace location)
	FString MarketplacePluginPath = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("Marketplace"), TEXT("UnrealClaude"));
	if (FPaths::DirectoryExists(MarketplacePluginPath))
	{
		return MarketplacePluginPath;
	}

	// Try project plugins
	FString ProjectPluginPath = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UnrealClaude"));
	if (FPaths::DirectoryExists(ProjectPluginPath))
	{
		return ProjectPluginPath;
	}

	UE_LOG(LogUnrealClaude, Warning, TEXT("Could not find UnrealClaude plugin directory. Checked: %s, %s, %s"),
		*EnginePluginPath, *MarketplacePluginPath, *ProjectPluginPath);
	return FString();
}

FString FClaudeCodeRunner::BuildCommandLine(const FClaudeRequestConfig& Config)
{
	FString CommandLine;

	// Print mode (non-interactive)
	CommandLine += TEXT("-p ");

	// Verbose mode to show thinking (required by stream-json output format)
	CommandLine += TEXT("--verbose ");

	// Skip permissions if requested
	if (Config.bSkipPermissions)
	{
		CommandLine += TEXT("--dangerously-skip-permissions ");
	}

	// JSON output if requested (but not when image attached -- stream-json overrides)
	if (Config.bUseJsonOutput && Config.AttachedImagePath.IsEmpty())
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
				TEXT("{\n  \"mcpServers\": {\n    \"unrealclaude\": {\n      \"command\": \"node\",\n      \"args\": [\"%s\"],\n      \"env\": {\n        \"UNREAL_MCP_URL\": \"http://localhost:%d\"\n      }\n    }\n  }\n}"),
				*MCPBridgePath.Replace(TEXT("\\"), TEXT("/")),
				UnrealClaudeConstants::MCPServer::DefaultPort
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

	// Use stream-json input/output format when image is attached (required for multimodal content)
	// Claude CLI requires both input and output to use stream-json together
	if (!Config.AttachedImagePath.IsEmpty())
	{
		CommandLine += TEXT("--input-format stream-json --output-format stream-json ");
	}

	// Write prompts to files to avoid command line length limits (Error 206)
	FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealClaude"));
	IFileManager::Get().MakeDirectory(*TempDir, true);

	// System prompt - write to file
	if (!Config.SystemPrompt.IsEmpty())
	{
		FString SystemPromptPath = FPaths::Combine(TempDir, TEXT("system-prompt.txt"));
		if (FFileHelper::SaveStringToFile(Config.SystemPrompt, *SystemPromptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			SystemPromptFilePath = SystemPromptPath;
			UE_LOG(LogUnrealClaude, Log, TEXT("System prompt written to: %s (%d chars)"), *SystemPromptPath, Config.SystemPrompt.Len());
		}
	}

	// User prompt - write to file
	FString PromptPath = FPaths::Combine(TempDir, TEXT("prompt.txt"));
	if (FFileHelper::SaveStringToFile(Config.Prompt, *PromptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		PromptFilePath = PromptPath;
		UE_LOG(LogUnrealClaude, Log, TEXT("Prompt written to: %s (%d chars)"), *PromptPath, Config.Prompt.Len());
	}

	// Don't add prompts to command line - we'll pipe them via stdin
	return CommandLine;
}

FString FClaudeCodeRunner::BuildStreamJsonPayload(const FString& TextPrompt, const FString& ImagePath)
{
	// Maximum image file size (bytes) before base64 encoding
	// Claude API accepts up to 5MB images; base64 adds ~33% overhead
	constexpr int64 MaxImageFileSize = 4608 * 1024; // 4.5 MB (Claude API limit is 5 MB)

	bool bImageValid = false;
	FString Base64ImageData;

	if (!ImagePath.IsEmpty())
	{
		FString ExpectedDir = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealClaude"), TEXT("screenshots")));
		FString FullImagePath = FPaths::ConvertRelativePathToFull(ImagePath);

		if (FullImagePath.Contains(TEXT("..")))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Rejecting image path with traversal: %s"), *FullImagePath);
		}
		else if (!FullImagePath.StartsWith(ExpectedDir))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Rejecting image path outside screenshots directory: %s"), *FullImagePath);
		}
		else if (!FPaths::FileExists(FullImagePath))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Attached image file no longer exists: %s"), *FullImagePath);
		}
		else
		{
			// Check file size before loading
			const int64 FileSize = IFileManager::Get().FileSize(*FullImagePath);
			if (FileSize > MaxImageFileSize)
			{
				UE_LOG(LogUnrealClaude, Warning, TEXT("Image file too large for base64 encoding: %s (%lld bytes, max %lld)"),
					*FullImagePath, FileSize, MaxImageFileSize);
			}
			else
			{
				// Load and base64 encode the PNG
				TArray<uint8> ImageData;
				if (FFileHelper::LoadFileToArray(ImageData, *FullImagePath))
				{
					Base64ImageData = FBase64::Encode(ImageData);
					bImageValid = true;
					UE_LOG(LogUnrealClaude, Log, TEXT("Base64 encoded image: %s (%d bytes -> %d chars)"),
						*FullImagePath, ImageData.Num(), Base64ImageData.Len());
				}
				else
				{
					UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to load image file for base64 encoding: %s"), *FullImagePath);
				}
			}
		}
	}

	// Build content blocks array
	TArray<TSharedPtr<FJsonValue>> ContentArray;

	// Text content block (system context + user message)
	if (!TextPrompt.IsEmpty())
	{
		TSharedPtr<FJsonObject> TextBlock = MakeShared<FJsonObject>();
		TextBlock->SetStringField(TEXT("type"), TEXT("text"));
		TextBlock->SetStringField(TEXT("text"), TextPrompt);
		ContentArray.Add(MakeShared<FJsonValueObject>(TextBlock));
	}

	// Image content block (base64-encoded PNG)
	if (bImageValid)
	{
		TSharedPtr<FJsonObject> Source = MakeShared<FJsonObject>();
		Source->SetStringField(TEXT("type"), TEXT("base64"));
		Source->SetStringField(TEXT("media_type"), TEXT("image/png"));
		Source->SetStringField(TEXT("data"), Base64ImageData);

		TSharedPtr<FJsonObject> ImageBlock = MakeShared<FJsonObject>();
		ImageBlock->SetStringField(TEXT("type"), TEXT("image"));
		ImageBlock->SetObjectField(TEXT("source"), Source);
		ContentArray.Add(MakeShared<FJsonValueObject>(ImageBlock));
	}

	// Build the inner message object: {"role":"user","content":[...]}
	TSharedPtr<FJsonObject> Message = MakeShared<FJsonObject>();
	Message->SetStringField(TEXT("role"), TEXT("user"));
	Message->SetArrayField(TEXT("content"), ContentArray);

	// Build the outer SDKUserMessage envelope: {"type":"user","message":{...}}
	TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
	Envelope->SetStringField(TEXT("type"), TEXT("user"));
	Envelope->SetObjectField(TEXT("message"), Message);

	// Serialize to condensed JSON (single line for NDJSON)
	FString JsonLine;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonLine);
	FJsonSerializer::Serialize(Envelope.ToSharedRef(), Writer);
	Writer->Close();

	// NDJSON requires newline termination
	JsonLine += TEXT("\n");

	UE_LOG(LogUnrealClaude, Log, TEXT("Built stream-json payload: %d chars (image: %s)"),
		JsonLine.Len(), bImageValid ? TEXT("yes") : TEXT("no"));

	return JsonLine;
}

FString FClaudeCodeRunner::ParseStreamJsonOutput(const FString& RawOutput)
{
	// Stream-json output is NDJSON: one JSON object per line
	// We look for the "result" message which contains the final response text
	// Format: {"type":"result","result":"the text response",...}
	// Fallback: accumulate text from "assistant" content blocks

	TArray<FString> Lines;
	RawOutput.ParseIntoArrayLines(Lines);

	// First pass: look for the "result" message
	for (const FString& Line : Lines)
	{
		if (Line.IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
		if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
		{
			continue;
		}

		FString Type;
		if (!JsonObj->TryGetStringField(TEXT("type"), Type))
		{
			continue;
		}

		if (Type == TEXT("result"))
		{
			FString ResultText;
			if (JsonObj->TryGetStringField(TEXT("result"), ResultText))
			{
				UE_LOG(LogUnrealClaude, Log, TEXT("Parsed stream-json result: %d chars"), ResultText.Len());
				return ResultText;
			}
		}
	}

	// Fallback: accumulate text from assistant content blocks
	FString AccumulatedText;
	for (const FString& Line : Lines)
	{
		if (Line.IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
		if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
		{
			continue;
		}

		FString Type;
		if (!JsonObj->TryGetStringField(TEXT("type"), Type))
		{
			continue;
		}

		if (Type == TEXT("assistant"))
		{
			const TSharedPtr<FJsonObject>* MessageObj;
			if (JsonObj->TryGetObjectField(TEXT("message"), MessageObj))
			{
				const TArray<TSharedPtr<FJsonValue>>* ContentArray;
				if ((*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray))
				{
					for (const TSharedPtr<FJsonValue>& ContentValue : *ContentArray)
					{
						const TSharedPtr<FJsonObject>* ContentObj;
						if (ContentValue->TryGetObject(ContentObj))
						{
							FString ContentType;
							if ((*ContentObj)->TryGetStringField(TEXT("type"), ContentType) && ContentType == TEXT("text"))
							{
								FString Text;
								if ((*ContentObj)->TryGetStringField(TEXT("text"), Text))
								{
									AccumulatedText += Text;
								}
							}
						}
					}
				}
			}
		}
	}

	if (!AccumulatedText.IsEmpty())
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("Parsed stream-json from assistant blocks: %d chars"), AccumulatedText.Len());
		return AccumulatedText;
	}

	// Last resort: return a user-friendly error instead of raw NDJSON
	UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to parse stream-json output (%d chars). Raw output logged below:"), RawOutput.Len());
	UE_LOG(LogUnrealClaude, Warning, TEXT("%s"), *RawOutput.Left(2000));
	return TEXT("Error: Failed to parse Claude's response. Check the Output Log for details.");
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

#if PLATFORM_WINDOWS
bool FClaudeCodeRunner::CreateProcessPipes()
{
	SECURITY_ATTRIBUTES SecurityAttributes;
	SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	SecurityAttributes.bInheritHandle = true;
	SecurityAttributes.lpSecurityDescriptor = NULL;

	// Create stdout pipe
	HANDLE StdOutReadPipe = NULL;
	HANDLE StdOutWritePipe = NULL;

	if (!CreatePipe(&StdOutReadPipe, &StdOutWritePipe, &SecurityAttributes, 0))
	{
		return false;
	}

	// Ensure read handle is not inherited
	SetHandleInformation(StdOutReadPipe, HANDLE_FLAG_INHERIT, 0);

	ReadPipe = StdOutReadPipe;
	WritePipe = StdOutWritePipe;

	// Create stdin pipe
	HANDLE StdInReadPipeHandle = NULL;
	HANDLE StdInWritePipeHandle = NULL;

	if (!CreatePipe(&StdInReadPipeHandle, &StdInWritePipeHandle, &SecurityAttributes, 0))
	{
		CloseHandle(StdOutReadPipe);
		CloseHandle(StdOutWritePipe);
		ReadPipe = nullptr;
		WritePipe = nullptr;
		return false;
	}

	// Ensure write handle is not inherited (we write to it, child reads from read end)
	SetHandleInformation(StdInWritePipeHandle, HANDLE_FLAG_INHERIT, 0);

	StdInReadPipe = StdInReadPipeHandle;
	StdInWritePipe = StdInWritePipeHandle;

	return true;
}

bool FClaudeCodeRunner::LaunchProcess(const FString& FullCommand, const FString& WorkingDir)
{
	HANDLE StdOutWritePipe = static_cast<HANDLE>(WritePipe);
	HANDLE StdInReadPipeHandle = static_cast<HANDLE>(StdInReadPipe);

	STARTUPINFOW StartupInfo;
	ZeroMemory(&StartupInfo, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.hStdInput = StdInReadPipeHandle;
	StartupInfo.hStdError = StdOutWritePipe;
	StartupInfo.hStdOutput = StdOutWritePipe;
	StartupInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	StartupInfo.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION ProcessInfo;
	ZeroMemory(&ProcessInfo, sizeof(ProcessInfo));

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
		LastProcessError = GetLastError();
		UE_LOG(LogUnrealClaude, Error, TEXT("CreateProcessW failed with error code %d"), LastProcessError);
		UE_LOG(LogUnrealClaude, Error, TEXT("Command: %s"), *FullCommand);
		UE_LOG(LogUnrealClaude, Error, TEXT("Working directory: %s"), *WorkingDir);
		return false;
	}

	LastProcessError = 0;
	ProcessHandle = ProcessInfo.hProcess;
	CloseHandle(ProcessInfo.hThread);

	// Close the stdout write pipe (child has it now)
	CloseHandle(StdOutWritePipe);
	WritePipe = nullptr;

	// Close the stdin read pipe (child has it now)
	CloseHandle(StdInReadPipeHandle);
	StdInReadPipe = nullptr;

	return true;
}

FString FClaudeCodeRunner::ReadProcessOutput()
{
	FString FullOutput;
	char Buffer[4096];
	DWORD BytesRead;

	HANDLE StdOutReadPipe = static_cast<HANDLE>(ReadPipe);
	HANDLE hProcess = static_cast<HANDLE>(ProcessHandle);

	// When using stream-json (image attachment), suppress progress callbacks since
	// the raw output is NDJSON that must be parsed before display
	bool bSuppressProgress = !CurrentConfig.AttachedImagePath.IsEmpty();

	while (!StopTaskCounter.GetValue())
	{
		// Check if process is done
		DWORD WaitResult = WaitForSingleObject(hProcess, UnrealClaudeConstants::Process::WaitTimeoutMs);

		// Read any available output
		while (ReadFile(StdOutReadPipe, Buffer, sizeof(Buffer) - 1, &BytesRead, NULL) && BytesRead > 0)
		{
			Buffer[BytesRead] = '\0';
			FString OutputChunk = UTF8_TO_TCHAR(Buffer);
			FullOutput += OutputChunk;

			// Report progress (skip for stream-json mode to avoid displaying raw NDJSON)
			if (!bSuppressProgress && OnProgressDelegate.IsBound())
			{
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

	return FullOutput;
}

void FClaudeCodeRunner::ReportError(const FString& ErrorMessage)
{
	FOnClaudeResponse CompleteCopy = OnCompleteDelegate;
	FString Message = ErrorMessage;
	AsyncTask(ENamedThreads::GameThread, [CompleteCopy, Message]()
	{
		CompleteCopy.ExecuteIfBound(Message, false);
	});
}

void FClaudeCodeRunner::ReportCompletion(const FString& Output, bool bSuccess)
{
	FOnClaudeResponse CompleteCopy = OnCompleteDelegate;
	FString FinalOutput = Output;
	AsyncTask(ENamedThreads::GameThread, [CompleteCopy, FinalOutput, bSuccess]()
	{
		CompleteCopy.ExecuteIfBound(FinalOutput, bSuccess);
	});
}
#endif // PLATFORM_WINDOWS

void FClaudeCodeRunner::ExecuteProcess()
{
#if PLATFORM_WINDOWS
	FString ClaudePath = GetClaudePath();

	// Verify the path exists
	if (ClaudePath.IsEmpty())
	{
		ReportError(TEXT("Claude CLI not found. Please install with: npm install -g @anthropic-ai/claude-code"));
		return;
	}

	if (!IFileManager::Get().FileExists(*ClaudePath))
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Claude path no longer exists: %s"), *ClaudePath);
		ReportError(FString::Printf(TEXT("Claude CLI path invalid: %s"), *ClaudePath));
		return;
	}

	FString CommandLine = BuildCommandLine(CurrentConfig);

	UE_LOG(LogUnrealClaude, Log, TEXT("Async executing Claude: %s %s"), *ClaudePath, *CommandLine);

	// Set working directory
	FString WorkingDir = CurrentConfig.WorkingDirectory;
	if (WorkingDir.IsEmpty())
	{
		WorkingDir = FPaths::ProjectDir();
	}

	// Create pipes for stdout capture
	if (!CreateProcessPipes())
	{
		ReportError(TEXT("Failed to create pipe for Claude process"));
		return;
	}

	// Build and launch the process
	FString FullCommand = FString::Printf(TEXT("\"%s\" %s"), *ClaudePath, *CommandLine);
	UE_LOG(LogUnrealClaude, Log, TEXT("Full command: %s"), *FullCommand);
	UE_LOG(LogUnrealClaude, Log, TEXT("Working directory: %s"), *WorkingDir);

	if (!LaunchProcess(FullCommand, WorkingDir))
	{
		CloseHandle(static_cast<HANDLE>(ReadPipe));
		CloseHandle(static_cast<HANDLE>(WritePipe));
		CloseHandle(static_cast<HANDLE>(StdInReadPipe));
		CloseHandle(static_cast<HANDLE>(StdInWritePipe));
		ReadPipe = nullptr;
		WritePipe = nullptr;
		StdInReadPipe = nullptr;
		StdInWritePipe = nullptr;

		// Build detailed error message for chat display
		FString ErrorMsg = FString::Printf(
			TEXT("Failed to start Claude process.\n\n")
			TEXT("Windows Error %d: %s\n\n")
			TEXT("Claude Path: %s\n")
			TEXT("Working Dir: %s\n\n")
			TEXT("Command (truncated): %.200s..."),
			LastProcessError,
			*GetWindowsErrorMessage(LastProcessError),
			*ClaudePath,
			*WorkingDir,
			*FullCommand
		);
		ReportError(ErrorMsg);
		return;
	}

	// Write prompt to stdin (Claude -p reads from stdin if no prompt on command line)
	HANDLE StdInWrite = static_cast<HANDLE>(StdInWritePipe);
	if (StdInWrite)
	{
		FString StdinPayload;
		bool bHasImageAttachment = !CurrentConfig.AttachedImagePath.IsEmpty();

		// Build the text portion of the prompt (system context + user message)
		FString TextPrompt;
		if (!SystemPromptFilePath.IsEmpty())
		{
			FString SystemPromptContent;
			if (FFileHelper::LoadFileToString(SystemPromptContent, *SystemPromptFilePath))
			{
				TextPrompt = FString::Printf(TEXT("[CONTEXT]\n%s\n[/CONTEXT]\n\n"), *SystemPromptContent);
			}
		}
		if (!PromptFilePath.IsEmpty())
		{
			FString PromptContent;
			if (FFileHelper::LoadFileToString(PromptContent, *PromptFilePath))
			{
				TextPrompt += PromptContent;
			}
		}

		if (bHasImageAttachment)
		{
			// Stream-json mode: build NDJSON with multimodal content blocks (text + base64 image)
			StdinPayload = BuildStreamJsonPayload(TextPrompt, CurrentConfig.AttachedImagePath);
		}
		else
		{
			// Plain text mode: pipe the text directly
			StdinPayload = TextPrompt;
		}

		// Write to stdin
		if (!StdinPayload.IsEmpty())
		{
			FTCHARToUTF8 Utf8Payload(*StdinPayload);
			DWORD BytesWritten;
			WriteFile(StdInWrite, Utf8Payload.Get(), Utf8Payload.Length(), &BytesWritten, NULL);
			UE_LOG(LogUnrealClaude, Log, TEXT("Wrote %d bytes to Claude stdin (format: %s, system: %d chars, user: %d chars)"),
				BytesWritten,
				bHasImageAttachment ? TEXT("stream-json") : TEXT("text"),
				CurrentConfig.SystemPrompt.Len(), CurrentConfig.Prompt.Len());
		}

		// Close stdin write pipe to signal EOF to Claude
		CloseHandle(StdInWrite);
		StdInWritePipe = nullptr;
	}

	// Clear temp file paths
	SystemPromptFilePath.Empty();
	PromptFilePath.Empty();

	// Read output until process completes
	FString FullOutput = ReadProcessOutput();

	// If stream-json output was used (image attachment), parse NDJSON to extract text
	if (!CurrentConfig.AttachedImagePath.IsEmpty())
	{
		FullOutput = ParseStreamJsonOutput(FullOutput);
	}

	// Get exit code
	DWORD ExitCode;
	GetExitCodeProcess(static_cast<HANDLE>(ProcessHandle), &ExitCode);

	// Cleanup handles
	CloseHandle(static_cast<HANDLE>(ReadPipe));
	CloseHandle(static_cast<HANDLE>(ProcessHandle));
	ReadPipe = nullptr;
	ProcessHandle = nullptr;

	// Report completion
	bool bSuccess = (ExitCode == 0) && !StopTaskCounter.GetValue();
	ReportCompletion(FullOutput, bSuccess);

#endif // PLATFORM_WINDOWS
}

// FClaudeCodeSubsystem is now in ClaudeSubsystem.cpp
