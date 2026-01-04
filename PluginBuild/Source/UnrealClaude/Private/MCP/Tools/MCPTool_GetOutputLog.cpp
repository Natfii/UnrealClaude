// Copyright Your Name. All Rights Reserved.

#include "MCPTool_GetOutputLog.h"
#include "UnrealClaudeModule.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

FMCPToolResult FMCPTool_GetOutputLog::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Get parameters
	int32 NumLines = 100;
	if (Params->HasField(TEXT("lines")))
	{
		NumLines = FMath::Clamp(static_cast<int32>(Params->GetNumberField(TEXT("lines"))), 1, 1000);
	}

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	// Get the current log file path
	FString LogFilePath = FPaths::ProjectLogDir() / FApp::GetProjectName() + TEXT(".log");

	// If project log doesn't exist, try the default log location
	if (!FPaths::FileExists(LogFilePath))
	{
		LogFilePath = FPaths::ProjectLogDir() / TEXT("UnrealEditor.log");
	}

	if (!FPaths::FileExists(LogFilePath))
	{
		// Try to find any .log file in the log directory
		TArray<FString> LogFiles;
		IFileManager::Get().FindFiles(LogFiles, *FPaths::ProjectLogDir(), TEXT("*.log"));

		if (LogFiles.Num() > 0)
		{
			LogFilePath = FPaths::ProjectLogDir() / LogFiles[0];
		}
		else
		{
			return FMCPToolResult::Error(TEXT("No log file found"));
		}
	}

	// Read the log file
	FString LogContent;
	if (!FFileHelper::LoadFileToString(LogContent, *LogFilePath))
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
