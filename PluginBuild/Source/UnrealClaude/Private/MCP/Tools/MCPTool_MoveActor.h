// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolRegistry.h"

/**
 * MCP Tool: Move/transform an actor
 */
class FMCPTool_MoveActor : public IMCPTool
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("move_actor");
		Info.Description = TEXT("Move, rotate, or scale an actor");
		Info.Parameters = {
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"), TEXT("The name of the actor to transform"), true),
			FMCPToolParameter(TEXT("location"), TEXT("object"), TEXT("New location {x, y, z}. Use null to keep current."), false),
			FMCPToolParameter(TEXT("rotation"), TEXT("object"), TEXT("New rotation {pitch, yaw, roll}. Use null to keep current."), false),
			FMCPToolParameter(TEXT("scale"), TEXT("object"), TEXT("New scale {x, y, z}. Use null to keep current."), false),
			FMCPToolParameter(TEXT("relative"), TEXT("boolean"), TEXT("If true, values are added to current transform instead of replacing"), false, TEXT("false"))
		};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
