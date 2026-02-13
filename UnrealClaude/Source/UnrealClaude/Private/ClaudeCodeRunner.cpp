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

	// Check common locations for claude CLI
	TArray<FString> PossiblePaths;

#if PLATFORM_WINDOWS
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
#else
	// Linux/Mac: Claude Code native installer location
	FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
	if (!Home.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".local"), TEXT("bin"), TEXT("claude")));
	}

	// Common system paths
	PossiblePaths.Add(TEXT("/usr/local/bin/claude"));
	PossiblePaths.Add(TEXT("/usr/bin/claude"));

	// npm global install locations
	if (!Home.IsEmpty())
	{
		// npm default global prefix on Linux
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".npm-global"), TEXT("bin"), TEXT("claude")));
		// nvm-managed node
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".nvm"), TEXT("versions"), TEXT("node")));
	}

	// Check PATH
	FString PathEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	TArray<FString> PathDirs;
	PathEnv.ParseIntoArray(PathDirs, TEXT(":"), true);

	for (const FString& Dir : PathDirs)
	{
		PossiblePaths.Add(FPaths::Combine(Dir, TEXT("claude")));
	}
#endif

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

	// Try using 'where' (Windows) or 'which' (Linux/Mac) as fallback
	FString WhereOutput;
	FString WhereErrors;
	int32 ReturnCode;

#if PLATFORM_WINDOWS
	const TCHAR* WhichCmd = TEXT("where");
#else
	const TCHAR* WhichCmd = TEXT("/usr/bin/which");
#endif

	if (FPlatformProcess::ExecProcess(WhichCmd, TEXT("claude"), &ReturnCode, &WhereOutput, &WhereErrors) && ReturnCode == 0)
	{
		WhereOutput.TrimStartAndEndInline();
		TArray<FString> Lines;
		WhereOutput.ParseIntoArrayLines(Lines);
		if (Lines.Num() > 0)
		{
			UE_LOG(LogUnrealClaude, Log, TEXT("Found Claude CLI via '%s': %s"), WhichCmd, *Lines[0]);
			CachedClaudePath = Lines[0];
			return CachedClaudePath;
		}
	}

	UE_LOG(LogUnrealClaude, Warning, TEXT("Claude CLI not found. Please install with: npm install -g @anthropic-ai/claude-code"));

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
		if (!FullImagePath.StartsWith(ExpectedDir))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Rejecting image path outside screenshots directory: %s"), *FullImagePath);
			continue;
		}
		if (!FPaths::FileExists(FullImagePath))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Attached image file no longer exists: %s"), *FullImagePath);
			continue;
		}

		// Check per-file size
		const int64 FileSize = IFileManager::Get().FileSize(*FullImagePath);
		if (FileSize > MaxImageFileSize)
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Image file too large for base64 encoding: %s (%lld bytes, max %lld)"),
				*FullImagePath, FileSize, MaxImageFileSize);
			continue;
		}

		// Check total payload size
		if (TotalImageBytes + FileSize > MaxTotalImagePayloadSize)
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Skipping image (total payload would exceed %lld bytes): %s"),
				MaxTotalImagePayloadSize, *FullImagePath);
			continue;
		}

		// Load and base64 encode the PNG
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
				(*ContentObj)->TryGetStringField(TEXT("content"), ResultContent);

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

		UE_LOG(LogUnrealClaude, Log, TEXT("NDJSON Result: subtype=%s, is_error=%d, duration=%.0fms, turns=%.0f, cost=$%.4f, result=%d chars"),
			*Subtype, bIsError, DurationMs, NumTurns, TotalCostUsd, ResultText.Len());

		if (CurrentConfig.OnStreamEvent.IsBound())
		{
			FClaudeStreamEvent Event;
			Event.Type = EClaudeStreamEventType::Result;
			Event.ResultText = ResultText;
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
}

bool FClaudeCodeRunner::Init()
{
	// bIsExecuting is already set by ExecuteAsync (thread-safe)
	StopTaskCounter.Reset();
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
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to create Claude process"));
		UE_LOG(LogUnrealClaude, Error, TEXT("Claude Path: %s"), *ClaudePath);
		UE_LOG(LogUnrealClaude, Error, TEXT("Params: %s"), *Params);
		UE_LOG(LogUnrealClaude, Error, TEXT("Working directory: %s"), *WorkingDir);
		return false;
	}

	return true;
}

FString FClaudeCodeRunner::ReadProcessOutput()
{
	FString FullOutput;

	// Reset NDJSON state for this request
	NdjsonLineBuffer.Empty();
	AccumulatedResponseText.Empty();

	while (!StopTaskCounter.GetValue())
	{
		// Read any available output from the pipe
		FString OutputChunk = FPlatformProcess::ReadPipe(ReadPipe);

		if (!OutputChunk.IsEmpty())
		{
			FullOutput += OutputChunk;

			// Parse NDJSON line-by-line: buffer chunks and split on newlines
			NdjsonLineBuffer += OutputChunk;

			int32 NewlineIdx;
			while (NdjsonLineBuffer.FindChar(TEXT('\n'), NewlineIdx))
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

		// Check if process has exited
		if (!FPlatformProcess::IsProcRunning(ProcessHandle))
		{
			// Process finished - read any remaining output
			FString RemainingOutput = FPlatformProcess::ReadPipe(ReadPipe);
			while (!RemainingOutput.IsEmpty())
			{
				FullOutput += RemainingOutput;
				NdjsonLineBuffer += RemainingOutput;
				RemainingOutput = FPlatformProcess::ReadPipe(ReadPipe);
			}

			// Parse all remaining buffered lines
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

			break;
		}

		// Brief sleep to avoid busy-waiting
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

	UE_LOG(LogUnrealClaude, Log, TEXT("Full command: %s %s"), *ClaudePath, *CommandLine);
	UE_LOG(LogUnrealClaude, Log, TEXT("Working directory: %s"), *WorkingDir);

	if (!LaunchProcess(CommandLine, WorkingDir))
	{
		CleanupHandles();

		FString ErrorMsg = FString::Printf(
			TEXT("Failed to start Claude process.\n\n")
			TEXT("Claude Path: %s\n")
			TEXT("Working Dir: %s\n\n")
			TEXT("Command (truncated): %.200s..."),
			*ClaudePath,
			*WorkingDir,
			*CommandLine
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

		// Write to stdin
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

	// Clear temp file paths
	SystemPromptFilePath.Empty();
	PromptFilePath.Empty();

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

	// Get exit code
	int32 ExitCode = 0;
	FPlatformProcess::GetProcReturnCode(ProcessHandle, &ExitCode);

	// Cleanup handles
	if (ReadPipe || WritePipe)
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = nullptr;
		WritePipe = nullptr;
	}
	FPlatformProcess::CloseProc(ProcessHandle);

	// Report completion with parsed response text
	bool bSuccess = (ExitCode == 0) && !StopTaskCounter.GetValue();
	ReportCompletion(ResponseText, bSuccess);
}

// FClaudeCodeSubsystem is now in ClaudeSubsystem.cpp
