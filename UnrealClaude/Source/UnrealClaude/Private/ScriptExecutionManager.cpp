// Copyright Natali Caggiano. All Rights Reserved.

#include "ScriptExecutionManager.h"
#include "ScriptPermissionDialog.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeSettings.h"
#include "UnrealClaudeUtils.h"
#include "JsonUtils.h"
#include "MCP/MCPParamValidator.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Async/TaskGraphInterfaces.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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
	FString FinalDescription = Description;
	if (FinalDescription.IsEmpty())
	{
		FinalDescription = ScriptHeader::ParseDescription(ScriptContent);
	}

	if (!ShowPermissionDialog(ScriptContent, Type, FinalDescription))
	{
		return FScriptExecutionResult::Error(TEXT("Script execution denied by user"));
	}

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
	// Honour the project-level auto-approve setting, intended for trusted MCP/agent-driven workflows
	// where the per-script confirmation is the dominant friction. Default OFF preserves the safe
	// "human in the loop" behaviour. We log every auto-approval so the audit trail survives.
	if (const UUnrealClaudeSettings* Settings = GetDefault<UUnrealClaudeSettings>())
	{
		if (Settings->bAutoApproveScripts)
		{
			UE_LOG(LogUnrealClaude, Log,
				TEXT("Script auto-approved (bAutoApproveScripts=true). Type=%s Description=%s"),
				*ScriptTypeToString(Type), *Description);
			return true;
		}
	}

	return FScriptPermissionDialog::Show(ScriptPreview, Type, Description);
}

