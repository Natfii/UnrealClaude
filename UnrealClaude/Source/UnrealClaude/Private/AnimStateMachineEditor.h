// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimBlueprint.h"
#include "Dom/JsonObject.h"

// Forward declarations
class UAnimGraphNode_StateMachine;
class UAnimStateNode;
class UAnimStateTransitionNode;
class UAnimationStateMachineGraph;
class UEdGraph;

/**
 * State Machine Editor for Animation Blueprints
 *
 * Responsibilities:
 * - Creating and managing state machines
 * - Adding/removing states
 * - Creating transitions between states
 * - Managing transition properties
 *
 * State Hierarchy:
 * UAnimBlueprint
 *   └─ UAnimBlueprintGeneratedClass
 *       └─ AnimGraph (UEdGraph)
 *           └─ UAnimGraphNode_StateMachine
 *               └─ UAnimationStateMachineGraph
 *                   ├─ UAnimStateNode (states)
 *                   └─ UAnimStateTransitionNode (transitions)
 */
class FAnimStateMachineEditor
{
public:
	// ===== State Machine Management =====

	/**
	 * Create a new state machine in AnimGraph
	 * @param AnimBP - Animation Blueprint
	 * @param StateMachineName - Name for the state machine
	 * @param Position - Position in AnimGraph
	 * @param OutNodeId - Generated node ID
	 * @param OutError - Error message if failed
	 * @return Created state machine node or nullptr
	 */
	static UAnimGraphNode_StateMachine* CreateStateMachine(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		FVector2D Position,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Find state machine by name in AnimBlueprint
	 */
	static UAnimGraphNode_StateMachine* FindStateMachine(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		FString& OutError
	);

	/**
	 * Get all state machine names in AnimBlueprint
	 */
	static TArray<FString> GetStateMachineNames(UAnimBlueprint* AnimBP);

	/**
	 * Get all state machine nodes in AnimBlueprint
	 */
	static TArray<UAnimGraphNode_StateMachine*> GetAllStateMachines(UAnimBlueprint* AnimBP);

	/**
	 * Get all state nodes in a state machine
	 */
	static TArray<UAnimStateNode*> GetAllStates(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		FString& OutError
	);

	/**
	 * Get all transition nodes in a state machine
	 */
	static TArray<UAnimStateTransitionNode*> GetAllTransitions(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		FString& OutError
	);

	/**
	 * Get the internal graph of a state machine
	 */
	static UAnimationStateMachineGraph* GetStateMachineGraph(
		UAnimGraphNode_StateMachine* StateMachine,
		FString& OutError
	);

	// ===== State Management =====

	/**
	 * Add a state to a state machine (by AnimBP and name)
	 * @param AnimBP - Animation Blueprint
	 * @param StateMachineName - State machine name
	 * @param StateName - Name for the new state
	 * @param Position - Position in state machine graph
	 * @param bIsEntryState - Whether to set as entry state
	 * @param OutNodeId - Generated node ID
	 * @param OutError - Error message if failed
	 * @return Created state node or nullptr
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
	 * Add a state to a state machine (by node)
	 * @param StateMachine - Parent state machine
	 * @param StateName - Name for the new state
	 * @param Position - Position in state machine graph
	 * @param OutNodeId - Generated node ID
	 * @param OutError - Error message if failed
	 * @return Created state node or nullptr
	 */
	static UAnimStateNode* AddState(
		UAnimGraphNode_StateMachine* StateMachine,
		const FString& StateName,
		FVector2D Position,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Remove a state from state machine (by AnimBP and name)
	 */
	static bool RemoveState(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		FString& OutError
	);

	/**
	 * Remove a state from state machine (by node)
	 */
	static bool RemoveState(
		UAnimGraphNode_StateMachine* StateMachine,
		const FString& StateName,
		FString& OutError
	);

	/**
	 * Find state by name (by AnimBP and state machine name)
	 */
	static UAnimStateNode* FindState(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		FString& OutError
	);

	/**
	 * Find state by name in state machine (by node)
	 */
	static UAnimStateNode* FindState(
		UAnimGraphNode_StateMachine* StateMachine,
		const FString& StateName,
		FString& OutError
	);

	/**
	 * Find state by node ID
	 */
	static UAnimStateNode* FindStateById(
		UAnimGraphNode_StateMachine* StateMachine,
		const FString& NodeId
	);

	/**
	 * Get all state names in state machine
	 */
	static TArray<FString> GetStateNames(UAnimGraphNode_StateMachine* StateMachine);

	/**
	 * Set the entry state for a state machine (by AnimBP and names)
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

	/**
	 * Set the entry state for a state machine (by node)
	 * @param StateMachine - State machine node
	 * @param StateName - Name of the state to set as entry
	 * @param OutError - Error message if failed
	 * @return True if successful
	 */
	static bool SetEntryState(
		UAnimGraphNode_StateMachine* StateMachine,
		const FString& StateName,
		FString& OutError
	);

	/**
	 * Get the current entry state name for a state machine
	 * @param StateMachine - State machine node
	 * @return Name of entry state, or empty string if not set
	 */
	static FString GetEntryStateName(UAnimGraphNode_StateMachine* StateMachine);

	/**
	 * Get the bound graph for a state (where animations are placed)
	 */
	static UEdGraph* GetStateBoundGraph(
		UAnimStateNode* StateNode,
		FString& OutError
	);

	// ===== Transition Management =====

	/**
	 * Create transition between two states (by AnimBP and state machine name)
	 */
	static UAnimStateTransitionNode* CreateTransition(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Create transition between two states (by node)
	 * @param StateMachine - Parent state machine
	 * @param FromState - Source state name
	 * @param ToState - Target state name
	 * @param OutNodeId - Generated transition node ID
	 * @param OutError - Error message if failed
	 * @return Created transition node or nullptr
	 */
	static UAnimStateTransitionNode* CreateTransition(
		UAnimGraphNode_StateMachine* StateMachine,
		const FString& FromState,
		const FString& ToState,
		FString& OutNodeId,
		FString& OutError
	);

	/**
	 * Remove a transition (by AnimBP and state machine name)
	 */
	static bool RemoveTransition(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		FString& OutError
	);

	/**
	 * Remove a transition (by node)
	 */
	static bool RemoveTransition(
		UAnimGraphNode_StateMachine* StateMachine,
		const FString& FromState,
		const FString& ToState,
		FString& OutError
	);

	/**
	 * Find transition between states (by AnimBP and state machine name)
	 */
	static UAnimStateTransitionNode* FindTransition(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		FString& OutError
	);

	/**
	 * Find transition between states (by node)
	 */
	static UAnimStateTransitionNode* FindTransition(
		UAnimGraphNode_StateMachine* StateMachine,
		const FString& FromState,
		const FString& ToState,
		FString& OutError
	);

	/**
	 * Find transition by node ID
	 */
	static UAnimStateTransitionNode* FindTransitionById(
		UAnimGraphNode_StateMachine* StateMachine,
		const FString& NodeId
	);

	/**
	 * Get the transition rule graph (for condition nodes)
	 */
	static UEdGraph* GetTransitionGraph(
		UAnimStateTransitionNode* Transition,
		FString& OutError
	);

	/**
	 * Set transition duration (by AnimBP and state machine name)
	 */
	static bool SetTransitionDuration(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		float Duration,
		FString& OutError
	);

	/**
	 * Set transition duration (by node)
	 */
	static bool SetTransitionDuration(
		UAnimStateTransitionNode* Transition,
		float Duration,
		FString& OutError
	);

	/**
	 * Set transition priority order (by AnimBP and state machine name)
	 */
	static bool SetTransitionPriority(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		int32 Priority,
		FString& OutError
	);

	/**
	 * Set transition priority order (by node)
	 */
	static bool SetTransitionPriority(
		UAnimStateTransitionNode* Transition,
		int32 Priority,
		FString& OutError
	);

	// ===== Serialization =====

	/**
	 * Serialize state machine info to JSON
	 */
	static TSharedPtr<FJsonObject> SerializeStateMachineInfo(
		UAnimGraphNode_StateMachine* StateMachine
	);

	/**
	 * Serialize state info to JSON
	 */
	static TSharedPtr<FJsonObject> SerializeStateInfo(UAnimStateNode* State);

	/**
	 * Serialize transition info to JSON
	 */
	static TSharedPtr<FJsonObject> SerializeTransitionInfo(UAnimStateTransitionNode* Transition);

	// ===== Node ID System =====

	/**
	 * Generate unique state node ID
	 * Format: "State_{StateName}_{Counter}"
	 */
	static FString GenerateStateNodeId(const FString& StateName, UEdGraph* Graph);

	/**
	 * Generate unique transition node ID
	 * Format: "Transition_{FromState}_To_{ToState}_{Counter}"
	 */
	static FString GenerateTransitionNodeId(const FString& FromState, const FString& ToState, UEdGraph* Graph);

	/**
	 * Set node ID in node comment
	 */
	static void SetNodeId(UEdGraphNode* Node, const FString& NodeId);

	/**
	 * Get node ID from node comment
	 */
	static FString GetNodeId(UEdGraphNode* Node);

private:
	// Thread-safe counter for unique IDs
	static volatile int32 NodeIdCounter;

	// Node ID prefix
	static const FString NodeIdPrefix;

	// Internal helpers
	static UAnimStateNode* FindStateNodeInGraph(UAnimationStateMachineGraph* Graph, const FString& StateName);
	static void ConnectStateNodes(UAnimStateNode* FromState, UAnimStateNode* ToState,
								  UAnimStateTransitionNode* Transition);
};
