// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Animation Blueprint Modification
 *
 * Provides comprehensive control over Animation Blueprints including:
 * - State machine management (create, query)
 * - State operations (add, remove, list)
 * - Transition operations (add, remove, configure duration/priority)
 * - Transition condition graphs (add condition nodes, connect, delete nodes)
 * - Animation assignment (AnimSequence, BlendSpace, BlendSpace1D, Montage)
 *
 * Operations:
 * - get_info: Get AnimBlueprint structure overview
 * - get_state_machine: Get detailed state machine info
 * - create_state_machine: Create new state machine
 * - add_state: Add state to state machine
 * - remove_state: Remove state from state machine
 * - set_entry_state: Set entry state for state machine
 * - add_transition: Create transition between states
 * - remove_transition: Remove transition
 * - set_transition_duration: Set blend duration
 * - set_transition_priority: Set transition priority
 * - add_condition_node: Add node to transition graph (fully supported)
 * - delete_condition_node: Remove node from transition graph
 * - connect_condition_nodes: Connect nodes in transition graph
 * - connect_to_result: Connect condition to transition result (for transitions)
 * - connect_state_machine_to_output: Connect State Machine to AnimGraph Output Pose
 * - set_state_animation: Assign animation to state
 * - find_animations: Search for compatible animation assets
 * - batch: Execute multiple operations atomically
 *
 * NEW Operations (Enhanced Pin/Node Introspection):
 * - get_transition_nodes: List all nodes in transition graph(s) with their pins
 * - inspect_node_pins: Get detailed pin info for a specific node (types, connections)
 * - set_pin_default_value: Set pin default value with type validation
 * - add_comparison_chain: Add GetVariable → Comparison → Result chain (auto-ANDs with existing)
 * - validate_blueprint: Return compile errors with full diagnostics
 *
 * Supported Condition Node Types (add_condition_node):
 * - TimeRemaining: Gets time remaining in current animation (output: float)
 * - Greater: Float comparison A > B (inputs: A, B; output: bool)
 * - Less: Float comparison A < B
 * - GreaterEqual: Float comparison A >= B
 * - LessEqual: Float comparison A <= B
 * - Equal: Float comparison A == B
 * - NotEqual: Float comparison A != B
 * - And: Boolean AND (inputs: A, B; output: bool)
 * - Or: Boolean OR (inputs: A, B; output: bool)
 * - Not: Boolean NOT (input: A; output: bool)
 * - GetVariable: Get blueprint variable (node_params: {variable_name})
 */
