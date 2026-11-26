// Copyright Natali Caggiano. All Rights Reserved.

#include "ClaudeCodeRunner.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"
#include "ProjectContext.h"
#include "MCP/UnrealClaudeMCPServer.h"
#include "MCP/MCPToolRegistry.h"
#include "MCP/MCPTaskQueue.h"
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

FClaudeCodeRunner::FClaudeCodeRunner()
	: Thread(nullptr)
	, bIsExecuting(false)
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
	if (ReadPipe || WritePipe)
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = nullptr;
		WritePipe = nullptr;
	}
	if (StdInReadPipe || StdInWritePipe)
	{
		FPlatformProcess::ClosePipe(StdInReadPipe, StdInWritePipe);
		StdInReadPipe = nullptr;
		StdInWritePipe = nullptr;
	}
	if (ProcessHandle.IsValid())
	{
		FPlatformProcess::CloseProc(ProcessHandle);
	}
}

bool FClaudeCodeRunner::IsClaudeAvailable()
{
	FString ClaudePath = GetClaudePath();
	return !ClaudePath.IsEmpty();
}

FString FClaudeCodeRunner::GetClaudePath()
{
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

	TArray<FString> PossiblePaths;
	TArray<FString> CheckedPaths; // For better logging when nothing is found

#if PLATFORM_WINDOWS
	// User profile .local/bin (Claude Code native installer location)
	FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	if (!UserProfile.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT(".local"), TEXT("bin"), TEXT("claude.exe")));
	}

	// npm global install locations
	FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	if (!AppData.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(AppData, TEXT("npm"), TEXT("claude.cmd")));
	}

	FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
	if (!LocalAppData.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(LocalAppData, TEXT("npm"), TEXT("claude.cmd")));
		// Common yarn / pnpm global locations on Windows
		PossiblePaths.Add(FPaths::Combine(LocalAppData, TEXT("Yarn"), TEXT("bin"), TEXT("claude.cmd")));
		PossiblePaths.Add(FPaths::Combine(LocalAppData, TEXT("pnpm"), TEXT("claude.cmd")));
	}

	// User profile npm (another common spot)
	if (!UserProfile.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT("AppData"), TEXT("Roaming"), TEXT("npm"), TEXT("claude.cmd")));
	}

	// Scan every PATH directory for claude.cmd or claude.exe as a last resort
	FString PathEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	TArray<FString> PathDirs;
	PathEnv.ParseIntoArray(PathDirs, TEXT(";"), true);

	for (const FString& Dir : PathDirs)
	{
		if (!Dir.IsEmpty())
		{
			PossiblePaths.Add(FPaths::Combine(Dir, TEXT("claude.cmd")));
			PossiblePaths.Add(FPaths::Combine(Dir, TEXT("claude.exe")));
		}
	}
#else
	// Linux / macOS
	FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
	if (!Home.IsEmpty())
	{
		// Native installer location
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".local"), TEXT("bin"), TEXT("claude")));
		// Common npm global prefix
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".npm-global"), TEXT("bin"), TEXT("claude")));
		// Yarn global
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".yarn"), TEXT("bin"), TEXT("claude")));
		// pnpm global
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".local"), TEXT("share"), TEXT("pnpm"), TEXT("claude")));
	}

	// Common system and package manager paths
	PossiblePaths.Add(TEXT("/usr/local/bin/claude"));
	PossiblePaths.Add(TEXT("/usr/bin/claude"));
	// Homebrew on Apple Silicon
	PossiblePaths.Add(TEXT("/opt/homebrew/bin/claude"));

	// Scan every PATH directory as a last resort
	FString PathEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	TArray<FString> PathDirs;
	PathEnv.ParseIntoArray(PathDirs, TEXT(":"), true);

	for (const FString& Dir : PathDirs)
	{
		if (!Dir.IsEmpty())
		{
			PossiblePaths.Add(FPaths::Combine(Dir, TEXT("claude")));
		}
	}
#endif

	// Check all the paths we collected
	for (const FString& Path : PossiblePaths)
	{
		CheckedPaths.Add(Path);
		if (IFileManager::Get().FileExists(*Path))
		{
			UE_LOG(LogUnrealClaude, Log, TEXT("Found Claude CLI at: %s"), *Path);
			CachedClaudePath = Path;
			return CachedClaudePath;
		}
	}

	// Fallback: use where (Windows) or which (Linux/macOS)
	FString WhereOutput;
	FString WhereErrors;
	int32 ReturnCode;

#if PLATFORM_WINDOWS
	const TCHAR* WhichCmd = TEXT("where");
	const TCHAR* WhichArgs = TEXT("claude");
#else
	const TCHAR* WhichCmd = TEXT("/bin/sh");
	const TCHAR* WhichArgs = TEXT("-c 'which claude 2>/dev/null'");
