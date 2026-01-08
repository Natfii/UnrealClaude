// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimBlueprint.h"
#include "Dom/JsonObject.h"

// Include sub-modules
#include "AnimStateMachineEditor.h"
#include "AnimGraphEditor.h"
#include "AnimAssetManager.h"

// Forward declarations for ASCII diagram generation
class UAnimStateTransitionNode;

/**
 * Animation Blueprint Utilities - Facade
 *
 * This class provides a unified interface for Animation Blueprint manipulation,
 * delegating to specialized sub-modules:
 *
 * - FAnimStateMachineEditor: State machine and transition management
 * - FAnimGraphEditor: Graph node operations and connections
 * - FAnimAssetManager: Animation asset loading and assignment
 *
 * Operation Hierarchy:
 * Level 1: AnimBlueprint access and validation
 * Level 2: State Machine management (create, find, list)
 * Level 3: State/Transition management (add, remove, configure)
 * Level 4: Transition graph operations (condition nodes)
 * Level 5: Animation assignment (sequences, blend spaces, montages)
 *
 * Supported Operations:
 * - State Machine: create_state_machine, get_state_machines
 * - States: add_state, remove_state, list_states
 * - Transitions: add_transition, remove_transition, set_transition_duration, set_transition_priority
 * - Condition Nodes: add_condition_node, connect_condition_nodes
 * - Animation: set_state_animation, find_animations
 */
class UNREALCLAUDE_API FAnimationBlueprintUtils
{
public:
	// ===== AnimBlueprint Access (Level 1) =====

	/**
	 * Load Animation Blueprint by path
	 * @param BlueprintPath - Asset path (/Game/...)
	 * @param OutError - Error message if failed
	 * @return Loaded AnimBlueprint or nullptr
	 */
	static UAnimBlueprint* LoadAnimBlueprint(const FString& BlueprintPath, FString& OutError);

	/**
	 * Check if blueprint is an Animation Blueprint
	 */
	static bool IsAnimationBlueprint(UBlueprint* Blueprint);

	/**
	 * Compile Animation Blueprint after modifications
	 */
	static bool CompileAnimBlueprint(UAnimBlueprint* AnimBP, FString& OutError);

	/**
	 * Mark AnimBlueprint as modified
	 */
	static void MarkAnimBlueprintModified(UAnimBlueprint* AnimBP);

	// ===== State Machine Operations (Level 2) =====

