// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Move/transform an actor
 */
class FMCPTool_MoveActor : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("move_actor");
		Info.Description = TEXT(
			"Transform an actor's location, rotation, and/or scale.\n\n"
			"Supports both absolute positioning (relative=false) and incremental changes (relative=true).\n"
			"Only specify the transform components you want to change - others remain unchanged.\n\n"
			"Examples:\n"
			"- Move to position: location={x:100, y:200, z:0}\n"
			"- Rotate 90 degrees: rotation={yaw:90}, relative=true\n"
			"- Scale up 2x: scale={x:2, y:2, z:2}\n"
			"- Move forward 50 units: location={x:50}, relative=true\n\n"
			"Returns: Actor's new transform (location, rotation, scale)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"), TEXT("The name of the actor to transform"), true),
			FMCPToolParameter(TEXT("location"), TEXT("object"), TEXT("New location {x, y, z}. Omit to keep current."), false),
			FMCPToolParameter(TEXT("rotation"), TEXT("object"), TEXT("New rotation {pitch, yaw, roll}. Omit to keep current."), false),
			FMCPToolParameter(TEXT("scale"), TEXT("object"), TEXT("New scale {x, y, z}. Omit to keep current."), false),
			FMCPToolParameter(TEXT("relative"), TEXT("boolean"), TEXT("If true, values are added to current transform instead of replacing"), false, TEXT("false"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
