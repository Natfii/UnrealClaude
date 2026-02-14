// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Get assets that a specific asset depends on
 *
 * Returns all dependencies (hard and soft) that would need to be loaded
 * when loading the specified asset.
 */
class FMCPTool_AssetDependencies : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("asset_dependencies");
		Info.Description = TEXT(
			"Get all assets that a specific asset depends on (its dependencies).\n\n"
			"Use this tool to understand what assets would need to be loaded together "
			"with a target asset. Useful for:\n"
			"- Understanding asset relationships\n"
			"- Checking what assets are bundled together\n"
			"- Finding shared dependencies between assets\n\n"
			"Example asset paths:\n"
			"- '/Game/Blueprints/BP_Player'\n"
			"- '/Game/Characters/Meshes/SK_Character'\n"
			"- '/Game/Materials/M_Ground'\n\n"
			"Returns: Array of dependency asset paths with their dependency type."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("asset_path"), TEXT("string"),
				TEXT("Full asset path (e.g., '/Game/Blueprints/BP_Player')"), true),
			FMCPToolParameter(TEXT("include_soft"), TEXT("boolean"),
				TEXT("Include soft references in addition to hard dependencies (default: true)"), false, TEXT("true")),
			FMCPToolParameter(TEXT("limit"), TEXT("number"),
				TEXT("Maximum results to return (1-1000, default: 25)"), false, TEXT("25")),
			FMCPToolParameter(TEXT("offset"), TEXT("number"),
				TEXT("Number of results to skip for pagination (default: 0)"), false, TEXT("0"))
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
