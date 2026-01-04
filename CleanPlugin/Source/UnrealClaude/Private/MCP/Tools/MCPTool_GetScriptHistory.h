// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolRegistry.h"

/**
 * MCP Tool for getting script execution history
 * Returns recent script executions with descriptions (not full code)
 */
class FMCPTool_GetScriptHistory : public IMCPTool
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("get_script_history");
		Info.Description = TEXT(
			"Get recent script execution history. Returns script type, filename, "
			"description (from header comment), and success/failure status."
		);
		Info.Parameters = {
			FMCPToolParameter(
				TEXT("count"),
				TEXT("number"),
				TEXT("Number of recent scripts to return (default: 10, max: 50)"),
				false,
				TEXT("10")
			)
		};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