#endif

	if (FPlatformProcess::ExecProcess(WhichCmd, WhichArgs, &ReturnCode, &WhereOutput, &WhereErrors) && ReturnCode == 0)
	{
		WhereOutput.TrimStartAndEndInline();
		TArray<FString> Lines;
		WhereOutput.ParseIntoArrayLines(Lines);

		for (const FString& Line : Lines)
		{
			FString CleanLine = Line.TrimStartAndEnd();
			if (!CleanLine.IsEmpty() && IFileManager::Get().FileExists(*CleanLine))
			{
				UE_LOG(LogUnrealClaude, Log, TEXT("Found Claude CLI via '%s': %s"), WhichCmd, *CleanLine);
				CachedClaudePath = CleanLine;
				return CachedClaudePath;
			}
		}
	}

	// Nothing found – log the paths we checked so the user can see what happened
	UE_LOG(LogUnrealClaude, Warning, TEXT("Claude CLI not found after checking %d locations."), CheckedPaths.Num());
	for (const FString& Checked : CheckedPaths)
	{
		UE_LOG(LogUnrealClaude, Verbose, TEXT("  Checked: %s"), *Checked);
	}

	UE_LOG(LogUnrealClaude, Warning, TEXT("Install Claude Code with: npm install -g @anthropic-ai/claude-code"));
	UE_LOG(LogUnrealClaude, Warning, TEXT("Then restart the editor. You can also set the full path in your system PATH if it is installed in a custom location."));

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
		OnComplete.ExecuteIfBound(TEXT("Claude CLI not found. Please install with: npm install -g @anthropic-ai/claude-code then restart the editor."), false);
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
		OutResponse = TEXT("Claude CLI not found. Please install with: npm install -g @anthropic-ai/claude-code then restart the editor.");
		return false;
	}

	FString ClaudePath = GetClaudePath();
	FString CommandLine = BuildCommandLine(Config);

	UE_LOG(LogUnrealClaude, Log, TEXT("Executing Claude: %s %s"), *ClaudePath, *CommandLine);

	FString StdOut;
	FString StdErr;
	int32 ReturnCode;

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

	// Always use stream-json for structured NDJSON output
	// This enables real-time parsing of text, tool_use, and tool_result events
	CommandLine += TEXT("--output-format stream-json ");
	CommandLine += TEXT("--input-format stream-json ");

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
			TSharedPtr<FUnrealClaudeMCPServer> MCPServer = FUnrealClaudeModule::Get().GetMCPServer();
			const FString AuthToken = MCPServer.IsValid() ? MCPServer->GetAuthToken() : FString();

			// Build JSON via FJsonObject so paths containing quotes or backslashes
			// cannot break the document (unlike FString::Printf).
			TSharedPtr<FJsonObject> EnvObj = MakeShared<FJsonObject>();
			EnvObj->SetStringField(TEXT("UNREAL_MCP_URL"),
				FString::Printf(TEXT("http://localhost:%d"), FUnrealClaudeModule::GetMCPServerPort()));
			EnvObj->SetStringField(TEXT("UNREAL_MCP_TOKEN"), AuthToken);

			TArray<TSharedPtr<FJsonValue>> ArgsArray;
			ArgsArray.Add(MakeShared<FJsonValueString>(MCPBridgePath.Replace(TEXT("\\"), TEXT("/"))));

			TSharedPtr<FJsonObject> ServerObj = MakeShared<FJsonObject>();
			ServerObj->SetStringField(TEXT("command"), TEXT("node"));
			ServerObj->SetArrayField(TEXT("args"), ArgsArray);
			ServerObj->SetObjectField(TEXT("env"), EnvObj);

			TSharedPtr<FJsonObject> ServersObj = MakeShared<FJsonObject>();
			ServersObj->SetObjectField(TEXT("unrealclaude"), ServerObj);

			TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
			RootObj->SetObjectField(TEXT("mcpServers"), ServersObj);

			FString MCPConfigContent;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MCPConfigContent);
			FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);

			if (FFileHelper::SaveStringToFile(MCPConfigContent, *MCPConfigPath))
			{
				// Convert to absolute path - FPaths::ProjectSavedDir() returns a relative path on macOS
				// which UE resolves internally, but external processes (Claude CLI) need the full path
				FString AbsConfigPath = FPaths::ConvertRelativePathToFull(MCPConfigPath);
				FString EscapedConfigPath = AbsConfigPath.Replace(TEXT("\\"), TEXT("/"));
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

	// Write prompts to unique per-request files to avoid races under concurrent sessions.
	// (Fixed names like prompt.txt / system-prompt.txt would collide.)
	FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealClaude"), TEXT("requests"));
	IFileManager::Get().MakeDirectory(*TempDir, true);
	const FString RequestId = FGuid::NewGuid().ToString(EGuidFormats::Digits);

	// System prompt - write to file
	if (!Config.SystemPrompt.IsEmpty())
	{
		FString SystemPromptPath = FPaths::Combine(TempDir, FString::Printf(TEXT("system-prompt-%s.txt"), *RequestId));
		if (FFileHelper::SaveStringToFile(Config.SystemPrompt, *SystemPromptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			SystemPromptFilePath = SystemPromptPath;
			UE_LOG(LogUnrealClaude, Log, TEXT("System prompt written to: %s (%d chars)"), *SystemPromptPath, Config.SystemPrompt.Len());
		}
	}

	// User prompt - write to file
	FString PromptPath = FPaths::Combine(TempDir, FString::Printf(TEXT("prompt-%s.txt"), *RequestId));
	if (FFileHelper::SaveStringToFile(Config.Prompt, *PromptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		PromptFilePath = PromptPath;
		UE_LOG(LogUnrealClaude, Log, TEXT("Prompt written to: %s (%d chars)"), *PromptPath, Config.Prompt.Len());
	}

	// Don't add prompts to command line - we'll pipe them via stdin
	return CommandLine;
}

FString FClaudeCodeRunner::BuildStreamJsonPayload(const FString& TextPrompt, const TArray<FString>& ImagePaths)
{
	using namespace UnrealClaudeConstants::ClipboardImage;

	// Pre-compute expected directory once for all images
	FString ExpectedDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealClaude"), TEXT("screenshots")));

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

	// Image content blocks (base64-encoded PNGs)
	int32 EncodedCount = 0;
	int64 TotalImageBytes = 0;
	const int32 MaxCount = FMath::Min(ImagePaths.Num(), MaxImagesPerMessage);

	for (int32 i = 0; i < MaxCount; ++i)
	{
		const FString& ImagePath = ImagePaths[i];
		if (ImagePath.IsEmpty())
		{
			continue;
		}

		FString FullImagePath = FPaths::ConvertRelativePathToFull(ImagePath);

		if (FullImagePath.Contains(TEXT("..")))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Rejecting image path with traversal: %s"), *FullImagePath);
			continue;
		}
		// Compare directory boundaries, not a raw string prefix.
		// A sibling such as "screenshots_evil" must not pass this check.
		FString ExpectedDirWithSeparator = ExpectedDir;
		if (!ExpectedDirWithSeparator.EndsWith(TEXT("/")) && !ExpectedDirWithSeparator.EndsWith(TEXT("\\")))
		{
			ExpectedDirWithSeparator += TEXT("/");
		}
		if (!FullImagePath.StartsWith(ExpectedDirWithSeparator, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Rejecting image path outside screenshots directory: %s"), *FullImagePath);
			continue;
		}
		if (!FPaths::FileExists(FullImagePath))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Attached image file no longer exists: %s"), *FullImagePath);
			continue;
		}

		const int64 FileSize = IFileManager::Get().FileSize(*FullImagePath);
		if (FileSize > MaxImageFileSize)
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Image file too large for base64 encoding: %s (%lld bytes, max %lld)"),
				*FullImagePath, FileSize, MaxImageFileSize);
			continue;
		}

		if (TotalImageBytes + FileSize > MaxTotalImagePayloadSize)
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Skipping image (total payload would exceed %lld bytes): %s"),
				MaxTotalImagePayloadSize, *FullImagePath);
			continue;
		}

		TArray<uint8> ImageData;
		if (!FFileHelper::LoadFileToArray(ImageData, *FullImagePath))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to load image file for base64 encoding: %s"), *FullImagePath);
			continue;
		}

		FString Base64ImageData = FBase64::Encode(ImageData);
		TotalImageBytes += FileSize;

		TSharedPtr<FJsonObject> Source = MakeShared<FJsonObject>();
		Source->SetStringField(TEXT("type"), TEXT("base64"));
		Source->SetStringField(TEXT("media_type"), TEXT("image/png"));
		Source->SetStringField(TEXT("data"), Base64ImageData);

		TSharedPtr<FJsonObject> ImageBlock = MakeShared<FJsonObject>();
		ImageBlock->SetStringField(TEXT("type"), TEXT("image"));
		ImageBlock->SetObjectField(TEXT("source"), Source);
		ContentArray.Add(MakeShared<FJsonValueObject>(ImageBlock));

		EncodedCount++;
		UE_LOG(LogUnrealClaude, Log, TEXT("Base64 encoded image [%d]: %s (%d bytes -> %d chars)"),
			EncodedCount, *FullImagePath, ImageData.Num(), Base64ImageData.Len());
	}

	if (EncodedCount > 0)
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("Encoded %d image(s), total %lld bytes"), EncodedCount, TotalImageBytes);
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

	UE_LOG(LogUnrealClaude, Log, TEXT("Built stream-json payload: %d chars (images: %d)"),
		JsonLine.Len(), EncodedCount);

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

	// First pass: check for refusal (must take priority) and look for result message
	FString FoundResultText;
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

		// Check for refusal in assistant messages (stop_reason in message object)
		if (Type == TEXT("assistant"))
		{
			const TSharedPtr<FJsonObject>* MessageObj;
			if (JsonObj->TryGetObjectField(TEXT("message"), MessageObj))
			{
				FString StopReason;
				if ((*MessageObj)->TryGetStringField(TEXT("stop_reason"), StopReason) && StopReason == TEXT("refusal"))
				{
					UE_LOG(LogUnrealClaude, Warning, TEXT("ParseStreamJsonOutput: Detected refusal in assistant message"));
					return TEXT("Error: Response refused by content safety filter. Please rephrase your request.");
				}
			}
		}

		// Check for refusal in result events (stop_reason at top level)
		if (Type == TEXT("result"))
		{
			FString StopReason;
			if (JsonObj->TryGetStringField(TEXT("stop_reason"), StopReason) && StopReason == TEXT("refusal"))
			{
				UE_LOG(LogUnrealClaude, Warning, TEXT("ParseStreamJsonOutput: Detected refusal in result event"));
				return TEXT("Error: Response refused by content safety filter. Please rephrase your request.");
			}

			// Not a refusal — save the result text for later
			JsonObj->TryGetStringField(TEXT("result"), FoundResultText);
		}
	}

	// Return the result text if found (no refusal detected)
	if (!FoundResultText.IsEmpty())
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("Parsed stream-json result: %d chars"), FoundResultText.Len());
		return FoundResultText;
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