class FMCPTool_AnimBlueprintModify : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("anim_blueprint_modify");
		Info.Description = TEXT(
			"Comprehensive Animation Blueprint modification tool.\n\n"
			"State Machine Operations:\n"
			"- 'get_info': Overview of AnimBlueprint structure\n"
			"- 'get_state_machine': Detailed state machine info\n"
			"- 'create_state_machine': Create new state machine\n"
			"- 'add_state', 'remove_state': Manage states\n"
			"- 'set_entry_state': Set entry state for state machine\n"
			"- 'add_transition', 'remove_transition': Manage transitions\n\n"
			"Transition Configuration:\n"
			"- 'set_transition_duration': Set blend duration\n"
			"- 'set_transition_priority': Set evaluation priority\n\n"
			"Condition Graph (transition logic):\n"
			"- 'add_condition_node': Add logic node (TimeRemaining, Greater, Less, And, Or, Not, GetVariable)\n"
			"- 'delete_condition_node', 'connect_condition_nodes', 'connect_to_result'\n\n"
			"Node/Pin Introspection (NEW):\n"
			"- 'get_transition_nodes': List all nodes in transition graph(s) with pins\n"
			"- 'inspect_node_pins': Get detailed pin info for a node (types, values, connections)\n"
			"- 'set_pin_default_value': Set pin value with type validation\n"
			"- 'add_comparison_chain': Add GetVariable->Comparison->Result (auto-ANDs with existing)\n"
			"- 'validate_blueprint': Return compile errors with full diagnostics\n\n"
			"Visualization (NEW):\n"
			"- 'get_state_machine_diagram': Generate ASCII diagram and enhanced JSON for state machine\n\n"
			"Bulk Operations:\n"
			"- 'setup_transition_conditions': Setup conditions for multiple transitions using pattern matching\n\n"
			"AnimGraph Connection:\n"
			"- 'connect_state_machine_to_output': Connect State Machine to AnimGraph Output Pose\n\n"
			"Animation Assignment:\n"
			"- 'set_state_animation': Assign AnimSequence, BlendSpace, BlendSpace1D, or Montage\n"
			"- 'find_animations': Search compatible animation assets\n\n"
			"- 'batch': Execute multiple operations atomically"
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("blueprint_path"), TEXT("string"), TEXT("Path to the Animation Blueprint (e.g., '/Game/Characters/ABP_Character')"), true),
			FMCPToolParameter(TEXT("operation"), TEXT("string"), TEXT("Operation: get_info, get_state_machine, get_state_machine_diagram, create_state_machine, add_state, remove_state, set_entry_state, add_transition, remove_transition, set_transition_duration, set_transition_priority, add_condition_node, delete_condition_node, connect_condition_nodes, connect_to_result, connect_state_machine_to_output, set_state_animation, find_animations, batch, get_transition_nodes, inspect_node_pins, set_pin_default_value, add_comparison_chain, validate_blueprint"), true),
			FMCPToolParameter(TEXT("state_machine"), TEXT("string"), TEXT("State machine name (for state/transition operations)"), false),
			FMCPToolParameter(TEXT("state_name"), TEXT("string"), TEXT("State name (for state operations)"), false),
			FMCPToolParameter(TEXT("from_state"), TEXT("string"), TEXT("Source state name (for transitions)"), false),
			FMCPToolParameter(TEXT("to_state"), TEXT("string"), TEXT("Target state name (for transitions)"), false),
			FMCPToolParameter(TEXT("position"), TEXT("object"), TEXT("Node position {x, y}"), false, TEXT("{\"x\":0,\"y\":0}")),
			FMCPToolParameter(TEXT("is_entry_state"), TEXT("boolean"), TEXT("Whether this state is the entry state"), false, TEXT("false")),
			FMCPToolParameter(TEXT("duration"), TEXT("number"), TEXT("Transition blend duration in seconds"), false),
			FMCPToolParameter(TEXT("priority"), TEXT("number"), TEXT("Transition priority (higher = checked first)"), false),
			FMCPToolParameter(TEXT("node_type"), TEXT("string"), TEXT("Condition node type: TimeRemaining, Greater, Less, GreaterEqual, LessEqual, Equal, NotEqual, And, Or, Not, GetVariable"), false),
			FMCPToolParameter(TEXT("node_params"), TEXT("object"), TEXT("Condition node parameters (e.g., {variable_name} for GetVariable)"), false),
			FMCPToolParameter(TEXT("node_id"), TEXT("string"), TEXT("Node ID for delete_condition_node operation"), false),
			FMCPToolParameter(TEXT("source_node_id"), TEXT("string"), TEXT("Source node ID for connection"), false),
			FMCPToolParameter(TEXT("source_pin"), TEXT("string"), TEXT("Source pin name"), false),
			FMCPToolParameter(TEXT("target_node_id"), TEXT("string"), TEXT("Target node ID for connection"), false),
			FMCPToolParameter(TEXT("target_pin"), TEXT("string"), TEXT("Target pin name"), false),
			FMCPToolParameter(TEXT("animation_type"), TEXT("string"), TEXT("Animation type: sequence, blendspace, blendspace1d, montage"), false),
			FMCPToolParameter(TEXT("animation_path"), TEXT("string"), TEXT("Path to animation asset"), false),
			FMCPToolParameter(TEXT("parameter_bindings"), TEXT("object"), TEXT("BlendSpace parameter bindings {\"X\": \"Speed\", \"Y\": \"Direction\"}"), false),
			FMCPToolParameter(TEXT("search_pattern"), TEXT("string"), TEXT("Animation search pattern (for find_animations)"), false),
			FMCPToolParameter(TEXT("asset_type"), TEXT("string"), TEXT("Asset type filter: AnimSequence, BlendSpace, BlendSpace1D, Montage, All"), false, TEXT("All")),
			FMCPToolParameter(TEXT("operations"), TEXT("array"), TEXT("Array of operations for batch mode"), false),
			// New parameters for enhanced operations
			FMCPToolParameter(TEXT("variable_name"), TEXT("string"), TEXT("Blueprint variable name (for add_comparison_chain)"), false),
			FMCPToolParameter(TEXT("comparison_type"), TEXT("string"), TEXT("Comparison type: Greater, Less, GreaterEqual, LessEqual, Equal, NotEqual (for add_comparison_chain)"), false),
			FMCPToolParameter(TEXT("compare_value"), TEXT("string"), TEXT("Value to compare against (for add_comparison_chain)"), false),
			FMCPToolParameter(TEXT("pin_value"), TEXT("string"), TEXT("Default value for the pin (for set_pin_default_value)"), false),
			FMCPToolParameter(TEXT("pin_name"), TEXT("string"), TEXT("Pin name to set value (for set_pin_default_value)"), false),
			// Bulk operation parameters
			FMCPToolParameter(TEXT("rules"), TEXT("array"), TEXT("Array of condition rules for setup_transition_conditions. Each rule: {match: {from, to}, conditions: [...], logic: 'AND'|'OR'}"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	// Operation handlers
	FMCPToolResult HandleGetInfo(const FString& BlueprintPath);
	FMCPToolResult HandleGetStateMachine(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleCreateStateMachine(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleAddState(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleRemoveState(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleSetEntryState(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleAddTransition(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleRemoveTransition(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleSetTransitionDuration(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleSetTransitionPriority(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleAddConditionNode(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleDeleteConditionNode(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleConnectConditionNodes(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleConnectToResult(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleConnectStateMachineToOutput(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleSetStateAnimation(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleFindAnimations(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleBatch(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);

	// NEW handlers for enhanced operations
	FMCPToolResult HandleGetTransitionNodes(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleInspectNodePins(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleSetPinDefaultValue(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleAddComparisonChain(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleValidateBlueprint(const FString& BlueprintPath);
	FMCPToolResult HandleGetStateMachineDiagram(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);

	// Bulk operation handler
	FMCPToolResult HandleSetupTransitionConditions(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params);

	// Helper to extract position
	FVector2D ExtractPosition(const TSharedRef<FJsonObject>& Params);

	/**
	 * Load animation blueprint or return error result.
	 * Reduces code duplication across 23+ handler methods.
	 *
	 * @param Path Blueprint path to load
	 * @param OutBP Output parameter for loaded blueprint
	 * @return Empty optional on success, error result on failure
	 */
	TOptional<FMCPToolResult> LoadAnimBlueprintOrError(
		const FString& Path,
		UAnimBlueprint*& OutBP);
};
