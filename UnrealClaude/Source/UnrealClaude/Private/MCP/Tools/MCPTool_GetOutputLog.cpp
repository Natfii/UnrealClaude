// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_GetOutputLog.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

FMCPToolResult FMCPTool_GetOutputLog::Execute(const TSharedRef<FJsonObject>& Params)
{
	int32 NumLines = UnrealClaudeConstants::MCPServer::DefaultOutputLogLines;
	if (Params->HasField(TEXT("lines")))
	{
		NumLines = FMath::Clamp(static_cast<int32>(Params->GetNumberField(TEXT("lines"))), 1, UnrealClaudeConstants::MCPServer::MaxOutputLogLines);
	}

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	// Absolute paths required — relative paths fail intermittently under Windows file sharing
	FString ProjectLogDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir());
	FString EngineLogDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Saved/Logs"));

	// Try most-specific to least-specific log locations
	FString LogFilePath;
	bool bFound = false;
	TArray<FString> SearchedPaths;

	{
		FString Candidate = ProjectLogDir / FApp::GetProjectName() + TEXT(".log");
		SearchedPaths.Add(Candidate);
		if (FPaths::FileExists(Candidate))
		{
			LogFilePath = Candidate;
			bFound = true;
		}
	}

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

	TArray<FString> AllLines;
	LogContent.ParseIntoArrayLines(AllLines);

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

	int32 StartIndex = FMath::Max(0, FilteredLines.Num() - NumLines);
	TArray<FString> ResultLines;
	for (int32 i = StartIndex; i < FilteredLines.Num(); ++i)
	{
		ResultLines.Add(FilteredLines[i]);
	}

	FString LogOutput = FString::Join(ResultLines, TEXT("\n"));
	bool bTruncated = false;
	if (LogOutput.Len() > UnrealClaudeConstants::MCPServer::MaxOutputLogContentSize)
	{
		LogOutput.LeftInline(UnrealClaudeConstants::MCPServer::MaxOutputLogContentSize);
		bTruncated = true;
	}

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
	if (bTruncated)
	{
		ResultData->SetBoolField(TEXT("truncated"), true);
		ResultData->SetNumberField(TEXT("max_content_characters"), UnrealClaudeConstants::MCPServer::MaxOutputLogContentSize);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved %d log lines from %s"), ResultLines.Num(), *FPaths::GetCleanFilename(LogFilePath)),
		ResultData
	);
}
