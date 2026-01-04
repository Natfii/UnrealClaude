// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Query Blueprint information (read-only operations)
 *
 * Operations:
 *   - list: List all Blueprints in project (with optional filters)
 *   - inspect: Get detailed Blueprint info (variables, functions, parent class)
 *   - get_graph: Get graph information (node count, events)
 */
class FMCPTool_BlueprintQuery : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("blueprint_query");
		Info.Description = TEXT("Query Blueprint information. Operations: 'list' (find Blueprints), 'inspect' (get details), 'get_graph' (graph info)");
		Info.Parameters = {
			// Operation selector
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'list', 'inspect', or 'get_graph'"), true),

			// For 'list' operation
			FMCPToolParameter(TEXT("path_filter"), TEXT("string"),
				TEXT("Path prefix filter (e.g., '/Game/Blueprints/')"), false, TEXT("/Game/")),
			FMCPToolParameter(TEXT("type_filter"), TEXT("string"),
				TEXT("Blueprint type filter: 'Actor', 'Object', 'Widget', 'AnimBlueprint', etc."), false),
			FMCPToolParameter(TEXT("name_filter"), TEXT("string"),
				TEXT("Name substring filter"), false),
			FMCPToolParameter(TEXT("limit"), TEXT("number"),
				TEXT("Maximum results to return"), false, TEXT("100")),

			// For 'inspect' and 'get_graph' operations
			FMCPToolParameter(TEXT("blueprint_path"), TEXT("string"),
				TEXT("Full Blueprint asset path (required for inspect/get_graph)"), false),

			// For 'inspect' operation
			FMCPToolParameter(TEXT("include_variables"), TEXT("boolean"),
				TEXT("Include variable list in inspect result"), false, TEXT("true")),
			FMCPToolParameter(TEXT("include_functions"), TEXT("boolean"),
				TEXT("Include function list in inspect result"), false, TEXT("true")),
			FMCPToolParameter(TEXT("include_graphs"), TEXT("boolean"),
				TEXT("Include graph info in inspect result"), false, TEXT("false"))
		};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	/** List Blueprints matching filters */
	FMCPToolResult ExecuteList(const TSharedRef<FJsonObject>& Params);

	/** Get detailed Blueprint info */
	FMCPToolResult ExecuteInspect(const TSharedRef<FJsonObject>& Params);

	/** Get graph information */
	FMCPToolResult ExecuteGetGraph(const TSharedRef<FJsonObject>& Params);
};
