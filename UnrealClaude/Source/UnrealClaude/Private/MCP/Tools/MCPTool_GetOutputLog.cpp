// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_GetOutputLog.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

FMCPToolResult FMCPTool_GetOutputLog::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Get parameters using centralized constants
	int32 NumLines = UnrealClaudeConstants::MCPServer::DefaultOutputLogLines;
	if (Params->HasField(TEXT("lines")))
	{
		NumLines = FMath::Clamp(static_cast<int32>(Params->GetNumberField(TEXT("lines"))), 1, UnrealClaudeConstants::MCPServer::MaxOutputLogLines);
	}

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	// Resolve all candidate paths to absolute paths for reliable Windows file access
	FString ProjectLogDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir());
	FString EngineLogDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Saved/Logs"));

	// Search for the log file across multiple candidate locations
	FString LogFilePath;
	bool bFound = false;
	TArray<FString> SearchedPaths;

	// 1. Project-named log in project log directory
	{
		FString Candidate = ProjectLogDir / FApp::GetProjectName() + TEXT(".log");
		SearchedPaths.Add(Candidate);
		if (FPaths::FileExists(Candidate))
		{
			LogFilePath = Candidate;
			bFound = true;
		}
	}

	// 2. UnrealEditor.log in project log directory
	if (!bFound)
	{
		FString Candidate = ProjectLogDir / TEXT("UnrealEditor.log");
		SearchedPaths.Add(Candidate);
		if (FPaths::FileExists(Candidate))
		{
			LogFilePath = Candidate;
			bFound = true;
		}
	}

	// 3. Any .log file in project log directory
	if (!bFound)
	{
		TArray<FString> LogFiles;
		IFileManager::Get().FindFiles(LogFiles, *ProjectLogDir, TEXT("*.log"));
		if (LogFiles.Num() > 0)
		{
			LogFilePath = ProjectLogDir / LogFiles[0];
			bFound = true;
		}
	}

	// 4. UnrealEditor.log in engine saved logs
	if (!bFound)
	{
		FString Candidate = EngineLogDir / TEXT("UnrealEditor.log");
		SearchedPaths.Add(Candidate);
		if (FPaths::FileExists(Candidate))
		{
			LogFilePath = Candidate;
			bFound = true;
		}
	}

	// 5. Any .log file in engine saved logs
	if (!bFound)
	{
		TArray<FString> LogFiles;
		IFileManager::Get().FindFiles(LogFiles, *EngineLogDir, TEXT("*.log"));
		if (LogFiles.Num() > 0)
		{
			LogFilePath = EngineLogDir / LogFiles[0];
			bFound = true;
		}
	}

	if (!bFound)
	{
		FString AllPaths = FString::Join(SearchedPaths, TEXT(", "));
		return FMCPToolResult::Error(
			FString::Printf(TEXT("No log file found. Searched paths: %s. Also scanned directories: %s, %s"),
				*AllPaths, *ProjectLogDir, *EngineLogDir));
	}

	// Read the log file with FILEREAD_AllowWrite so we can read while the editor writes to it.
	// Without this flag, Windows file sharing semantics prevent reading the active log.
	FString LogContent;
	if (!FFileHelper::LoadFileToString(LogContent, *LogFilePath, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to read log file: %s"), *LogFilePath));
	}

	// Split into lines
	TArray<FString> AllLines;
	LogContent.ParseIntoArrayLines(AllLines);

	// Filter lines if a filter is specified
	TArray<FString> FilteredLines;
	if (Filter.IsEmpty())
	{
		FilteredLines = AllLines;
	}
	else
	{
		for (const FString& Line : AllLines)
		{
			if (Line.Contains(Filter, ESearchCase::IgnoreCase))
			{
				FilteredLines.Add(Line);
			}
		}
	}

	// Get the last N lines
	int32 StartIndex = FMath::Max(0, FilteredLines.Num() - NumLines);
	TArray<FString> ResultLines;
	for (int32 i = StartIndex; i < FilteredLines.Num(); ++i)
	{
		ResultLines.Add(FilteredLines[i]);
	}

	// Build result
	FString LogOutput = FString::Join(ResultLines, TEXT("\n"));

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("log_file"), LogFilePath);
	ResultData->SetNumberField(TEXT("total_lines"), AllLines.Num());
	ResultData->SetNumberField(TEXT("returned_lines"), ResultLines.Num());
	if (!Filter.IsEmpty())
	{
		ResultData->SetStringField(TEXT("filter"), Filter);
		ResultData->SetNumberField(TEXT("filtered_lines"), FilteredLines.Num());
	}
	ResultData->SetStringField(TEXT("content"), LogOutput);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved %d log lines from %s"), ResultLines.Num(), *FPaths::GetCleanFilename(LogFilePath)),
		ResultData
	);
}
