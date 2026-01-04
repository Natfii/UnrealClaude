// Copyright Your Name. All Rights Reserved.

#include "ScriptExecutionManager.h"
#include "ScriptPermissionDialog.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeUtils.h"
#include "JsonUtils.h"
#include "MCP/MCPParamValidator.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Editor.h"
#include "Engine/World.h"

// Live Coding support
#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

FScriptExecutionManager& FScriptExecutionManager::Get()
{
	static FScriptExecutionManager Instance;
	return Instance;
}

FScriptExecutionManager::FScriptExecutionManager()
	: MaxHistorySize(100)
	, ScriptCounter(0)
{
	LoadHistory();

	// Ensure script directories exist on startup
	FString ContentScriptDir = GetContentScriptDirectory();
	if (!IFileManager::Get().DirectoryExists(*ContentScriptDir))
	{
		IFileManager::Get().MakeDirectory(*ContentScriptDir, true);
		UE_LOG(LogUnrealClaude, Log, TEXT("Created script directory: %s"), *ContentScriptDir);
	}
}

FScriptExecutionManager::~FScriptExecutionManager()
{
	SaveHistory();
}

FScriptExecutionResult FScriptExecutionManager::ExecuteScript(
	EScriptType Type,
	const FString& ScriptContent,
	const FString& Description)
{
	// Parse description from header if not provided
	FString FinalDescription = Description;
	if (FinalDescription.IsEmpty())
	{
		FinalDescription = ScriptHeader::ParseDescription(ScriptContent);
	}

	// Show permission dialog
	if (!ShowPermissionDialog(ScriptContent, Type, FinalDescription))
	{
		return FScriptExecutionResult::Error(TEXT("Script execution denied by user"));
	}

	// Execute based on type
	FScriptExecutionResult Result;
	switch (Type)
	{
		case EScriptType::Cpp:
			Result = ExecuteCpp(ScriptContent, FinalDescription);
			break;
		case EScriptType::Python:
			Result = ExecutePython(ScriptContent, FinalDescription);
			break;
		case EScriptType::Console:
			Result = ExecuteConsole(ScriptContent, FinalDescription);
			break;
		case EScriptType::EditorUtility:
			Result = ExecuteEditorUtility(ScriptContent, FinalDescription);
			break;
		default:
			Result = FScriptExecutionResult::Error(TEXT("Unknown script type"));
	}

	return Result;
}

bool FScriptExecutionManager::ShowPermissionDialog(
	const FString& ScriptPreview,
	EScriptType Type,
	const FString& Description)
{
	// Delegate to the extracted permission dialog class
	return FScriptPermissionDialog::Show(ScriptPreview, Type, Description);
}

FScriptExecutionResult FScriptExecutionManager::ExecuteCpp(
	const FString& ScriptContent,
	const FString& Description)
{
#if WITH_LIVE_CODING
	// Generate script name and write to file
	FString ScriptName = GenerateScriptName(EScriptType::Cpp, Description);
	FString FilePath = WriteScriptFile(ScriptContent, EScriptType::Cpp, ScriptName);

	if (FilePath.IsEmpty())
	{
		return FScriptExecutionResult::Error(TEXT("Failed to write C++ script file"));
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("C++ script written to: %s"), *FilePath);

	// Trigger Live Coding compilation
	FString ErrorLog;
	FScriptExecutionResult Result;

	if (TriggerLiveCodingCompile(ErrorLog))
	{
		// Compilation succeeded
		Result = FScriptExecutionResult::Success(
			TEXT("C++ script compiled successfully via Live Coding"),
			TEXT("Script file: ") + FilePath
		);

		// Add to history
		FScriptHistoryEntry Entry;
		Entry.ScriptType = EScriptType::Cpp;
		Entry.Filename = ScriptName + TEXT(".cpp");
		Entry.Description = Description;
		Entry.bSuccess = true;
		Entry.ResultMessage = Result.Message;
		Entry.FilePath = FilePath;
		AddToHistory(Entry);

		return Result;
	}

	// Compilation failed - return error for Claude to fix and retry
	// Claude will call execute_script again with fixed code
	UE_LOG(LogUnrealClaude, Warning, TEXT("C++ compilation failed, returning error for Claude to fix"));

	Result = FScriptExecutionResult::Error(
		TEXT("Compilation failed. Fix these errors and call execute_script again:\n\n") + ErrorLog,
		ErrorLog
	);

	// Add to history as failure
	FScriptHistoryEntry Entry;
	Entry.ScriptType = EScriptType::Cpp;
	Entry.Filename = ScriptName + TEXT(".cpp");
	Entry.Description = Description;
	Entry.bSuccess = false;
	Entry.ResultMessage = TEXT("Compilation failed: ") + ErrorLog.Left(200);
	Entry.FilePath = FilePath;
	AddToHistory(Entry);

	return Result;
#else
	return FScriptExecutionResult::Error(TEXT("Live Coding not available - C++ scripts cannot be compiled"));
#endif
}

