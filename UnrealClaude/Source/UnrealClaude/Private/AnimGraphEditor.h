// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimBlueprint.h"
#include "Dom/JsonObject.h"
#include "AnimNodePinUtils.h"  // For FPinSearchConfig

// Forward declarations
class UAnimGraphNode_StateMachine;
class UAnimGraphNode_Root;
class UAnimStateNode;
class UAnimStateTransitionNode;
class UEdGraph;
class UEdGraphNode;
class UAnimSequence;
class UBlendSpace;
class UBlendSpace1D;
class UAnimMontage;

/**
 * Animation Graph Editor for Animation Blueprints
 *
 * Responsibilities:
 * - Finding animation graphs (AnimGraph, state bound graphs, transition graphs)
 * - Creating animation-specific nodes
 * - Managing node connections in animation graphs
 * - Handling transition condition nodes
 *
 * Supported Condition Node Types:
 * - TimeRemaining: Check time remaining in current state
 * - CompareFloat: Float comparison (requires "comparison" param: Greater, Less, etc.)
 * - CompareBool: Boolean equality comparison
 * - Greater, Less, GreaterEqual, LessEqual, Equal, NotEqual: Direct comparison operators
 * - And, Or, Not: Logical operators
 */
class FAnimGraphEditor
{
public:
	// ===== Graph Finding =====

	/**
	 * Find AnimGraph (main animation graph)
	 */
	static UEdGraph* FindAnimGraph(UAnimBlueprint* AnimBP, FString& OutError);

	/**
	 * Find state bound graph by state name
	 */
	static UEdGraph* FindStateBoundGraph(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		FString& OutError
	);

	/**
	 * Find transition graph by states
	 */
	static UEdGraph* FindTransitionGraph(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		FString& OutError
	);

	// ===== Transition Condition Nodes (Level 4) =====

