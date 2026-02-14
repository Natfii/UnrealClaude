// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Get assets that reference a specific asset
 *
 * Returns all assets that have dependencies on the specified asset.
 * Useful for impact analysis before modifying or deleting an asset.
 */
class FMCPTool_AssetReferencers : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("asset_referencers");
		Info.Description = TEXT(
			"Get all assets that reference a specific asset (its referencers).\n\n"
			"Use this tool to find what would be affected if you modify or delete an asset. "
			"Essential for impact analysis before making changes.\n\n"
			"Common use cases:\n"
			"- Find all materials using a specific texture\n"
			"- Find all blueprints using a specific mesh\n"
			"- Check if an asset is safe to delete\n"
			"- Understand how assets are connected\n\n"
			"Example asset paths:\n"
			"- '/Game/Textures/T_Icon' - Find what uses this texture\n"
			"- '/Game/Meshes/SM_Rock' - Find what uses this mesh\n"
			"- '/Game/Materials/M_Ground' - Find what uses this material\n\n"
			"Returns: Array of referencer asset paths with their class type."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("asset_path"), TEXT("string"),
				TEXT("Full asset path to find referencers for (e.g., '/Game/Textures/T_Icon')"), true),
			FMCPToolParameter(TEXT("include_soft"), TEXT("boolean"),
				TEXT("Include soft references in addition to hard references (default: true)"), false, TEXT("true")),
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
