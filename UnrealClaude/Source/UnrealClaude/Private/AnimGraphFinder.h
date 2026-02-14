// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimBlueprint.h"

class UAnimGraphNode_StateMachine;
class UAnimGraphNode_Root;
class UEdGraph;

/**
 * AnimGraphFinder - Graph finding utilities for Animation Blueprints
 *
 * Responsibilities:
 * - Finding AnimGraph (main animation graph)
 * - Finding state bound graphs
 * - Finding transition graphs
 * - Finding AnimGraph root node
 */
class FAnimGraphFinder
{
public:
	/**
	 * Find AnimGraph (main animation graph)
	 * @param AnimBP Animation Blueprint to search
	 * @param OutError Error message if not found
	 * @return The AnimGraph or nullptr
	 */
	static UEdGraph* FindAnimGraph(UAnimBlueprint* AnimBP, FString& OutError);

	/**
	 * Find state bound graph by state name
	 * @param AnimBP Animation Blueprint
	 * @param StateMachineName Name of the state machine
	 * @param StateName Name of the state
	 * @param OutError Error message if not found
	 * @return The state's bound graph or nullptr
	 */
	static UEdGraph* FindStateBoundGraph(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		FString& OutError
	);

	/**
	 * Find transition graph between two states
	 * @param AnimBP Animation Blueprint
	 * @param StateMachineName Name of the state machine
	 * @param FromState Source state name
	 * @param ToState Target state name
	 * @param OutError Error message if not found
	 * @return The transition graph or nullptr
	 */
	static UEdGraph* FindTransitionGraph(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& FromState,
		const FString& ToState,
		FString& OutError
	);

	/**
	 * Find the AnimGraph root node (Output Pose)
	 * @param AnimBP Animation Blueprint
	 * @param OutError Error message if not found
	 * @return The root node or nullptr
	 */
	static UAnimGraphNode_Root* FindAnimGraphRoot(UAnimBlueprint* AnimBP, FString& OutError);
};
