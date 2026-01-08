// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UAnimBlueprint;
class UEdGraph;
class UEdGraphNode;

/**
 * Type of comparison pin for creating type-appropriate comparison nodes
 */
enum class EComparisonPinType : uint8
{
	Float,		// Double/float comparison
	Integer,	// Integer comparison
	Boolean,	// Boolean comparison
	Byte,		// Byte comparison
	Enum		// Enum comparison (uses byte comparison)
};

/**
 * AnimTransitionConditionFactory - Factory for creating transition condition nodes
 *
 * Responsibilities:
 * - Creating condition nodes (TimeRemaining, comparisons, logic)
 * - Managing transition node connections
 * - Creating comparison chains
 *
 * Supported Condition Node Types:
 * - TimeRemaining: Check time remaining in current state
 * - CompareFloat: Float comparison (requires "comparison" param)
 * - CompareBool: Boolean equality comparison
 * - Greater, Less, GreaterEqual, LessEqual, Equal, NotEqual: Direct comparisons
 * - And, Or, Not: Logical operators
 */
class UNREALCLAUDE_API FAnimTransitionConditionFactory
{
public:
	/**
	 * Create a transition condition node
	 *
	 * @param TransitionGraph Transition rule graph
	 * @param NodeType Type of condition node
	 * @param Params Node parameters
	 * @param PosX X position
	 * @param PosY Y position
	 * @param OutNodeId Generated node ID
	 * @param OutError Error message if failed
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
	 * @param TransitionGraph Transition graph
	 * @param SourceNodeId Source node ID
	 * @param SourcePinName Source pin name
	 * @param TargetNodeId Target node ID
	 * @param TargetPinName Target pin name
	 * @param OutError Error message if failed
	 * @return True if successful
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
	 * @param TransitionGraph Transition graph
	 * @param ConditionNodeId Condition node ID
	 * @param ConditionPinName Condition output pin name
	 * @param OutError Error message if failed
	 * @return True if successful
	 */
	static bool ConnectToTransitionResult(
		UEdGraph* TransitionGraph,
		const FString& ConditionNodeId,
		const FString& ConditionPinName,
		FString& OutError
	);

	/**
	 * Create a comparison chain: GetVariable -> Comparison -> Result
	 * Auto-chains with AND to existing logic if present
	 *
	 * @param AnimBP Animation Blueprint
	 * @param TransitionGraph Transition graph
	 * @param VariableName Variable to compare
	 * @param ComparisonType Comparison operator
	 * @param CompareValue Value to compare against
	 * @param Position Node position
	 * @param OutError Error message if failed
	 * @return JSON result with node IDs
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
	/** Create TimeRemaining getter node */
	static UEdGraphNode* CreateTimeRemainingNode(
		UEdGraph* Graph,
		const TSharedPtr<FJsonObject>& Params,
		FVector2D Position,
		FString& OutError
	);

	/** Create comparison node with type-appropriate function */
	static UEdGraphNode* CreateComparisonNode(
		UEdGraph* Graph,
		const FString& ComparisonType,
		const TSharedPtr<FJsonObject>& Params,
		FVector2D Position,
		FString& OutError,
		bool bIsBooleanType = false,
		EComparisonPinType PinType = EComparisonPinType::Float
	);

	/** Create logic node (And, Or, Not) */
	static UEdGraphNode* CreateLogicNode(
		UEdGraph* Graph,
		const FString& LogicType,
		FVector2D Position,
		FString& OutError
	);

	/** Create variable getter node */
	static UEdGraphNode* CreateVariableGetNode(
		UEdGraph* Graph,
		UAnimBlueprint* AnimBP,
		const FString& VariableName,
		FVector2D Position,
		FString& OutError
	);

	// ===== Pin Type Auto-Detection Helpers =====

	/**
	 * Detect the comparison type from a pin's type info
	 * @param Pin Pin to analyze
	 * @return Appropriate comparison type for the pin
	 */
	static EComparisonPinType DetectComparisonTypeFromPin(const UEdGraphPin* Pin);

	/**
	 * Check if a node is a KismetMathLibrary comparison node
	 * @param Node Node to check
	 * @param OutComparisonType If comparison node, the type of comparison (Greater, Less, etc.)
	 * @param OutPinType If comparison node, the pin type it's configured for
	 * @return True if node is a comparison node
	 */
	static bool IsComparisonNode(
		const UEdGraphNode* Node,
		FString& OutComparisonType,
		EComparisonPinType& OutPinType
	);

	/**
	 * Recreate a comparison node with a different pin type
	 * Preserves position and metadata, recreates with correct function
	 * @param Graph Graph containing the node
	 * @param ExistingNode Existing comparison node to replace
	 * @param ComparisonType Type of comparison (Greater, Less, etc.)
	 * @param NewPinType New pin type for the comparison
	 * @param OutError Error message if failed
	 * @return New comparison node or nullptr
	 */
	static UEdGraphNode* RecreateComparisonNodeWithType(
		UEdGraph* Graph,
		UEdGraphNode* ExistingNode,
		const FString& ComparisonType,
		EComparisonPinType NewPinType,
		FString& OutError
	);
};
