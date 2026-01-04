// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Get all actors in the current level
 */
class FMCPTool_GetLevelActors : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("get_level_actors");
		Info.Description = TEXT("Get a list of all actors in the current level, optionally filtered by class");
		Info.Parameters = {
			FMCPToolParameter(TEXT("class_filter"), TEXT("string"), TEXT("Optional class name to filter actors (e.g., 'StaticMeshActor', 'PointLight')"), false),
			FMCPToolParameter(TEXT("name_filter"), TEXT("string"), TEXT("Optional substring to filter actors by name"), false),
			FMCPToolParameter(TEXT("include_hidden"), TEXT("boolean"), TEXT("Include hidden actors in results"), false, TEXT("false")),
			FMCPToolParameter(TEXT("limit"), TEXT("number"), TEXT("Maximum number of actors to return (0 = no limit)"), false, TEXT("100"))
		};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
