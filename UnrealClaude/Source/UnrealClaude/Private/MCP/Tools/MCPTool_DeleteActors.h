// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Delete actors from the level
 */
class FMCPTool_DeleteActors : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("delete_actors");
		Info.Description = TEXT(
			"Delete actors from the current level. WARNING: This is destructive and cannot be undone via MCP.\n\n"
			"Deletion modes (use one):\n"
			"- actor_name: Delete a single actor by name\n"
			"- actor_names: Delete multiple actors by name array\n"
			"- class_filter: Delete ALL actors of a specific class (use with caution!)\n\n"
			"Best practice: Use get_level_actors first to verify which actors will be deleted.\n\n"
			"Returns: List of deleted actor names and count."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("actor_names"), TEXT("array"), TEXT("Array of actor names to delete"), false),
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"), TEXT("Single actor name to delete (alternative to actor_names)"), false),
			FMCPToolParameter(TEXT("class_filter"), TEXT("string"), TEXT("Delete all actors matching this class name"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Destructive();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
