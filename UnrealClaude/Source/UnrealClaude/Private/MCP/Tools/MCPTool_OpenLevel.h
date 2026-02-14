// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Open, create, or list level maps in the editor
 *
 * Supports four actions:
 * - "open": Load an existing map by path
 * - "new": Create a new blank map or from a template
 * - "save_as": Save current level to a specified path
 * - "list_templates": List available map templates
 */
class FMCPTool_OpenLevel : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("open_level");
		Info.Description = TEXT(
			"Open, create, save, or list level maps in the Unreal Editor.\n\n"
			"Actions:\n"
			"- 'open': Load an existing map by asset path (e.g., '/Game/Maps/MyLevel')\n"
			"- 'new': Create a new blank map, or from a template if 'template' is specified\n"
			"- 'save_as': Save the current level to a specified path (e.g., '/Game/Maps/MyLevel')\n"
			"- 'list_templates': List all available map templates\n\n"
			"The editor will prompt to save unsaved changes before switching levels.\n\n"
			"Returns: The loaded map name and world info, save result, or template list."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("action"), TEXT("string"),
				TEXT("Action to perform: 'open', 'new', 'save_as', or 'list_templates'"), true),
			FMCPToolParameter(TEXT("level_path"), TEXT("string"),
				TEXT("Asset path of the level to open (required for 'open' action, e.g., '/Game/Maps/MyLevel')"), false),
			FMCPToolParameter(TEXT("template"), TEXT("string"),
				TEXT("Template name for 'new' action (omit for blank map). Use 'list_templates' to see available names."), false),
			FMCPToolParameter(TEXT("save_current"), TEXT("boolean"),
				TEXT("For 'new' action: whether to prompt to save the current level first (default: true). The 'open' action always uses the engine's built-in save prompt."), false, TEXT("true")),
			FMCPToolParameter(TEXT("save_path"), TEXT("string"),
				TEXT("Asset path to save the level to (required for 'save_as' action, e.g., '/Game/Maps/MyLevel')"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	/** Execute the 'open' action */
	FMCPToolResult ExecuteOpen(const TSharedRef<FJsonObject>& Params);

	/** Execute the 'new' action */
	FMCPToolResult ExecuteNew(const TSharedRef<FJsonObject>& Params);

	/** Execute the 'save_as' action */
	FMCPToolResult ExecuteSaveAs(const TSharedRef<FJsonObject>& Params);

	/** Execute the 'list_templates' action */
	FMCPToolResult ExecuteListTemplates();

	/** Validate a level asset path for safety */
	static bool ValidateLevelPath(const FString& Path, FString& OutError);
};