void FClaudeCodeRunner::ParseAndEmitNdjsonLine(const FString& JsonLine)
{
	if (JsonLine.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogUnrealClaude, Verbose, TEXT("NDJSON: Non-JSON line (skipping): %.200s"), *JsonLine);
		return;
	}

	FString Type;
	if (!JsonObj->TryGetStringField(TEXT("type"), Type))
	{
		UE_LOG(LogUnrealClaude, Verbose, TEXT("NDJSON: Line missing 'type' field"));
		return;
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("NDJSON Event: type=%s"), *Type);

	if (Type == TEXT("system"))
	{
		// Session init event
		FString Subtype;
		JsonObj->TryGetStringField(TEXT("subtype"), Subtype);
		FString SessionId;
		JsonObj->TryGetStringField(TEXT("session_id"), SessionId);

		UE_LOG(LogUnrealClaude, Log, TEXT("NDJSON System: subtype=%s, session_id=%s"), *Subtype, *SessionId);

		if (CurrentConfig.OnStreamEvent.IsBound())
		{
			FClaudeStreamEvent Event;
			Event.Type = EClaudeStreamEventType::SessionInit;
			Event.SessionId = SessionId;
			Event.RawJson = JsonLine;
			FOnClaudeStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
			AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
			{
				EventDelegate.ExecuteIfBound(Event);
			});
		}
	}
	else if (Type == TEXT("assistant"))
	{
		// Assistant message with content blocks
		const TSharedPtr<FJsonObject>* MessageObj;
		if (!JsonObj->TryGetObjectField(TEXT("message"), MessageObj))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("NDJSON: assistant message missing 'message' field"));
			return;
		}

		// Check for stop_reason: "refusal" - streaming classifiers intervened
		FString StopReason;
		(*MessageObj)->TryGetStringField(TEXT("stop_reason"), StopReason);
		if (StopReason == TEXT("refusal"))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("NDJSON: Streaming refusal detected (stop_reason=refusal)"));

			bRefusalDetected.Store(true);
			AccumulatedResponseText.Empty();

			if (CurrentConfig.OnStreamEvent.IsBound())
			{
				FClaudeStreamEvent Event;
				Event.Type = EClaudeStreamEventType::Refusal;
				Event.StopReason = StopReason;
				Event.bIsError = true;
				Event.RawJson = JsonLine;
				FOnClaudeStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
				AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
				{
					EventDelegate.ExecuteIfBound(Event);
				});
			}

			// Do not parse content blocks from a refused message
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* ContentArray;
		if (!(*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("NDJSON: assistant message.content missing"));
			return;
		}

		for (const TSharedPtr<FJsonValue>& ContentValue : *ContentArray)
		{
			const TSharedPtr<FJsonObject>* ContentObj;
			if (!ContentValue->TryGetObject(ContentObj))
			{
				continue;
			}

			FString ContentType;
			if (!(*ContentObj)->TryGetStringField(TEXT("type"), ContentType))
			{
				continue;
			}

			if (ContentType == TEXT("text"))
			{
				FString Text;
				if ((*ContentObj)->TryGetStringField(TEXT("text"), Text))
				{
					UE_LOG(LogUnrealClaude, Log, TEXT("NDJSON TextContent: %d chars"), Text.Len());
					AccumulatedResponseText += Text;

					// Fire old progress delegate for backward compat
					if (OnProgressDelegate.IsBound())
					{
						FOnClaudeProgress ProgressCopy = OnProgressDelegate;
						AsyncTask(ENamedThreads::GameThread, [ProgressCopy, Text]()
						{
							ProgressCopy.ExecuteIfBound(Text);
						});
					}

					// Fire new structured event
					if (CurrentConfig.OnStreamEvent.IsBound())
					{
						FClaudeStreamEvent Event;
						Event.Type = EClaudeStreamEventType::TextContent;
						Event.Text = Text;
						FOnClaudeStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
						AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
						{
							EventDelegate.ExecuteIfBound(Event);
						});
					}
				}
			}
			else if (ContentType == TEXT("tool_use"))
			{
				FString ToolName, ToolId;
				(*ContentObj)->TryGetStringField(TEXT("name"), ToolName);
				(*ContentObj)->TryGetStringField(TEXT("id"), ToolId);

				// Serialize input to string
				FString ToolInputStr;
				const TSharedPtr<FJsonObject>* InputObj;
				if ((*ContentObj)->TryGetObjectField(TEXT("input"), InputObj))
				{
					TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
						TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ToolInputStr);
					FJsonSerializer::Serialize((*InputObj).ToSharedRef(), Writer);
					Writer->Close();
				}

				UE_LOG(LogUnrealClaude, Log, TEXT("NDJSON ToolUse: name=%s, id=%s, input=%d chars"),
					*ToolName, *ToolId, ToolInputStr.Len());

				if (CurrentConfig.OnStreamEvent.IsBound())
				{
					FClaudeStreamEvent Event;
					Event.Type = EClaudeStreamEventType::ToolUse;
					Event.ToolName = ToolName;
					Event.ToolCallId = ToolId;
					Event.ToolInput = ToolInputStr;
					Event.RawJson = JsonLine;
					FOnClaudeStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
					AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
					{
						EventDelegate.ExecuteIfBound(Event);
					});
				}
			}
			else
			{
				UE_LOG(LogUnrealClaude, Verbose, TEXT("NDJSON: unknown content block type: %s"), *ContentType);
			}
		}
	}
	else if (Type == TEXT("user"))
	{
		// Tool result message
		const TSharedPtr<FJsonObject>* MessageObj;
		if (!JsonObj->TryGetObjectField(TEXT("message"), MessageObj))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* ContentArray;
		if (!(*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& ContentValue : *ContentArray)
		{
			const TSharedPtr<FJsonObject>* ContentObj;
			if (!ContentValue->TryGetObject(ContentObj))
			{
				continue;
			}

			FString ContentType;
			if (!(*ContentObj)->TryGetStringField(TEXT("type"), ContentType))
			{
				continue;
			}

			if (ContentType == TEXT("tool_result"))
			{
				FString ToolUseId, ResultContent;
				(*ContentObj)->TryGetStringField(TEXT("tool_use_id"), ToolUseId);

				// content can be a string OR an array of content blocks
				if (!(*ContentObj)->TryGetStringField(TEXT("content"), ResultContent))
				{
					// Extract text from content block array: [{"type":"text","text":"..."},...]
					const TArray<TSharedPtr<FJsonValue>>* ResultArray;
					if ((*ContentObj)->TryGetArrayField(TEXT("content"), ResultArray))
					{
						for (const TSharedPtr<FJsonValue>& Block : *ResultArray)
						{
							const TSharedPtr<FJsonObject>* BlockObj;
							if (Block->TryGetObject(BlockObj))
							{
								FString BlockType;
								(*BlockObj)->TryGetStringField(TEXT("type"), BlockType);
								if (BlockType == TEXT("text"))
								{
									FString BlockText;
									if ((*BlockObj)->TryGetStringField(TEXT("text"), BlockText))
									{
										if (!ResultContent.IsEmpty())
										{
											ResultContent += TEXT("\n");
										}
										ResultContent += BlockText;
									}
								}
							}
						}
					}
				}

				UE_LOG(LogUnrealClaude, Log, TEXT("NDJSON ToolResult: tool_use_id=%s, content=%d chars"),
					*ToolUseId, ResultContent.Len());

				if (CurrentConfig.OnStreamEvent.IsBound())
				{
					FClaudeStreamEvent Event;
					Event.Type = EClaudeStreamEventType::ToolResult;
					Event.ToolCallId = ToolUseId;
					Event.ToolResultContent = ResultContent;
					Event.RawJson = JsonLine;
					FOnClaudeStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
					AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
					{
						EventDelegate.ExecuteIfBound(Event);
					});
				}
			}
		}
	}
	else if (Type == TEXT("result"))
	{
		// Final result message with stats
		FString ResultText, Subtype;
		JsonObj->TryGetStringField(TEXT("result"), ResultText);
		JsonObj->TryGetStringField(TEXT("subtype"), Subtype);
		bool bIsError = false;
		JsonObj->TryGetBoolField(TEXT("is_error"), bIsError);
		double DurationMs = 0.0;
		JsonObj->TryGetNumberField(TEXT("duration_ms"), DurationMs);
		double NumTurns = 0.0;
		JsonObj->TryGetNumberField(TEXT("num_turns"), NumTurns);
		double TotalCostUsd = 0.0;
		JsonObj->TryGetNumberField(TEXT("total_cost_usd"), TotalCostUsd);

		// Check for stop_reason in the result event
		FString StopReason;
		JsonObj->TryGetStringField(TEXT("stop_reason"), StopReason);

		UE_LOG(LogUnrealClaude, Log, TEXT("NDJSON Result: subtype=%s, is_error=%d, stop_reason=%s, duration=%.0fms, turns=%.0f, cost=$%.4f, result=%d chars"),
			*Subtype, bIsError, *StopReason, DurationMs, NumTurns, TotalCostUsd, ResultText.Len());

		// If the result carries a refusal stop_reason, emit a Refusal event
		// only if one wasn't already emitted from the assistant message
		if (StopReason == TEXT("refusal"))
		{
			if (!bRefusalDetected.Load())
			{
				UE_LOG(LogUnrealClaude, Warning, TEXT("NDJSON: Refusal detected in result event (stop_reason=refusal)"));

				bRefusalDetected.Store(true);
				AccumulatedResponseText.Empty();

				if (CurrentConfig.OnStreamEvent.IsBound())
				{
					FClaudeStreamEvent RefusalEvent;
					RefusalEvent.Type = EClaudeStreamEventType::Refusal;
					RefusalEvent.StopReason = StopReason;
					RefusalEvent.bIsError = true;
					RefusalEvent.RawJson = JsonLine;
					FOnClaudeStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
					AsyncTask(ENamedThreads::GameThread, [EventDelegate, RefusalEvent]()
					{
						EventDelegate.ExecuteIfBound(RefusalEvent);
					});
				}
			}
			else
			{
				UE_LOG(LogUnrealClaude, Log, TEXT("NDJSON: Refusal in result event (already handled from assistant message)"));
			}
		}

		if (CurrentConfig.OnStreamEvent.IsBound())
		{
			FClaudeStreamEvent Event;
			Event.Type = EClaudeStreamEventType::Result;
			Event.ResultText = ResultText;
			Event.StopReason = StopReason;
			Event.bIsError = bIsError;
			Event.DurationMs = static_cast<int32>(DurationMs);
			Event.NumTurns = static_cast<int32>(NumTurns);
			Event.TotalCostUsd = static_cast<float>(TotalCostUsd);
			Event.RawJson = JsonLine;
			FOnClaudeStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
			AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
			{
				EventDelegate.ExecuteIfBound(Event);
			});
		}
	}
	else
	{
		UE_LOG(LogUnrealClaude, Verbose, TEXT("NDJSON: unhandled message type: %s"), *Type);
	}
}

