// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolRegistry.h"

/**
 * MCP Tool for cleaning up generated scripts and history
 * Use before pushing to production to remove all Claude-generated scripts
 */
class FMCPTool_CleanupScripts : public IMCPTool
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("cleanup_scripts");
		Info.Description = TEXT(
			"Remove all Claude-generated scripts and clear script history. "
			"Use this before pushing to production to clean up generated files."
		);
		Info.Parameters = {};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
