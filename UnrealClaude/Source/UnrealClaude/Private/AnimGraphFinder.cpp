// Copyright Natali Caggiano. All Rights Reserved.

#include "AnimGraphFinder.h"
#include "AnimStateMachineEditor.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_Root.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationGraph.h"
#include "EdGraph/EdGraph.h"

UEdGraph* FAnimGraphFinder::FindAnimGraph(UAnimBlueprint* AnimBP, FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("Invalid Animation Blueprint");
		return nullptr;
	}

	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph->IsA<UAnimationGraph>())
		{
			return Graph;
		}
	}

	OutError = TEXT("Animation Blueprint has no AnimGraph");
	return nullptr;
}

UEdGraph* FAnimGraphFinder::FindStateBoundGraph(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	FString& OutError)
{
	// Find state machine
	UAnimGraphNode_StateMachine* StateMachine = FAnimStateMachineEditor::FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!StateMachine) return nullptr;

	// Find state
	UAnimStateNode* State = FAnimStateMachineEditor::FindState(StateMachine, StateName, OutError);
	if (!State) return nullptr;

	// Get bound graph
	return FAnimStateMachineEditor::GetStateBoundGraph(State, OutError);
}

UEdGraph* FAnimGraphFinder::FindTransitionGraph(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	FString& OutError)
{
	// Find state machine
	UAnimGraphNode_StateMachine* StateMachine = FAnimStateMachineEditor::FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!StateMachine) return nullptr;

	// Find transition
	UAnimStateTransitionNode* Transition = FAnimStateMachineEditor::FindTransition(StateMachine, FromState, ToState, OutError);
	if (!Transition) return nullptr;

	// Get transition graph
	return FAnimStateMachineEditor::GetTransitionGraph(Transition, OutError);
}

UAnimGraphNode_Root* FAnimGraphFinder::FindAnimGraphRoot(UAnimBlueprint* AnimBP, FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("Invalid Animation Blueprint");
		return nullptr;
	}

	// Find the main AnimGraph
	UEdGraph* AnimGraph = FindAnimGraph(AnimBP, OutError);
	if (!AnimGraph)
	{
		return nullptr;
	}

	// Find the root node in the AnimGraph
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_Root* RootNode = Cast<UAnimGraphNode_Root>(Node))
		{
			return RootNode;
		}
	}

	OutError = TEXT("AnimGraph root node (Output Pose) not found");
	return nullptr;
}