bool FScriptExecutionManager::TriggerLiveCodingCompile(FString& OutErrorLog)
{
#if WITH_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>("LiveCoding");
	if (!LiveCoding)
	{
		OutErrorLog = TEXT("Live Coding module not loaded");
		return false;
	}

	if (!LiveCoding->IsEnabledForSession())
	{
		OutErrorLog = TEXT("Live Coding is not enabled. Press Ctrl+Alt+F11 to enable.");
		return false;
	}

	// Trigger compilation
	LiveCoding->Compile(ELiveCodingCompileFlags::None, nullptr);

	// Wait for compilation (simplified - in production would use delegates)
	float WaitTime = 0.0f;
	const float MaxWait = 60.0f;
	const float PollInterval = 0.5f;

	while (LiveCoding->IsCompiling() && WaitTime < MaxWait)
	{
		FPlatformProcess::Sleep(PollInterval);
		WaitTime += PollInterval;
	}

	if (WaitTime >= MaxWait)
	{
		OutErrorLog = TEXT("Live Coding compilation timed out (60s)");
		return false;
	}

	// Check for errors - would need to parse output log in production
	// For now, assume success if compilation completed
	return true;
#else
	OutErrorLog = TEXT("Live Coding not available in this build");
	return false;
#endif
}

FScriptExecutionResult FScriptExecutionManager::ExecutePython(
	const FString& ScriptContent,
	const FString& Description)
{
	// Check if editor is available
	if (!GEditor)
	{
		return FScriptExecutionResult::Error(TEXT("Editor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FScriptExecutionResult::Error(TEXT("No active world"));
	}

	// Write script to file
	FString ScriptName = GenerateScriptName(EScriptType::Python, Description);
	FString FilePath = WriteScriptFile(ScriptContent, EScriptType::Python, ScriptName);

	if (FilePath.IsEmpty())
	{
		return FScriptExecutionResult::Error(TEXT("Failed to write Python script file"));
	}

	// Execute via console command
	FString Command = FString::Printf(TEXT("py \"%s\""), *FilePath);

	// Capture output using shared utility
	FStringOutputDevice OutputDevice;
	GEditor->Exec(World, *Command, OutputDevice);

	FString Output = OutputDevice.GetTrimmedOutput();

	// Detect Python errors in output
	bool bHasError = Output.Contains(TEXT("Traceback")) ||
	                 Output.Contains(TEXT("Error:")) ||
	                 Output.Contains(TEXT("SyntaxError")) ||
	                 Output.Contains(TEXT("NameError")) ||
	                 Output.Contains(TEXT("TypeError")) ||
	                 Output.Contains(TEXT("ValueError")) ||
	                 Output.Contains(TEXT("ImportError")) ||
	                 Output.Contains(TEXT("AttributeError"));

	// Add to history
	FScriptHistoryEntry Entry;
	Entry.ScriptType = EScriptType::Python;
	Entry.Filename = ScriptName + TEXT(".py");
	Entry.Description = Description;
	Entry.bSuccess = !bHasError;
	Entry.ResultMessage = Output.Left(200);
	Entry.FilePath = FilePath;
	AddToHistory(Entry);

	if (bHasError)
	{
		return FScriptExecutionResult::Error(
			TEXT("Python script execution failed"),
			Output
		);
	}

	return FScriptExecutionResult::Success(
		TEXT("Python script executed"),
		Output
	);
}

FScriptExecutionResult FScriptExecutionManager::ExecuteConsole(
	const FString& ScriptContent,
	const FString& Description)
{
	if (!GEditor)
	{
		return FScriptExecutionResult::Error(TEXT("Editor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FScriptExecutionResult::Error(TEXT("No active world"));
	}

	// Parse commands (one per line)
	TArray<FString> Commands;
	ScriptContent.ParseIntoArrayLines(Commands, true);

	// Output capture using shared utility
	FStringOutputDevice OutputDevice;
	FString AllOutput;
	int32 ExecutedCount = 0;

	for (const FString& RawCommand : Commands)
	{
		FString Command = RawCommand.TrimStartAndEnd();

		// Skip empty lines and comments
		if (Command.IsEmpty() || Command.StartsWith(TEXT("#")) || Command.StartsWith(TEXT("//")))
		{
			continue;
		}

		// Skip header metadata lines
		if (Command.Contains(TEXT("@UnrealClaude")) || Command.Contains(TEXT("@Name:")) ||
			Command.Contains(TEXT("@Description:")) || Command.Contains(TEXT("@Created:")))
		{
			continue;
		}

		// Validate command
		FString ValidationError;
		if (!FMCPParamValidator::ValidateConsoleCommand(Command, ValidationError))
		{
			AllOutput += FString::Printf(TEXT("Skipped blocked command: %s\n"), *Command);
			continue;
		}

		OutputDevice.Clear();
		GEditor->Exec(World, *Command, OutputDevice);
		AllOutput += FString::Printf(TEXT("> %s\n%s\n"), *Command, *OutputDevice.GetTrimmedOutput());
		ExecutedCount++;
	}

	// Add to history
	FScriptHistoryEntry Entry;
	Entry.ScriptType = EScriptType::Console;
	Entry.Filename = FString::Printf(TEXT("console_%d.txt"), ScriptCounter);
	Entry.Description = Description;
	Entry.bSuccess = ExecutedCount > 0;
	Entry.ResultMessage = FString::Printf(TEXT("Executed %d commands"), ExecutedCount);
	AddToHistory(Entry);
	ScriptCounter++;

	return FScriptExecutionResult::Success(
		FString::Printf(TEXT("Executed %d console commands"), ExecutedCount),
		AllOutput
	);
}

FScriptExecutionResult FScriptExecutionManager::ExecuteEditorUtility(
	const FString& ScriptContent,
	const FString& Description)
{
	// Editor Utility execution is more complex - would need to create Blueprint asset
	// For now, return not implemented
	return FScriptExecutionResult::Error(
		TEXT("Editor Utility script execution not yet implemented. Use Python or Console commands instead.")
	);
}

FString FScriptExecutionManager::WriteScriptFile(
	const FString& Content,
	EScriptType Type,
	const FString& ScriptName)
{
	FString Directory;
	FString Extension = GetScriptExtension(Type);

	if (Type == EScriptType::Cpp)
	{
		Directory = GetCppScriptDirectory();
	}
	else
	{
		Directory = GetContentScriptDirectory();
	}

	// Ensure directory exists
	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}

	FString FilePath = FPaths::Combine(Directory, ScriptName + Extension);

	if (FFileHelper::SaveStringToFile(Content, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return FilePath;
	}

	UE_LOG(LogUnrealClaude, Error, TEXT("Failed to write script to: %s"), *FilePath);
	return FString();
}

FString FScriptExecutionManager::GenerateScriptName(EScriptType Type, const FString& Description)
{
	// Sanitize description for filename using single-pass character filtering
	FString BaseName;
	BaseName.Reserve(30);

	// Invalid filename characters on Windows
	static const TCHAR* InvalidChars = TEXT(" /\\:*?\"<>|");

	int32 CharCount = 0;
	for (TCHAR C : Description)
	{
		if (CharCount >= 30)
		{
			break;
		}

		// Replace invalid characters with underscore
		bool bIsInvalid = false;
		for (const TCHAR* InvalidChar = InvalidChars; *InvalidChar; ++InvalidChar)
		{
			if (C == *InvalidChar)
			{
				bIsInvalid = true;
				break;
			}
		}

		BaseName.AppendChar(bIsInvalid ? TEXT('_') : C);
		CharCount++;
	}

	if (BaseName.IsEmpty())
	{
		BaseName = TEXT("Script");
	}

	ScriptCounter++;
	return FString::Printf(TEXT("%s_%03d"), *BaseName, ScriptCounter);
}

void FScriptExecutionManager::AddToHistory(const FScriptHistoryEntry& Entry)
{
	History.Add(Entry);

	// Trim if exceeds max
	while (History.Num() > MaxHistorySize)
	{
		History.RemoveAt(0);
	}

	// Auto-save
	SaveHistory();
}

TArray<FScriptHistoryEntry> FScriptExecutionManager::GetRecentScripts(int32 Count) const
{
	TArray<FScriptHistoryEntry> Recent;
	int32 StartIdx = FMath::Max(0, History.Num() - Count);

	for (int32 i = History.Num() - 1; i >= StartIdx; --i)
	{
		Recent.Add(History[i]);
	}

	return Recent;
}

FString FScriptExecutionManager::FormatHistoryForContext(int32 Count) const
{
	TArray<FScriptHistoryEntry> Recent = GetRecentScripts(Count);

	if (Recent.Num() == 0)
	{
		return TEXT("");
	}

	FString Context = TEXT("## Recent Script Executions:\n");

	for (int32 i = 0; i < Recent.Num(); ++i)
	{
		const FScriptHistoryEntry& Entry = Recent[i];
		Context += FString::Printf(
			TEXT("%d. [%s] %s - \"%s\" %s\n"),
			i + 1,
			*ScriptTypeToString(Entry.ScriptType).ToUpper(),
			*Entry.Filename,
			*Entry.Description.Left(50),
			Entry.bSuccess ? TEXT("✓") : TEXT("✗")
		);
	}

	return Context;
}

void FScriptExecutionManager::ClearHistory()
{
	History.Empty();
	SaveHistory();
}

bool FScriptExecutionManager::SaveHistory()
{
	FString HistoryPath = GetHistoryFilePath();
	FString SaveDir = FPaths::GetPath(HistoryPath);

	if (!IFileManager::Get().DirectoryExists(*SaveDir))
	{
		IFileManager::Get().MakeDirectory(*SaveDir, true);
	}

	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> ScriptsArray;
	for (const FScriptHistoryEntry& Entry : History)
	{
		ScriptsArray.Add(MakeShared<FJsonValueObject>(Entry.ToJson()));
	}

	RootObject->SetArrayField(TEXT("scripts"), ScriptsArray);
	RootObject->SetStringField(TEXT("last_updated"), FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")));

	FString JsonString = FJsonUtils::Stringify(RootObject, true);

	if (FFileHelper::SaveStringToFile(JsonString, *HistoryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("Script history saved: %d entries"), History.Num());
		return true;
	}

	UE_LOG(LogUnrealClaude, Error, TEXT("Failed to save script history to: %s"), *HistoryPath);
	return false;
}

bool FScriptExecutionManager::LoadHistory()
{
	FString HistoryPath = GetHistoryFilePath();

	if (!IFileManager::Get().FileExists(*HistoryPath))
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("No script history file found"));
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *HistoryPath))
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to load script history from: %s"), *HistoryPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject = FJsonUtils::Parse(JsonString);
	if (!RootObject.IsValid())
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to parse script history JSON"));
		return false;
	}

	History.Empty();

	TArray<TSharedPtr<FJsonValue>> ScriptsArray;
	if (FJsonUtils::GetArrayField(RootObject, TEXT("scripts"), ScriptsArray))
	{
		for (const TSharedPtr<FJsonValue>& ScriptValue : ScriptsArray)
		{
			const TSharedPtr<FJsonObject>* ScriptObject = nullptr;
			if (ScriptValue->TryGetObject(ScriptObject) && ScriptObject != nullptr && (*ScriptObject).IsValid())
			{
				History.Add(FScriptHistoryEntry::FromJson(*ScriptObject));
			}
		}
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Loaded script history: %d entries"), History.Num());
	return true;
}

FString FScriptExecutionManager::CleanupAll()
{
	int32 DeletedFiles = 0;
	TArray<FString> Errors;

	// Delete C++ scripts
	FString CppDir = GetCppScriptDirectory();
	if (IFileManager::Get().DirectoryExists(*CppDir))
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *CppDir, TEXT("*.*"), true, false);

		for (const FString& File : Files)
		{
			if (IFileManager::Get().Delete(*File))
			{
				DeletedFiles++;
			}
		}

		IFileManager::Get().DeleteDirectory(*CppDir, false, true);
	}

	// Delete content scripts
	FString ContentDir = GetContentScriptDirectory();
	if (IFileManager::Get().DirectoryExists(*ContentDir))
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *ContentDir, TEXT("*.*"), true, false);

		for (const FString& File : Files)
		{
			if (IFileManager::Get().Delete(*File))
			{
				DeletedFiles++;
			}
		}

		IFileManager::Get().DeleteDirectory(*ContentDir, false, true);
	}

	// Clear history
	int32 HistoryCount = History.Num();
	ClearHistory();

	return FString::Printf(TEXT("Cleanup complete: Deleted %d files, cleared %d history entries"), DeletedFiles, HistoryCount);
}

FString FScriptExecutionManager::GetHistoryFilePath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealClaude"), TEXT("script_history.json"));
}

FString FScriptExecutionManager::GetCppScriptDirectory() const
{
	// Source/[ProjectName]/Generated/UnrealClaude/
	FString ProjectName = FApp::GetProjectName();
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), ProjectName, TEXT("Generated"), TEXT("UnrealClaude"));
}

FString FScriptExecutionManager::GetContentScriptDirectory() const
{
	// Content/UnrealClaude/Scripts/
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UnrealClaude"), TEXT("Scripts"));
}