	/**
	 * Add condition node to transition graph
	 *
	 * Supported NodeTypes:
	 * - "TimeRemaining" - params: { threshold }
	 * - "CompareFloat" - params: { variable_name, comparison: "Greater"|"Less"|"Equal", value }
	 * - "CompareBool" - params: { variable_name, expected_value }
	 * - "And" / "Or" / "Not" - logical operators
	 *
	 * @param TransitionGraph - Transition rule graph
	 * @param NodeType - Type of condition node
	 * @param Params - Node parameters
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @param OutNodeId - Generated node ID
	 * @param OutError - Error message if failed
	 * @return Created node or nullptr
	 */
	static UEdGraphNode* CreateTransitionConditionNode(
		UEdGraph* TransitionGraph,
		const FString& NodeType,
		const TSharedPtr<FJsonObject>& Params,
		int32 PosX,
		int32 PosY,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Connect transition condition nodes
	 */
	static bool ConnectTransitionNodes(
		UEdGraph* TransitionGraph,
		const FString& SourceNodeId,
		const FString& SourcePinName,
		const FString& TargetNodeId,
		const FString& TargetPinName,
		FString& OutError
	);

	/**
	 * Connect condition result to transition result
	 */
	static bool ConnectToTransitionResult(
		UEdGraph* TransitionGraph,
		const FString& ConditionNodeId,
		const FString& ConditionPinName,
		FString& OutError
	);

	// ===== Animation Asset Nodes (Level 5) =====

	/**
	 * Add animation sequence player node to state graph
	 */
	static UEdGraphNode* CreateAnimSequenceNode(
		UEdGraph* StateGraph,
		UAnimSequence* AnimSequence,
		FVector2D Position,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Add BlendSpace player node to state graph (2D)
	 */
	static UEdGraphNode* CreateBlendSpaceNode(
		UEdGraph* StateGraph,
		UBlendSpace* BlendSpace,
		FVector2D Position,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Add BlendSpace1D player node to state graph
	 */
	static UEdGraphNode* CreateBlendSpace1DNode(
		UEdGraph* StateGraph,
		UBlendSpace1D* BlendSpace,
		FVector2D Position,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Connect animation node to state output pose
	 */
	static bool ConnectToOutputPose(
		UEdGraph* StateGraph,
		const FString& AnimNodeId,
		FString& OutError
	);

	/**
	 * Clear all nodes from state graph (except result)
	 */
	static bool ClearStateGraph(UEdGraph* StateGraph, FString& OutError);

	// ===== AnimGraph Root Connection (Level 5) =====

	/**
	 * Connect a State Machine node's output to the AnimGraph's root result
	 * This is used to wire the State Machine into the animation output
	 *
	 * @param AnimBP - The Animation Blueprint
	 * @param StateMachineName - Name of the State Machine node to connect
	 * @param OutError - Error message if failed
	 * @return True if connection succeeded
	 */
	static bool ConnectStateMachineToAnimGraphRoot(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		FString& OutError
	);

	/**
	 * Find the AnimGraph root node (Output Pose)
	 */
	static UAnimGraphNode_Root* FindAnimGraphRoot(UAnimBlueprint* AnimBP, FString& OutError);

	// ===== Node Finding =====

	/**
	 * Find node by ID in graph
	 */
	static UEdGraphNode* FindNodeById(UEdGraph* Graph, const FString& NodeId);

	/**
	 * Find result/output node in graph
	 */
	static UEdGraphNode* FindResultNode(UEdGraph* Graph);

	/**
	 * Find pin on node by name
	 */
	static UEdGraphPin* FindPinByName(
		UEdGraphNode* Node,
		const FString& PinName,
		EEdGraphPinDirection Direction = EGPD_MAX
	);

	/**
	 * Find pin using configuration with multiple fallback strategies
	 * (Delegates to FAnimNodePinUtils)
	 *
	 * @param Node - Node to search
	 * @param Config - Search configuration (see FPinSearchConfig in AnimNodePinUtils.h)
	 * @param OutError - Optional: If provided and no pin found, populated with available pins list
	 * @return Found pin or nullptr
	 */
	static UEdGraphPin* FindPinWithFallbacks(
		UEdGraphNode* Node,
		const FPinSearchConfig& Config,
		FString* OutError = nullptr
	);

	/**
	 * Build error message listing available pins on a node
	 */
	static FString BuildAvailablePinsError(
		UEdGraphNode* Node,
		EEdGraphPinDirection Direction,
		const FString& Context
	);

	// ===== Node ID System =====

	/**
	 * Generate animation-specific node ID
	 */
	static FString GenerateAnimNodeId(
		const FString& NodeType,
		const FString& Context,
		UEdGraph* Graph
	);

	/**
	 * Set node ID
	 */
	static void SetNodeId(UEdGraphNode* Node, const FString& NodeId);

	/**
	 * Get node ID
	 */
	static FString GetNodeId(UEdGraphNode* Node);

	// ===== Serialization =====

	static TSharedPtr<FJsonObject> SerializeAnimNodeInfo(UEdGraphNode* Node);

	/**
	 * Serialize detailed pin information (for inspect_node_pins)
	 * Includes pin type, sub-category, default value, connected pins, etc.
	 */
	static TSharedPtr<FJsonObject> SerializeDetailedPinInfo(UEdGraphPin* Pin);

	// ===== Transition Graph Node Operations (for new MCP tools) =====

	/**
	 * Get all nodes in a transition graph with detailed pin information
	 * Used by get_transition_nodes operation
	 */
	static TSharedPtr<FJsonObject> GetTransitionGraphNodes(
		UEdGraph* TransitionGraph,
		FString& OutError
	);

	/**
	 * Get all transition nodes for a state machine (all transitions)
	 * Used by get_transition_nodes with state_machine parameter only
	 */
	static TSharedPtr<FJsonObject> GetAllTransitionNodes(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		FString& OutError
	);

	/**
	 * Set pin default value with type validation
	 * Returns error if value type doesn't match pin type
	 */
	static bool SetPinDefaultValueWithValidation(
		UEdGraph* Graph,
		const FString& NodeId,
		const FString& PinName,
		const FString& Value,
		FString& OutError
	);

	/**
	 * Validate if a value is compatible with a pin type
	 */
	static bool ValidatePinValueType(
		UEdGraphPin* Pin,
		const FString& Value,
		FString& OutError
	);

	/**
	 * Create a comparison chain: GetVariable → Comparison → Result
	 * Auto-chains with AND to existing logic if present
	 */
	static TSharedPtr<FJsonObject> CreateComparisonChain(
		UAnimBlueprint* AnimBP,
		UEdGraph* TransitionGraph,
		const FString& VariableName,
		const FString& ComparisonType,
		const FString& CompareValue,
		FVector2D Position,
		FString& OutError
	);

private:
	// Thread-safe counter for unique IDs
	static volatile int32 NodeIdCounter;

	// Node ID prefix
	static const FString NodeIdPrefix;

	// Internal node creation helpers
	static UEdGraphNode* CreateTimeRemainingNode(UEdGraph* Graph,
		const TSharedPtr<FJsonObject>& Params, FVector2D Position, FString& OutError);
	static UEdGraphNode* CreateComparisonNode(UEdGraph* Graph, const FString& ComparisonType,
		const TSharedPtr<FJsonObject>& Params, FVector2D Position, FString& OutError, bool bIsBooleanType = false);
	static UEdGraphNode* CreateLogicNode(UEdGraph* Graph, const FString& LogicType,
		FVector2D Position, FString& OutError);
	static UEdGraphNode* CreateVariableGetNode(UEdGraph* Graph, UAnimBlueprint* AnimBP,
		const FString& VariableName, FVector2D Position, FString& OutError);
};
