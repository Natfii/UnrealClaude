// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Spawn an actor in the current level
 */
class FMCPTool_SpawnActor : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("spawn_actor");
		Info.Description = TEXT("Spawn an actor of the specified class in the current level");
		Info.Parameters = {
			FMCPToolParameter(TEXT("class"), TEXT("string"), TEXT("The class path to spawn (e.g., '/Script/Engine.PointLight' or 'StaticMeshActor')"), true),
			FMCPToolParameter(TEXT("name"), TEXT("string"), TEXT("Optional name for the spawned actor"), false),
			FMCPToolParameter(TEXT("location"), TEXT("object"), TEXT("Spawn location {x, y, z}"), false, TEXT("{\"x\":0,\"y\":0,\"z\":0}")),
			FMCPToolParameter(TEXT("rotation"), TEXT("object"), TEXT("Spawn rotation {pitch, yaw, roll}"), false, TEXT("{\"pitch\":0,\"yaw\":0,\"roll\":0}")),
			FMCPToolParameter(TEXT("scale"), TEXT("object"), TEXT("Spawn scale {x, y, z}"), false, TEXT("{\"x\":1,\"y\":1,\"z\":1}"))
		};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