void FClaudeCodeRunner::Cancel()
{
	// Signal stop first - ReadProcessOutput() checks this and will exit its loop
	StopTaskCounter.Set(1);

	// Terminate the process to unblock any pending pipe reads
	// Don't close pipes/handles here - ExecuteProcess() on the worker thread
	// will handle cleanup after ReadProcessOutput() returns
	if (ProcessHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(ProcessHandle, true);
	}

	// Clear watchdog state so IsSilenceWarningActive() stops reporting the stale
	// silence window on the next request. LastPipeActivityMillis is otherwise only
	// re-seeded inside LaunchProcess, which leaves a window where the Slate tick
	// still sees 60+s of "silence" and the orange banner latches on immediately.
	LastPipeActivityMillis.Store(0);
	bSilenceBannerLatched.Store(false);
	bHangDiagnosticLogged.Store(false);
}

bool FClaudeCodeRunner::Init()
{
	// bIsExecuting is already set by ExecuteAsync (thread-safe)
	StopTaskCounter.Reset();
	bRefusalDetected.Store(false);
	NdjsonLineBuffer.Empty();
	AccumulatedResponseText.Empty();
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

double FClaudeCodeRunner::GetSilenceSeconds() const
{
	const int64 LastMillis = LastPipeActivityMillis.Load();
	if (LastMillis == 0)
	{
		return 0.0;
	}
	const double NowSec = FPlatformTime::Seconds();
	// LastPipeActivityMillis is stored in FPlatformTime::Seconds() units * 1000.
	return NowSec - (static_cast<double>(LastMillis) / 1000.0);
}

bool FClaudeCodeRunner::IsSilenceWarningActive() const
{
	// Compute from the timestamp directly rather than relying on bSilenceBannerLatched.
	// The latch is only set by MaybeFireSilenceWatchdog, which runs on the worker thread's
	// read loop — but the subprocess can hang BEFORE the read loop starts (e.g. stdin
	// WritePipe blocks when the child isn't reading stdin fast enough). By computing
	// silence here on every game-thread Slate tick we catch those pre-read-loop hangs too.
	// LastPipeActivityMillis is seeded in LaunchProcess, so it's valid from process start.
	if (LastPipeActivityMillis.Load() == 0)
	{
		return false;
	}
	return GetSilenceSeconds() >= SilenceWarningThresholdSeconds;
}

void FClaudeCodeRunner::RecordPipeActivity()
{
	const int64 NowMillis = static_cast<int64>(FPlatformTime::Seconds() * 1000.0);
	LastPipeActivityMillis.Store(NowMillis);
	bSilenceBannerLatched.Store(false);
}

FString FClaudeCodeRunner::BuildHangDiagnostic(
	double SilenceSeconds,
	bool bProcRunning,
	const FString& StdinPayload,
	const FString& NdjsonLineBufferSnapshot,
	int32 TaskQueuePending,
	int32 TaskQueueRunning,
	int32 TaskQueueCompleted)
{
	auto HeadSlice = [](const FString& S, int32 N) -> FString
	{
		return S.Len() <= N ? S : S.Left(N);
	};
	auto TailSlice = [](const FString& S, int32 N) -> FString
	{
		return S.Len() <= N ? S : S.Right(N);
	};
	auto EscapeForSingleLine = [](const FString& S) -> FString
	{
		return S.Replace(TEXT("\r"), TEXT("\\r")).Replace(TEXT("\n"), TEXT("\\n"));
	};

	constexpr int32 PreviewChars = 500;
	const FString PayloadHead = EscapeForSingleLine(HeadSlice(StdinPayload, PreviewChars));
	const FString PayloadTail = EscapeForSingleLine(TailSlice(StdinPayload, PreviewChars));
	const FString BufferTail = EscapeForSingleLine(TailSlice(NdjsonLineBufferSnapshot, PreviewChars));

	return FString::Printf(
		TEXT("[SilenceWatchdog] silence_sec=%d proc_running=%s payload_bytes=%d buffer_bytes=%d mcp_queue=%d/%d/%d payload_head=\"%s\" payload_tail=\"%s\" buffer_tail=\"%s\""),
		FMath::FloorToInt(SilenceSeconds),
		bProcRunning ? TEXT("true") : TEXT("false"),
		StdinPayload.Len(),
		NdjsonLineBufferSnapshot.Len(),
		TaskQueuePending, TaskQueueRunning, TaskQueueCompleted,
		*PayloadHead, *PayloadTail, *BufferTail);
}

bool FClaudeCodeRunner::MaybeFireSilenceWatchdog(double NowPlatformSeconds)
{
	const int64 LastMillis = LastPipeActivityMillis.Load();
	if (LastMillis == 0)
	{
		// No subprocess activity recorded yet — nothing to evaluate
		return false;
	}
	const double SilenceSec = NowPlatformSeconds - (static_cast<double>(LastMillis) / 1000.0);

	if (SilenceSec < SilenceWarningThresholdSeconds)
	{
		return false;
	}

	// Banner latch: set unconditionally when over threshold. RecordPipeActivity clears it.
	bSilenceBannerLatched.Store(true);

	// Diagnostic latch: one-shot per session.
	bool bExpected = false;
	const bool bWonTheRace = bHangDiagnosticLogged.CompareExchange(bExpected, true);
	if (!bWonTheRace)
	{
		// Diagnostic already logged this session
		return false;
	}

	// We won the race — emit the real diagnostic.
	int32 Pending = 0, Running = 0, Completed = 0;
	if (FUnrealClaudeModule::IsAvailable())
	{
		TSharedPtr<FUnrealClaudeMCPServer> Server = FUnrealClaudeModule::Get().GetMCPServer();
		if (Server.IsValid())
		{
			TSharedPtr<FMCPToolRegistry> Registry = Server->GetToolRegistry();
			if (Registry.IsValid())
			{
				if (TSharedPtr<FMCPTaskQueue> Queue = Registry->GetTaskQueue())
				{
					Queue->GetStats(Pending, Running, Completed);
				}
			}
		}
	}

	const FString Diag = BuildHangDiagnostic(
		SilenceSec,
		FPlatformProcess::IsProcRunning(ProcessHandle),
		LastStdinPayload,
		NdjsonLineBuffer,
		Pending, Running, Completed);
	UE_LOG(LogUnrealClaude, Warning, TEXT("%s"), *Diag);
	return true;
}

bool FClaudeCodeRunner::CreateProcessPipes()
{
	// Create stdout pipe (we read from ReadPipe, child writes to WritePipe)
	if (!FPlatformProcess::CreatePipe(ReadPipe, WritePipe, false))
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to create stdout pipe"));
		return false;
	}

	// Create stdin pipe (child reads from StdInReadPipe, we write to StdInWritePipe)
	if (!FPlatformProcess::CreatePipe(StdInReadPipe, StdInWritePipe, true))
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to create stdin pipe"));
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = nullptr;
		WritePipe = nullptr;
		return false;
	}

	return true;
}