	/**
	 * Create a new state machine in AnimGraph
	 * Delegates to FAnimStateMachineEditor::CreateStateMachine
	 */
	static UAnimGraphNode_StateMachine* CreateStateMachine(
		UAnimBlueprint* AnimBP,
		const FString& MachineName,
		FVector2D Position,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Find state machine by name
	 */
	static UAnimGraphNode_StateMachine* FindStateMachine(
		UAnimBlueprint* AnimBP,
		const FString& MachineName,
		FString& OutError
	);

	/**
	 * Get all state machines in AnimBlueprint
	 */
	static TArray<UAnimGraphNode_StateMachine*> GetAllStateMachines(UAnimBlueprint* AnimBP);

	/**
	 * Get state machine graph for adding states/transitions
	 */
	static UAnimationStateMachineGraph* GetStateMachineGraph(
		UAnimGraphNode_StateMachine* StateMachineNode,
		FString& OutError
	);

	// ===== State Operations (Level 3) =====

	/**
	 * Add a new state to state machine
	 */
	static UAnimStateNode* AddState(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		FVector2D Position,
		bool bIsEntryState,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Remove state from state machine
	 */
	static bool RemoveState(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		FString& OutError
	);

	/**
	 * Find state by name
	 */
	static UAnimStateNode* FindState(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		FString& OutError
	);

	/**
	 * Get all states in state machine
	 */
	static TArray<UAnimStateNode*> GetAllStates(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		FString& OutError
	);

	/**
	 * Set the entry state for a state machine
	 * @param AnimBP - Animation Blueprint
	 * @param StateMachineName - State machine name
	 * @param StateName - Name of the state to set as entry
	 * @param OutError - Error message if failed
	 * @return True if successful
	 */
	static bool SetEntryState(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		FString& OutError
	);

	// ===== Transition Operations (Level 3) =====

	/**
	 * Create transition between states
	 */
	static UAnimStateTransitionNode* CreateTransition(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromStateName,
		const FString& ToStateName,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Remove transition
	 */
	static bool RemoveTransition(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromStateName,
		const FString& ToStateName,
		FString& OutError
	);

	/**
	 * Find transition between states
	 */
	static UAnimStateTransitionNode* FindTransition(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromStateName,
		const FString& ToStateName,
		FString& OutError
	);

	/**
	 * Set transition duration
	 */
	static bool SetTransitionDuration(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromStateName,
		const FString& ToStateName,
		float Duration,
		FString& OutError
	);

	/**
	 * Set transition priority
	 */
	static bool SetTransitionPriority(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromStateName,
		const FString& ToStateName,
		int32 Priority,
		FString& OutError
	);

	/**
	 * Get all transitions in state machine
	 */
	static TArray<UAnimStateTransitionNode*> GetAllTransitions(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		FString& OutError
	);

	// ===== Transition Condition Graph Operations (Level 4) =====

	/**
	 * Get transition rule graph for adding condition nodes
	 */
	static UEdGraph* GetTransitionGraph(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromStateName,
		const FString& ToStateName,
		FString& OutError
	);

	/**
	 * Add condition node to transition graph
	 * Supported types: TimeRemaining, Greater, Less, GreaterEqual, LessEqual, Equal, NotEqual, And, Or, Not, GetVariable
	 */
	static UEdGraphNode* AddConditionNode(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromStateName,
		const FString& ToStateName,
		const FString& NodeType,
		const TSharedPtr<FJsonObject>& Params,
		FVector2D Position,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Delete condition node from transition graph
	 */
	static bool DeleteConditionNode(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromStateName,
		const FString& ToStateName,
		const FString& NodeId,
		FString& OutError
	);

	/**
	 * Connect condition nodes in transition graph
	 */
	static bool ConnectConditionNodes(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromStateName,
		const FString& ToStateName,
		const FString& SourceNodeId,
		const FString& SourcePinName,
		const FString& TargetNodeId,
		const FString& TargetPinName,
		FString& OutError
	);

	/**
	 * Connect condition output to transition result
	 */
	static bool ConnectToTransitionResult(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromStateName,
		const FString& ToStateName,
		const FString& ConditionNodeId,
		const FString& ConditionPinName,
		FString& OutError
	);

	// ===== Animation Assignment Operations (Level 5) =====

	/**
	 * Set animation sequence for a state
	 */
	static bool SetStateAnimSequence(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		const FString& AnimSequencePath,
		FString& OutError
	);

	/**
	 * Set BlendSpace for a state (2D)
	 * @param ParameterBindings - Map of BlendSpace params to Blueprint variables
	 */
	static bool SetStateBlendSpace(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		const FString& BlendSpacePath,
		const TMap<FString, FString>& ParameterBindings,
		FString& OutError
	);

	/**
	 * Set BlendSpace1D for a state
	 */
	static bool SetStateBlendSpace1D(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		const FString& BlendSpacePath,
		const FString& ParameterBinding,
		FString& OutError
	);

	/**
	 * Set montage for a state
	 */
	static bool SetStateMontage(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		const FString& MontagePath,
		FString& OutError
	);

	/**
	 * Find animation assets in project
	 * @param SearchPattern - Name pattern
	 * @param AssetType - AnimSequence, BlendSpace, BlendSpace1D, Montage, or All
	 * @param SkeletonFilter - Optional skeleton to filter compatibility
	 */
	static TArray<FString> FindAnimationAssets(
		const FString& SearchPattern,
		const FString& AssetType = TEXT("All"),
		UAnimBlueprint* AnimBPForSkeleton = nullptr
	);

	// ===== Serialization =====

	/**
	 * Serialize AnimBlueprint info to JSON
	 */
	static TSharedPtr<FJsonObject> SerializeAnimBlueprintInfo(UAnimBlueprint* AnimBP);

	/**
	 * Serialize state machine structure to JSON
	 */
	static TSharedPtr<FJsonObject> SerializeStateMachineInfo(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName
	);

	/**
	 * Serialize state info to JSON
	 */
	static TSharedPtr<FJsonObject> SerializeStateInfo(UAnimStateNode* StateNode);

	/**
	 * Serialize transition info to JSON
	 */
	static TSharedPtr<FJsonObject> SerializeTransitionInfo(UAnimStateTransitionNode* TransitionNode);

	// ===== Batch Operations =====

	/**
	 * Execute batch operations
	 * Each operation: { "type": "add_state", "params": {...} }
	 */
	static TSharedPtr<FJsonObject> ExecuteBatchOperations(
		UAnimBlueprint* AnimBP,
		const TArray<TSharedPtr<FJsonValue>>& Operations,
		FString& OutError
	);

	// ===== New Operations for MCP Tool Enhancements =====

	/**
	 * Get all nodes in a transition graph (get_transition_nodes)
	 * Can get single transition or all transitions in a state machine
	 */
	static TSharedPtr<FJsonObject> GetTransitionNodes(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		FString& OutError
	);

	/**
	 * Inspect node pins with detailed type information (inspect_node_pins)
	 */
	static TSharedPtr<FJsonObject> InspectNodePins(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		const FString& NodeId,
		FString& OutError
	);

	/**
	 * Set pin default value with type validation (set_pin_default_value)
	 */
	static bool SetPinDefaultValue(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		const FString& NodeId,
		const FString& PinName,
		const FString& Value,
		FString& OutError
	);

	/**
	 * Create comparison chain: GetVariable → Comparison → Result (add_comparison_chain)
	 * Auto-chains with AND to existing logic
	 */
	static TSharedPtr<FJsonObject> AddComparisonChain(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		const FString& VariableName,
		const FString& ComparisonType,
		const FString& CompareValue,
		FVector2D Position,
		FString& OutError
	);

	/**
	 * Validate blueprint and return detailed diagnostics (validate_blueprint)
	 */
	static TSharedPtr<FJsonObject> ValidateBlueprint(
		UAnimBlueprint* AnimBP,
		FString& OutError
	);

	/**
	 * Generate ASCII diagram of a state machine for visualization
	 * Includes state machine structure, transitions, and abbreviated conditions
	 *
	 * @param AnimBP Animation Blueprint
	 * @param StateMachineName State machine to visualize
	 * @param OutError Error message if failed
	 * @return JSON result with "ascii_diagram" and "enhanced_info" fields
	 */
	static TSharedPtr<FJsonObject> GetStateMachineDiagram(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		FString& OutError
	);

	/**
	 * Setup transition conditions in bulk using pattern matching (setup_transition_conditions)
	 *
	 * Supports multiple matching modes:
	 * - Exact: "Idle" matches only "Idle"
	 * - Wildcard: "*" matches any, "Attack_*" matches states starting with "Attack_"
	 * - Regex: "^Attack_\\d+$" matches Attack_1, Attack_2, etc.
	 * - List: ["Idle", "Walk", "Run"] matches any of these
	 *
	 * @param AnimBP - Animation Blueprint to modify
	 * @param StateMachineName - State machine name
	 * @param Rules - Array of rule objects with match patterns and conditions
	 * @param OutError - Error message if failed
	 * @return JSON result with matched transitions and results
	 *
	 * Rule format:
	 * {
	 *   "match": {
	 *     "from": "Idle" | "*" | "Attack_*" | ["Idle", "Walk"],
	 *     "to": "Walk" | "^.*_\\d$" | ["Run", "Jump"]
	 *   },
	 *   "conditions": [
	 *     {"variable": "Speed", "comparison": "Greater", "value": "10"},
	 *     {"variable": "bIsInAir", "comparison": "Equal", "value": "false"},
	 *     {"type": "TimeRemaining", "comparison": "Less", "value": "0.1"}
	 *   ],
	 *   "logic": "AND" | "OR" (default: AND)
	 * }
	 */
	static TSharedPtr<FJsonObject> SetupTransitionConditions(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const TArray<TSharedPtr<FJsonValue>>& Rules,
		FString& OutError
	);

private:
	// Pattern matching helpers for SetupTransitionConditions
	static bool MatchesPattern(const FString& StateName, const TSharedPtr<FJsonValue>& Pattern);
	static bool MatchesWildcard(const FString& StateName, const FString& Pattern);
	static bool MatchesRegex(const FString& StateName, const FString& Pattern);
	// Internal helpers
	static bool ValidateAnimBlueprintForOperation(UAnimBlueprint* AnimBP, FString& OutError);

	// ASCII diagram helpers
	struct FDiagramState
	{
		FString Name;
		int32 GridX = 0;
		int32 GridY = 0;
		bool bIsEntry = false;
		FString AnimationName;
	};

	struct FDiagramTransition
	{
		FString FromState;
		FString ToState;
		FString ConditionAbbrev; // Abbreviated condition (e.g., "Speed>10 & Time<0.2")
		float Duration = 0.2f;
	};

	/** Abbreviate transition condition graph to a short string */
	static FString AbbreviateTransitionCondition(UAnimStateTransitionNode* TransitionNode);

	/** Generate ASCII diagram string from states and transitions */
	static FString GenerateASCIIDiagram(
		const TArray<FDiagramState>& States,
		const TArray<FDiagramTransition>& Transitions
	);

	/** Calculate grid layout for states */
	static void CalculateStateLayout(
		TArray<FDiagramState>& States,
		const TArray<FDiagramTransition>& Transitions
	);
};
