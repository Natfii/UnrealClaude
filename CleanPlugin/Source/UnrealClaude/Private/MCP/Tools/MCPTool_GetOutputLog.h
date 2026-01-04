// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

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
		Info.Description = TEXT("Get recent entries from the Unreal Engine output log");
		Info.Parameters = {
			FMCPToolParameter(TEXT("lines"), TEXT("number"), TEXT("Number of recent lines to return (default: 100, max: 1000)"), false),
			FMCPToolParameter(TEXT("filter"), TEXT("string"), TEXT("Optional category or text filter (e.g., 'Warning', 'Error', 'LogTemp')"), false)
		};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
