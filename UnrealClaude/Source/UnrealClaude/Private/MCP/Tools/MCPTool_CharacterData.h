// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

// Forward declarations
class UCharacterConfigDataAsset;
class UDataTable;
struct FCharacterStatsRow;
class ACharacter;

/**
 * MCP Tool: Character Data Assets
 *
 * Create and manage character configuration DataAssets and stats DataTables.
 * Supports CRUD operations for character configs and stats rows.
 *
 * DataAsset Operations:
 *   - create_character_data: Create new UCharacterConfigDataAsset
 *   - query_character_data: Search character config assets
 *   - get_character_data: Get details of a specific config
 *   - update_character_data: Modify existing config
 *
 * DataTable Operations:
 *   - create_stats_table: Create new stats DataTable
 *   - query_stats_table: Get stats from DataTable
 *   - add_stats_row: Add row to stats table
 *   - update_stats_row: Modify existing row
 *   - remove_stats_row: Delete row from table
 *
 * Application:
 *   - apply_character_data: Apply config to a runtime character
 *
 * Default asset path: /Game/Characters/ (customizable via package_path)
 */
class FMCPTool_CharacterData : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("character_data");
		Info.Description = TEXT(
			"Create and manage character configuration DataAssets and stats DataTables.\n\n"
			"DataAsset Operations:\n"
			"- 'create_character_data': Create new character config asset\n"
			"- 'query_character_data': Search character configs by name/tags\n"
			"- 'get_character_data': Get details of specific config\n"
			"- 'update_character_data': Modify existing config\n\n"
			"DataTable Operations:\n"
			"- 'create_stats_table': Create new stats DataTable\n"
			"- 'query_stats_table': Get rows from stats table\n"
			"- 'add_stats_row': Add new row to table\n"
			"- 'update_stats_row': Modify existing row\n"
			"- 'remove_stats_row': Delete row from table\n\n"
			"Application:\n"
			"- 'apply_character_data': Apply config to runtime character\n\n"
			"Default asset path: /Game/Characters/"
		);
		Info.Parameters = {
			// Operation selector
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation to perform (see description)"), true),

			// Asset paths
			FMCPToolParameter(TEXT("package_path"), TEXT("string"),
				TEXT("Package path for new assets (default: '/Game/Characters')"), false, TEXT("/Game/Characters")),
			FMCPToolParameter(TEXT("asset_name"), TEXT("string"),
				TEXT("Name for new asset (e.g., 'DA_PlayerConfig')"), false),
			FMCPToolParameter(TEXT("asset_path"), TEXT("string"),
				TEXT("Full path to existing asset"), false),
			FMCPToolParameter(TEXT("table_path"), TEXT("string"),
				TEXT("Path to stats DataTable"), false),

			// DataAsset config fields
			FMCPToolParameter(TEXT("config_id"), TEXT("string"),
				TEXT("Unique config identifier"), false),
			FMCPToolParameter(TEXT("display_name"), TEXT("string"),
				TEXT("Display name for config"), false),
			FMCPToolParameter(TEXT("description"), TEXT("string"),
				TEXT("Config description"), false),
			FMCPToolParameter(TEXT("skeletal_mesh"), TEXT("string"),
				TEXT("Path to skeletal mesh asset"), false),
			FMCPToolParameter(TEXT("anim_blueprint"), TEXT("string"),
				TEXT("Path to animation blueprint class"), false),
			FMCPToolParameter(TEXT("is_player_character"), TEXT("boolean"),
				TEXT("Whether this is a player character config"), false),

			// Movement stats (for config)
			FMCPToolParameter(TEXT("base_walk_speed"), TEXT("number"),
				TEXT("Base walking speed (cm/s)"), false),
			FMCPToolParameter(TEXT("base_run_speed"), TEXT("number"),
				TEXT("Base running speed (cm/s)"), false),
			FMCPToolParameter(TEXT("base_jump_velocity"), TEXT("number"),
				TEXT("Base jump velocity (cm/s)"), false),
			FMCPToolParameter(TEXT("base_acceleration"), TEXT("number"),
				TEXT("Base acceleration (cm/s^2)"), false),
			FMCPToolParameter(TEXT("base_ground_friction"), TEXT("number"),
				TEXT("Base ground friction"), false),
			FMCPToolParameter(TEXT("base_air_control"), TEXT("number"),
				TEXT("Base air control (0-1)"), false),
			FMCPToolParameter(TEXT("base_gravity_scale"), TEXT("number"),
				TEXT("Base gravity scale"), false),

			// Combat stats
			FMCPToolParameter(TEXT("base_health"), TEXT("number"),
				TEXT("Base health value"), false),
			FMCPToolParameter(TEXT("base_stamina"), TEXT("number"),
				TEXT("Base stamina value"), false),
			FMCPToolParameter(TEXT("base_damage"), TEXT("number"),
				TEXT("Base damage value"), false),
			FMCPToolParameter(TEXT("base_defense"), TEXT("number"),
				TEXT("Base defense value"), false),

			// Collision
			FMCPToolParameter(TEXT("capsule_radius"), TEXT("number"),
				TEXT("Capsule collision radius"), false),
			FMCPToolParameter(TEXT("capsule_half_height"), TEXT("number"),
				TEXT("Capsule collision half-height"), false),

			// Tags
			FMCPToolParameter(TEXT("gameplay_tags"), TEXT("array"),
				TEXT("Array of gameplay tag names"), false),

			// Stats table row fields
			FMCPToolParameter(TEXT("row_name"), TEXT("string"),
				TEXT("Row name in DataTable"), false),
			FMCPToolParameter(TEXT("stats_id"), TEXT("string"),
				TEXT("Stats row identifier"), false),
			FMCPToolParameter(TEXT("max_health"), TEXT("number"),
				TEXT("Maximum health value"), false),
			FMCPToolParameter(TEXT("max_stamina"), TEXT("number"),
				TEXT("Maximum stamina value"), false),
			FMCPToolParameter(TEXT("walk_speed"), TEXT("number"),
				TEXT("Walk speed for stats row"), false),
			FMCPToolParameter(TEXT("run_speed"), TEXT("number"),
				TEXT("Run speed for stats row"), false),
			FMCPToolParameter(TEXT("jump_velocity"), TEXT("number"),
				TEXT("Jump velocity for stats row"), false),
			FMCPToolParameter(TEXT("damage_multiplier"), TEXT("number"),
				TEXT("Damage multiplier (0-10)"), false),
			FMCPToolParameter(TEXT("defense_multiplier"), TEXT("number"),
				TEXT("Defense multiplier (0-10)"), false),
			FMCPToolParameter(TEXT("xp_multiplier"), TEXT("number"),
				TEXT("XP multiplier (0-10)"), false),
			FMCPToolParameter(TEXT("level"), TEXT("number"),
				TEXT("Character level"), false),
			FMCPToolParameter(TEXT("tags"), TEXT("array"),
				TEXT("Array of tag names for stats row"), false),

			// Query options
			FMCPToolParameter(TEXT("search_name"), TEXT("string"),
				TEXT("Search filter for asset names"), false),
			FMCPToolParameter(TEXT("search_tags"), TEXT("array"),
				TEXT("Filter by gameplay tags"), false),
			FMCPToolParameter(TEXT("limit"), TEXT("number"),
				TEXT("Max results (1-1000, default: 25)"), false, TEXT("25")),
			FMCPToolParameter(TEXT("offset"), TEXT("number"),
				TEXT("Skip first N results"), false, TEXT("0")),

			// For apply operation
			FMCPToolParameter(TEXT("character_name"), TEXT("string"),
				TEXT("Target character actor name/label"), false),
			FMCPToolParameter(TEXT("apply_movement"), TEXT("boolean"),
				TEXT("Apply movement stats to character (default: true)"), false, TEXT("true")),
			FMCPToolParameter(TEXT("apply_mesh"), TEXT("boolean"),
				TEXT("Apply skeletal mesh (default: false)"), false, TEXT("false")),
			FMCPToolParameter(TEXT("apply_anim"), TEXT("boolean"),
				TEXT("Apply animation blueprint (default: false)"), false, TEXT("false"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	// DataAsset operations
	FMCPToolResult ExecuteCreateCharacterData(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteQueryCharacterData(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetCharacterData(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteUpdateCharacterData(const TSharedRef<FJsonObject>& Params);

	// DataTable operations
	FMCPToolResult ExecuteCreateStatsTable(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteQueryStatsTable(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddStatsRow(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteUpdateStatsRow(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteRemoveStatsRow(const TSharedRef<FJsonObject>& Params);

	// Application
	FMCPToolResult ExecuteApplyCharacterData(const TSharedRef<FJsonObject>& Params);

	// Helper methods
	UCharacterConfigDataAsset* LoadCharacterConfig(const FString& Path, FString& OutError);
	UDataTable* LoadStatsTable(const FString& Path, FString& OutError);
	bool SaveAsset(UObject* Asset, FString& OutError);

	// JSON conversion
	TSharedPtr<FJsonObject> ConfigToJson(UCharacterConfigDataAsset* Config);
	TSharedPtr<FJsonObject> StatsRowToJson(const FCharacterStatsRow& Row, const FName& RowName);

	// Config population from params
	void PopulateConfigFromParams(UCharacterConfigDataAsset* Config, const TSharedRef<FJsonObject>& Params);
	void PopulateStatsRowFromParams(FCharacterStatsRow& Row, const TSharedRef<FJsonObject>& Params);
};
