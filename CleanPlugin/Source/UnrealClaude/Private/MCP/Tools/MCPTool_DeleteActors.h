// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

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
		Info.Description = TEXT("Delete one or more actors from the current level");
		Info.Parameters = {
			FMCPToolParameter(TEXT("actor_names"), TEXT("array"), TEXT("Array of actor names to delete"), false),
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"), TEXT("Single actor name to delete (alternative to actor_names)"), false),
			FMCPToolParameter(TEXT("class_filter"), TEXT("string"), TEXT("Delete all actors matching this class name"), false)
		};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
