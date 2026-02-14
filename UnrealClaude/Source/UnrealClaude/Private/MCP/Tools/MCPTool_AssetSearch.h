// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Search for assets in the Unreal project
 *
 * Finds assets by class type, path prefix, or name pattern.
 * Returns full asset paths with metadata for LLM consumption.
 */
class FMCPTool_AssetSearch : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("asset_search");
		Info.Description = TEXT(
			"PREFERRED: Use this tool instead of file-system searches to find project assets.\n\n"
			"Search for assets in the Unreal project by class, path, or name. "
			"All filters are optional and combine with AND logic.\n\n"
			"Filter examples:\n"
			"- class_filter='Blueprint' - Find all blueprints\n"
			"- class_filter='StaticMesh', path_filter='/Game/Environment/' - Static meshes in folder\n"
			"- name_pattern='Player' - Assets with 'Player' in name\n"
			"- path_filter='/Game/Characters/', name_pattern='Enemy' - Combined filters\n\n"
			"Common class types: Blueprint, StaticMesh, SkeletalMesh, Texture2D, Material, "
			"MaterialInstance, AnimSequence, AnimBlueprint, SoundWave, ParticleSystem, NiagaraSystem\n\n"
			"Returns: Array of assets with path, name, class, and package_path. "
			"Use limit/offset for pagination on large result sets."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("class_filter"), TEXT("string"),
				TEXT("Asset class to filter by (e.g., 'Blueprint', 'StaticMesh', 'Texture2D')"), false),
			FMCPToolParameter(TEXT("path_filter"), TEXT("string"),
				TEXT("Path prefix to search within (e.g., '/Game/Characters/'). Searches recursively. Default: '/Game/'"), false, TEXT("/Game/")),
			FMCPToolParameter(TEXT("name_pattern"), TEXT("string"),
				TEXT("Substring to match in asset names (case-insensitive)"), false),
			FMCPToolParameter(TEXT("limit"), TEXT("number"),
				TEXT("Maximum results to return (1-1000, default: 25)"), false, TEXT("25")),
			FMCPToolParameter(TEXT("offset"), TEXT("number"),
				TEXT("Number of results to skip for pagination (default: 0)"), false, TEXT("0"))
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	/** Convert FAssetData to JSON object with full path information */
	TSharedPtr<FJsonObject> AssetDataToJson(const FAssetData& AssetData) const;
};