bool FClaudeCodeRunner::LaunchProcess(const FString& FullCommand, const FString& WorkingDir)
{
	FString ClaudePath = GetClaudePath();

	// FPlatformProcess::CreateProc takes the URL (executable) and Params separately
	FString Params = FullCommand;

	ProcessHandle = FPlatformProcess::CreateProc(
		*ClaudePath,
		*Params,
		false,    // bLaunchDetached
		false,    // bLaunchHidden
		true,     // bLaunchReallyHidden
		nullptr,  // OutProcessID
		0,        // PriorityModifier
		*WorkingDir,
		WritePipe,    // PipeWriteChild - child's stdout goes here
		StdInReadPipe // PipeReadChild - child reads stdin from here
	);

	if (!ProcessHandle.IsValid())
	{
		// Process never started, so there is no exit code or stderr yet.
		// Log everything we have so the user can diagnose (path, full command, working dir).
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to start Claude process (CreateProc returned invalid handle)."));
		UE_LOG(LogUnrealClaude, Error, TEXT("  Claude Path : %s"), *ClaudePath);
		UE_LOG(LogUnrealClaude, Error, TEXT("  Working Dir : %s"), *WorkingDir);
		UE_LOG(LogUnrealClaude, Error, TEXT("  Full Command: %s"), *Params);
		UE_LOG(LogUnrealClaude, Error, TEXT("  Common causes: Claude CLI missing/unexecutable, antivirus blocking the process,"));
		UE_LOG(LogUnrealClaude, Error, TEXT("                 invalid working directory, or missing system libraries."));
		return false;
	}

	// Reset watchdog state for this new subprocess
	bSilenceBannerLatched.Store(false);
	bHangDiagnosticLogged.Store(false);
	LastPipeActivityMillis.Store(static_cast<int64>(FPlatformTime::Seconds() * 1000.0));
	LastStdinPayload.Empty();

	return true;
}

