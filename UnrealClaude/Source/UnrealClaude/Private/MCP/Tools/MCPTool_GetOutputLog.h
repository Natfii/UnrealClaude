// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Get the Unreal Engine output log
 */
class FMCPTool_GetOutputLog : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("get_output_log");
		Info.Description = TEXT(
			"Retrieve recent entries from the Unreal Engine output log.\n\n"
			"Essential for debugging and monitoring engine activity. Use filters to focus on specific issues.\n\n"
			"Common filters:\n"
			"- 'Error' - Show only errors\n"
			"- 'Warning' - Show warnings\n"
			"- 'LogTemp' - Show UE_LOG(LogTemp, ...) output\n"
			"- 'LogBlueprint' - Blueprint-related messages\n"
			"- 'LogScript' - Script compilation messages\n"
			"- 'LogActor' - Actor lifecycle messages\n\n"
			"Returns: Array of log entries with timestamp, category, verbosity, and message."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("lines"), TEXT("number"), TEXT("Number of recent lines to return (default: 100, max: 1000)"), false, TEXT("100")),
			FMCPToolParameter(TEXT("filter"), TEXT("string"), TEXT("Optional category or text filter (e.g., 'Warning', 'Error', 'LogTemp')"), false)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
