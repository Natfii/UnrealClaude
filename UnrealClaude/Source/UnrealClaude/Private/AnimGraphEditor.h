// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimBlueprint.h"
#include "Dom/JsonObject.h"

// Forward declarations
class UAnimGraphNode_StateMachine;
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
 * - CompareFloat: Compare float variable (Speed, Direction, etc.)
 * - CompareBool: Compare boolean variable (IsJumping, IsFalling, etc.)
 * - And, Or, Not: Logical operators
 */
class UNREALCLAUDE_API FAnimGraphEditor
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

private:
	// Thread-safe counter for unique IDs
	static volatile int32 NodeIdCounter;

	// Node ID prefix
	static const FString NodeIdPrefix;

	// Internal node creation helpers
	static UEdGraphNode* CreateTimeRemainingNode(UEdGraph* Graph,
		const TSharedPtr<FJsonObject>& Params, FVector2D Position, FString& OutError);
	static UEdGraphNode* CreateComparisonNode(UEdGraph* Graph, const FString& ComparisonType,
		const TSharedPtr<FJsonObject>& Params, FVector2D Position, FString& OutError);
	static UEdGraphNode* CreateLogicNode(UEdGraph* Graph, const FString& LogicType,
		FVector2D Position, FString& OutError);
	static UEdGraphNode* CreateVariableGetNode(UEdGraph* Graph, UAnimBlueprint* AnimBP,
		const FString& VariableName, FVector2D Position, FString& OutError);
};