FString FClaudeCodeRunner::ReadProcessOutput()
{
	FString FullOutput;
	bool bOutputCapped = false;

	// Reset NDJSON state for this request
	NdjsonLineBuffer.Empty();
	AccumulatedResponseText.Empty();

	while (!StopTaskCounter.GetValue())
	{
		FString OutputChunk = FPlatformProcess::ReadPipe(ReadPipe);

		if (!OutputChunk.IsEmpty())
		{
			RecordPipeActivity();

			// Hard byte cap on accumulated stdout to bound memory use
			const int32 Cap = UnrealClaudeConstants::Process::MaxProcessOutputBytes;
			if (!bOutputCapped && FullOutput.Len() + OutputChunk.Len() > Cap)
			{
				const int32 Remain = FMath::Max(0, Cap - FullOutput.Len());
				if (Remain > 0)
				{
					FullOutput += OutputChunk.Left(Remain);
				}
				bOutputCapped = true;
				UE_LOG(LogUnrealClaude, Warning,
					TEXT("Claude process output exceeded %d bytes; truncating further reads"), Cap);
				// Still drain the pipe below so the child does not block on a full pipe,
				// but stop growing FullOutput / NDJSON buffers.
			}
			else if (!bOutputCapped)
			{
				FullOutput += OutputChunk;

				// NDJSON is line-delimited: buffer chunks until a newline, then split
				NdjsonLineBuffer += OutputChunk;
			}

			int32 NewlineIdx;
			while (!bOutputCapped && NdjsonLineBuffer.FindChar(TEXT('\n'), NewlineIdx))
			{
				FString CompleteLine = NdjsonLineBuffer.Left(NewlineIdx);
				CompleteLine.TrimEndInline();
				NdjsonLineBuffer.RightChopInline(NewlineIdx + 1);

				if (!CompleteLine.IsEmpty())
				{
					ParseAndEmitNdjsonLine(CompleteLine);
				}
			}
		}
		else
		{
			MaybeFireSilenceWatchdog(FPlatformTime::Seconds());
		}

		if (!FPlatformProcess::IsProcRunning(ProcessHandle))
		{
			// Drain any output the child wrote between our last read and its exit
			FString RemainingOutput = FPlatformProcess::ReadPipe(ReadPipe);
			while (!RemainingOutput.IsEmpty())
			{
				RecordPipeActivity();
				if (!bOutputCapped)
				{
					const int32 Cap = UnrealClaudeConstants::Process::MaxProcessOutputBytes;
					if (FullOutput.Len() + RemainingOutput.Len() > Cap)
					{
						const int32 Remain = FMath::Max(0, Cap - FullOutput.Len());
						if (Remain > 0)
						{
							FullOutput += RemainingOutput.Left(Remain);
							NdjsonLineBuffer += RemainingOutput.Left(Remain);
						}
						bOutputCapped = true;
					}
					else
					{
						FullOutput += RemainingOutput;
						NdjsonLineBuffer += RemainingOutput;
					}
				}
				RemainingOutput = FPlatformProcess::ReadPipe(ReadPipe);
			}

			int32 FinalNewlineIdx;
			while (NdjsonLineBuffer.FindChar(TEXT('\n'), FinalNewlineIdx))
			{
				FString CompleteLine = NdjsonLineBuffer.Left(FinalNewlineIdx);
				CompleteLine.TrimEndInline();
				NdjsonLineBuffer.RightChopInline(FinalNewlineIdx + 1);

				if (!CompleteLine.IsEmpty())
				{
					ParseAndEmitNdjsonLine(CompleteLine);
				}
			}

			// Parse any final incomplete line (no trailing newline)
			NdjsonLineBuffer.TrimEndInline();
			if (!NdjsonLineBuffer.IsEmpty())
			{
				ParseAndEmitNdjsonLine(NdjsonLineBuffer);
				NdjsonLineBuffer.Empty();
			}

			// Clean-exit-no-output: subprocess finished without producing any NDJSON.
			// This is the "silent crash" path — emit a diagnostic so the user knows why.
			if (FullOutput.IsEmpty() && !bHangDiagnosticLogged.Load())
			{
				// Force-set LastPipeActivityMillis to a value old enough to cross the threshold
				// so MaybeFireSilenceWatchdog emits the diagnostic with current elapsed time.
				const double FakeSilenceSec = SilenceWarningThresholdSeconds + 1.0;
				LastPipeActivityMillis.Store(static_cast<int64>((FPlatformTime::Seconds() - FakeSilenceSec) * 1000.0));
				MaybeFireSilenceWatchdog(FPlatformTime::Seconds());
			}

			break;
		}

		// Brief sleep to avoid busy-waiting the read loop
		FPlatformProcess::Sleep(0.01f);
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

void FClaudeCodeRunner::ExecuteProcess()
{
	FString ClaudePath = GetClaudePath();

	if (ClaudePath.IsEmpty())
	{
		ReportError(TEXT("Claude CLI not found. Please install with: npm install -g @anthropic-ai/claude-code then restart the editor."));
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

	// Set working directory - convert to absolute path since FPaths::ProjectDir()
	// returns a relative path on macOS that external processes can't resolve
	FString WorkingDir = CurrentConfig.WorkingDirectory;
	if (WorkingDir.IsEmpty())
	{
		WorkingDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	if (!CreateProcessPipes())
	{
		ReportError(TEXT("Failed to create pipe for Claude process"));
		return;
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Full command: %s %s"), *ClaudePath, *CommandLine);
	UE_LOG(LogUnrealClaude, Log, TEXT("Working directory: %s"), *WorkingDir);

	if (!LaunchProcess(CommandLine, WorkingDir))
	{
		CleanupHandles();

		// Truncate the command for the user message but keep the full version in the log above
		FString ShortCmd = CommandLine.Len() > 180 ? CommandLine.Left(180) + TEXT("...") : CommandLine;
		FString ErrorMsg = FString::Printf(
			TEXT("Failed to start Claude process.\n\n")
			TEXT("Claude Path: %s\n")
			TEXT("Working Dir: %s\n")
			TEXT("Command: %s\n\n")
			TEXT("Check the Output Log (LogUnrealClaude) for the full command line.\n")
			TEXT("Common fixes: reinstall Claude Code (npm install -g @anthropic-ai/claude-code),\n")
			TEXT("make sure the path is executable, and restart the editor."),
			*ClaudePath,
			*WorkingDir,
			*ShortCmd
		);
		ReportError(ErrorMsg);
		return;
	}

	// Write prompt to stdin as stream-json NDJSON payload
	if (StdInWritePipe)
	{
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

		// Always use stream-json payload (handles text-only and image cases uniformly)
		FString StdinPayload = BuildStreamJsonPayload(TextPrompt, CurrentConfig.AttachedImagePaths);
		// Cache the stdin payload for the silence watchdog diagnostic.
		LastStdinPayload = StdinPayload;

		if (!StdinPayload.IsEmpty())
		{
			FTCHARToUTF8 Utf8Payload(*StdinPayload);
			int32 BytesWritten = 0;
			bool bWritten = FPlatformProcess::WritePipe(StdInWritePipe, (const uint8*)Utf8Payload.Get(), Utf8Payload.Length(), &BytesWritten);
			UE_LOG(LogUnrealClaude, Log, TEXT("Wrote to Claude stdin (stream-json, success=%d, %d/%d bytes, images: %d, system: %d chars, user: %d chars)"),
				bWritten, BytesWritten, Utf8Payload.Length(), CurrentConfig.AttachedImagePaths.Num(),
				CurrentConfig.SystemPrompt.Len(), CurrentConfig.Prompt.Len());
		}

		// Close stdin write pipe to signal EOF to Claude
		// We close the entire stdin pipe pair since child has the read end
		FPlatformProcess::ClosePipe(StdInReadPipe, StdInWritePipe);
		StdInReadPipe = nullptr;
		StdInWritePipe = nullptr;
	}

	// Delete per-request prompt files now that stdin has been written
	if (!SystemPromptFilePath.IsEmpty())
	{
		IFileManager::Get().Delete(*SystemPromptFilePath);
		SystemPromptFilePath.Empty();
	}
	if (!PromptFilePath.IsEmpty())
	{
		IFileManager::Get().Delete(*PromptFilePath);
		PromptFilePath.Empty();
	}

	// Read output until process completes (NDJSON events parsed during reading)
	FString FullOutput = ReadProcessOutput();

	// Use accumulated response text from parsed NDJSON events
	// Falls back to legacy ParseStreamJsonOutput if no events were parsed
	FString ResponseText = AccumulatedResponseText;
	if (ResponseText.IsEmpty() && !FullOutput.IsEmpty())
	{
		// Fallback: try legacy parsing in case NDJSON format wasn't as expected
		ResponseText = ParseStreamJsonOutput(FullOutput);
		UE_LOG(LogUnrealClaude, Log, TEXT("NDJSON parser produced no text, fell back to legacy parser (%d chars)"),
			ResponseText.Len());
	}

	int32 ExitCode = 0;
	FPlatformProcess::GetProcReturnCode(ProcessHandle, &ExitCode);

	if (ReadPipe || WritePipe)
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = nullptr;
		WritePipe = nullptr;
	}
	FPlatformProcess::CloseProc(ProcessHandle);

	// A refusal is treated as a failure to prevent the refused exchange from being saved
	bool bSuccess = (ExitCode == 0) && !StopTaskCounter.GetValue() && !bRefusalDetected.Load();

	if (!bSuccess)
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Claude process finished with failure (exit code %d)."), ExitCode);
		if (!FullOutput.IsEmpty())
		{
			// Log a truncated version of whatever the process wrote (stdout/stderr combined on the pipe)
			FString LogSnippet = FullOutput.Len() > 1000 ? FullOutput.Left(1000) + TEXT("...") : FullOutput;
			UE_LOG(LogUnrealClaude, Error, TEXT("Process output:\n%s"), *LogSnippet);
		}
		else
		{
			UE_LOG(LogUnrealClaude, Error, TEXT("Process produced no output."));
		}

		// If we have no parsed response, give the user a short diagnostic instead of a blank message
		if (ResponseText.IsEmpty())
		{
			ResponseText = FString::Printf(
				TEXT("Claude process failed (exit code %d). Check the Output Log (LogUnrealClaude) for details."),
				ExitCode
			);
		}
	}

	ReportCompletion(ResponseText, bSuccess);
}

// FClaudeCodeSubsystem is now in ClaudeSubsystem.cpp
