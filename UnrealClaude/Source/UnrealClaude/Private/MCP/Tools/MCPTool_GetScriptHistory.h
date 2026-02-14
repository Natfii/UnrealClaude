// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool for getting script execution history
 * Returns recent script executions with descriptions (not full code)
 */
class FMCPTool_GetScriptHistory : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("get_script_history");
		Info.Description = TEXT(
			"Retrieve history of previously executed scripts.\n\n"
			"Use this to understand what scripts have been run in this session. "
			"Helpful for context restoration and debugging.\n\n"
			"Returns for each script:\n"
			"- Script type (cpp, python, console, editor_utility)\n"
			"- Filename (for file-based scripts)\n"
			"- Description (from @Description header)\n"
			"- Execution timestamp\n"
			"- Success/failure status\n"
			"- Error message (if failed)\n\n"
			"Note: Full script content is NOT returned for security - only metadata."
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
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
