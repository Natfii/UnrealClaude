// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool for cleaning up generated scripts and history
 * Use before pushing to production to remove all Claude-generated scripts
 */
class FMCPTool_CleanupScripts : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("cleanup_scripts");
		Info.Description = TEXT(
			"Remove all Claude-generated scripts and clear execution history.\n\n"
			"WARNING: This is a destructive operation that permanently deletes generated script files.\n\n"
			"Use this to:\n"
			"- Clean up before committing to version control\n"
			"- Remove temporary scripts after debugging\n"
			"- Reset script history for a fresh session\n\n"
			"Affected locations:\n"
			"- Generated C++ scripts in Source/UnrealClaude/Generated/\n"
			"- Script execution history records\n\n"
			"Returns: Count of deleted files and cleared history entries."
		);
		Info.Parameters = {};
		Info.Annotations = FMCPToolAnnotations::Destructive();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