FScriptExecutionResult FScriptExecutionManager::ExecuteCpp(
	const FString& ScriptContent,
	const FString& Description)
{
#if WITH_LIVE_CODING
	FString ScriptName = GenerateScriptName(EScriptType::Cpp, Description);
	FString FilePath = WriteScriptFile(ScriptContent, EScriptType::Cpp, ScriptName);

	if (FilePath.IsEmpty())
	{
		return FScriptExecutionResult::Error(TEXT("Failed to write C++ script file"));
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("C++ script written to: %s"), *FilePath);

	FString ErrorLog;
	FScriptExecutionResult Result;

	if (TriggerLiveCodingCompile(ErrorLog))
	{
		Result = FScriptExecutionResult::Success(
			TEXT("C++ script compiled successfully via Live Coding"),
			TEXT("Script file: ") + FilePath
		);

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

	// Returning an error here lets the calling agent re-invoke execute_script with corrected code
	UE_LOG(LogUnrealClaude, Warning, TEXT("C++ compilation failed, returning error for Claude to fix"));

	Result = FScriptExecutionResult::Error(
		TEXT("Compilation failed. Fix these errors and call execute_script again:\n\n") + ErrorLog,
		ErrorLog
	);

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

/**
 * Custom output device to capture Live Coding compilation output
 * Monitors for error patterns in compiler output
 */
class FLiveCodingOutputCapture : public FOutputDevice
{
public:
	TArray<FString> ErrorMessages;
	TArray<FString> WarningMessages;
	bool bHasErrors = false;

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		FString Message(V);

		if (Category == FName("LiveCoding") || Category == FName("LogLiveCoding"))
		{
			if (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Fatal)
			{
				ErrorMessages.Add(Message);
				bHasErrors = true;
			}
			else if (Verbosity == ELogVerbosity::Warning)
			{
				WarningMessages.Add(Message);
			}
		}

		// Catch compiler/linker errors that arrive on other log categories
		if (Message.Contains(TEXT("error C")) ||      // MSVC error
		    Message.Contains(TEXT("error:")) ||        // Generic compiler error
		    Message.Contains(TEXT("fatal error")) ||   // Fatal errors
		    Message.Contains(TEXT("LNK2001")) ||       // Linker errors
		    Message.Contains(TEXT("LNK2019")))
		{
			if (!ErrorMessages.Contains(Message))
			{
				ErrorMessages.Add(Message);
				bHasErrors = true;
			}
		}
	}

	FString GetErrorSummary() const
	{
		if (ErrorMessages.Num() == 0)
		{
			return FString();
		}

		// Cap at 5 errors so the caller doesn't get a multi-page wall of text
		FString Summary;
		int32 MaxErrors = FMath::Min(5, ErrorMessages.Num());
		for (int32 i = 0; i < MaxErrors; i++)
		{
			Summary += ErrorMessages[i] + TEXT("\n");
		}
		if (ErrorMessages.Num() > MaxErrors)
		{
			Summary += FString::Printf(TEXT("... and %d more errors"), ErrorMessages.Num() - MaxErrors);
		}
		return Summary;
	}
};

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

	// Hook GLog before triggering compile so we capture every error/warning event
	FLiveCodingOutputCapture OutputCapture;
	GLog->AddOutputDevice(&OutputCapture);

	LiveCoding->Compile(ELiveCodingCompileFlags::None, nullptr);

	// LiveCoding compiles asynchronously. Never block the game thread with long Sleep()
	// calls — that freezes the editor for up to 60s. On the game thread, pump the
	// task graph between short waits; off the game thread, sleep normally.
	float WaitTime = 0.0f;
	const float MaxWait = 60.0f;
	const bool bOnGameThread = IsInGameThread();
	const float PollInterval = bOnGameThread ? 0.05f : 0.25f;

	while (LiveCoding->IsCompiling() && WaitTime < MaxWait)
	{
		if (bOnGameThread)
		{
			FPlatformProcess::Sleep(PollInterval);
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		}
		else
		{
			FPlatformProcess::Sleep(PollInterval);
		}
		WaitTime += PollInterval;
	}

	GLog->RemoveOutputDevice(&OutputCapture);

	if (WaitTime >= MaxWait)
	{
		OutErrorLog = TEXT("Live Coding compilation timed out (60s)");
		return false;
	}

	if (OutputCapture.bHasErrors)
	{
		OutErrorLog = OutputCapture.GetErrorSummary();
		UE_LOG(LogUnrealClaude, Error, TEXT("Live Coding compilation failed:\n%s"), *OutErrorLog);
		return false;
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Live Coding compilation completed successfully"));
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

	FString ScriptName = GenerateScriptName(EScriptType::Python, Description);
	FString FilePath = WriteScriptFile(ScriptContent, EScriptType::Python, ScriptName);

	if (FilePath.IsEmpty())
	{
		return FScriptExecutionResult::Error(TEXT("Failed to write Python script file"));
	}

	// Snapshot actor count so we can report how many the script created
	int32 ActorCountBefore = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		ActorCountBefore++;
	}

	FString Command = FString::Printf(TEXT("py \"%s\""), *FilePath);

	// Capture both exec output and global log output (Python errors go to GLog, not exec output)
	FUnrealClaudeOutputDevice ExecOutput;
	FUnrealClaudeOutputDevice LogOutput;
	GLog->AddOutputDevice(&LogOutput);

	GEditor->Exec(World, *Command, ExecOutput);

	GLog->RemoveOutputDevice(&LogOutput);

	FString ExecText = ExecOutput.GetTrimmedOutput();
	FString LogText = LogOutput.GetTrimmedOutput();
	FString Output = ExecText;
	if (!LogText.IsEmpty())
	{
		if (!Output.IsEmpty())
		{
			Output += TEXT("\n");
		}
		Output += LogText;
	}

	int32 ActorCountAfter = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		ActorCountAfter++;
	}
	int32 ActorsCreated = ActorCountAfter - ActorCountBefore;

	UE_LOG(LogUnrealClaude, Log, TEXT("Python script output (%d chars): %s"),
		Output.Len(), Output.Len() > 500 ? *(Output.Left(500) + TEXT("...")) : *Output);
	UE_LOG(LogUnrealClaude, Log, TEXT("Python script actor delta: %d before, %d after (%+d)"),
		ActorCountBefore, ActorCountAfter, ActorsCreated);

	// Heuristic scan for Python error markers — Python exceptions don't fail the console exec, so we sniff the text
	bool bHasError = Output.Contains(TEXT("Traceback")) ||
	                 Output.Contains(TEXT("Error:")) ||
	                 Output.Contains(TEXT("SyntaxError")) ||
	                 Output.Contains(TEXT("NameError")) ||
	                 Output.Contains(TEXT("TypeError")) ||
	                 Output.Contains(TEXT("ValueError")) ||
	                 Output.Contains(TEXT("ImportError")) ||
	                 Output.Contains(TEXT("AttributeError")) ||
	                 Output.Contains(TEXT("RuntimeError")) ||
	                 Output.Contains(TEXT("Exception:")) ||
	                 Output.Contains(TEXT("ModuleNotFoundError")) ||
	                 Output.Contains(TEXT("FileNotFoundError")) ||
	                 Output.Contains(TEXT("IndentationError")) ||
	                 Output.Contains(TEXT("KeyError"));

	FString ResultMessage;
	if (bHasError)
	{
		ResultMessage = TEXT("Python script execution failed");
	}
	else
	{
		ResultMessage = FString::Printf(TEXT("Python script executed (actors created: %d)"), ActorsCreated);
	}

	FString FullOutput = Output;
	if (ActorsCreated > 0)
	{
		FullOutput += FString::Printf(TEXT("\n[%d new actors added to level]"), ActorsCreated);
	}
	else if (!bHasError)
	{
		FullOutput += TEXT("\n[WARNING: Script reported success but no new actors were created in the level. The script may have failed silently.]");
	}

	FScriptHistoryEntry Entry;
	Entry.ScriptType = EScriptType::Python;
	Entry.Filename = ScriptName + TEXT(".py");
	Entry.Description = Description;
	Entry.bSuccess = !bHasError;
	Entry.ResultMessage = ResultMessage.Left(200);
	Entry.FilePath = FilePath;
	AddToHistory(Entry);

	if (bHasError)
	{
		return FScriptExecutionResult::Error(ResultMessage, FullOutput);
	}

	return FScriptExecutionResult::Success(ResultMessage, FullOutput);
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

	// One command per line — matches in-editor console behaviour
	TArray<FString> Commands;
	ScriptContent.ParseIntoArrayLines(Commands, true);

	FUnrealClaudeOutputDevice OutputDevice;
	FString AllOutput;
	int32 ExecutedCount = 0;

	for (const FString& RawCommand : Commands)
	{
		FString Command = RawCommand.TrimStartAndEnd();

		if (Command.IsEmpty() || Command.StartsWith(TEXT("#")) || Command.StartsWith(TEXT("//")))
		{
			continue;
		}

		// Skip @UnrealClaude header metadata that scripts ship with
		if (Command.Contains(TEXT("@UnrealClaude")) || Command.Contains(TEXT("@Name:")) ||
			Command.Contains(TEXT("@Description:")) || Command.Contains(TEXT("@Created:")))
		{
			continue;
		}

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
	// Sanitize description for filename: replace Windows-invalid chars with underscores, cap at 30
	FString BaseName;
	BaseName.Reserve(30);

	static const TCHAR* InvalidChars = TEXT(" /\\:*?\"<>|");

	int32 CharCount = 0;
	for (TCHAR C : Description)
	{
		if (CharCount >= 30)
		{
			break;
		}

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

	while (History.Num() > MaxHistorySize)
	{
		History.RemoveAt(0);
	}

	// Persist after every change so history survives editor crashes
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
