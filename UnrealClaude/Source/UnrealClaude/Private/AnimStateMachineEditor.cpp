// Copyright Natali Caggiano. All Rights Reserved.

#include "AnimStateMachineEditor.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationTransitionGraph.h"
#include "AnimationTransitionSchema.h"
#include "AnimationStateGraph.h"
#include "AnimationStateGraphSchema.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "HAL/PlatformAtomics.h"

volatile int32 FAnimStateMachineEditor::NodeIdCounter = 0;
const FString FAnimStateMachineEditor::NodeIdPrefix = TEXT("MCP_ANIM_ID:");

// ===== State Machine Management =====

UAnimGraphNode_StateMachine* FAnimStateMachineEditor::CreateStateMachine(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("Invalid Animation Blueprint");
		return nullptr;
	}

	// Get or create the AnimGraph
	TArray<UEdGraph*> AnimGraphs;
	AnimBP->GetAllGraphs(AnimGraphs);

	UAnimationGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : AnimGraphs)
	{
		if (UAnimationGraph* AG = Cast<UAnimationGraph>(Graph))
		{
			AnimGraph = AG;
			break;
		}
	}

	if (!AnimGraph)
	{
		OutError = TEXT("Animation Blueprint has no AnimGraph");
		return nullptr;
	}

	// Create the state machine node
	FGraphNodeCreator<UAnimGraphNode_StateMachine> NodeCreator(*AnimGraph);
	UAnimGraphNode_StateMachine* StateMachineNode = NodeCreator.CreateNode();

	if (!StateMachineNode)
	{
		OutError = TEXT("Failed to create state machine node");
		return nullptr;
	}

	StateMachineNode->NodePosX = static_cast<int32>(Position.X);
	StateMachineNode->NodePosY = static_cast<int32>(Position.Y);

	// Set the name
	StateMachineNode->OnRenameNode(StateMachineName);

	NodeCreator.Finalize();

	// Create the internal state machine graph
	UAnimationStateMachineGraph* SMGraph = CastChecked<UAnimationStateMachineGraph>(
		FBlueprintEditorUtils::CreateNewGraph(
			StateMachineNode,
			NAME_None,
			UAnimationStateMachineGraph::StaticClass(),
			UAnimationStateMachineSchema::StaticClass()
		)
	);

	StateMachineNode->EditorStateMachineGraph = SMGraph;

	// Call schema to create default nodes (including entry node)
	const UAnimationStateMachineSchema* Schema = CastChecked<UAnimationStateMachineSchema>(SMGraph->GetSchema());
	if (Schema)
	{
		Schema->CreateDefaultNodesForGraph(*SMGraph);
	}

	// Set the owner reference
	SMGraph->OwnerAnimGraphNode = StateMachineNode;

	// Generate node ID
	OutNodeId = FString::Printf(TEXT("StateMachine_%s"), *StateMachineName.Replace(TEXT(" "), TEXT("_")));
	SetNodeId(StateMachineNode, OutNodeId);

	// Mark dirty
	AnimGraph->Modify();
	AnimBP->Modify();

	return StateMachineNode;
}

UAnimGraphNode_StateMachine* FAnimStateMachineEditor::FindStateMachine(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	FString& OutError)
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
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node))
			{
				FString NodeName = SMNode->GetStateMachineName();
				if (NodeName.Equals(StateMachineName, ESearchCase::IgnoreCase))
				{
					return SMNode;
				}
			}
		}
	}

	// Get available names for error message
	TArray<FString> AvailableNames = GetStateMachineNames(AnimBP);
	OutError = FString::Printf(TEXT("State machine '%s' not found. Available: %s"),
		*StateMachineName,
		AvailableNames.Num() > 0 ? *FString::Join(AvailableNames, TEXT(", ")) : TEXT("(none)"));

	return nullptr;
}

TArray<FString> FAnimStateMachineEditor::GetStateMachineNames(UAnimBlueprint* AnimBP)
{
	TArray<FString> Names;

	if (!AnimBP) return Names;

	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node))
			{
				Names.Add(SMNode->GetStateMachineName());
			}
		}
	}

	return Names;
}

TArray<UAnimGraphNode_StateMachine*> FAnimStateMachineEditor::GetAllStateMachines(UAnimBlueprint* AnimBP)
{
	TArray<UAnimGraphNode_StateMachine*> StateMachines;

	if (!AnimBP) return StateMachines;

	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node))
			{
				StateMachines.Add(SMNode);
			}
		}
	}

	return StateMachines;
}

TArray<UAnimStateNode*> FAnimStateMachineEditor::GetAllStates(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	FString& OutError)
{
	TArray<UAnimStateNode*> States;

	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return States;

	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(SM, OutError);
	if (!SMGraph) return States;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			States.Add(StateNode);
		}
	}

	return States;
}

TArray<UAnimStateTransitionNode*> FAnimStateMachineEditor::GetAllTransitions(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	FString& OutError)
{
	TArray<UAnimStateTransitionNode*> Transitions;

	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return Transitions;

	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(SM, OutError);
	if (!SMGraph) return Transitions;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Node))
		{
			Transitions.Add(TransNode);
		}
	}

	return Transitions;
}

UAnimationStateMachineGraph* FAnimStateMachineEditor::GetStateMachineGraph(
	UAnimGraphNode_StateMachine* StateMachine,
	FString& OutError)
{
	if (!StateMachine)
	{
		OutError = TEXT("Invalid state machine node");
		return nullptr;
	}

	UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(
		StateMachine->EditorStateMachineGraph
	);

	if (!SMGraph)
	{
		OutError = TEXT("State machine has no internal graph");
		return nullptr;
	}

	return SMGraph;
}

// ===== State Management =====

UAnimStateNode* FAnimStateMachineEditor::AddState(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	FVector2D Position,
	bool bIsEntryState,
	FString& OutNodeId,
	FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return nullptr;

	UAnimStateNode* State = AddState(SM, StateName, Position, OutNodeId, OutError);

	// If bIsEntryState, set this state as the entry state
	if (State && bIsEntryState)
	{
		FString EntryError;
		if (!SetEntryState(SM, StateName, EntryError))
		{
			UE_LOG(LogTemp, Warning, TEXT("State '%s' created but failed to set as entry: %s"), *StateName, *EntryError);
		}
	}

	return State;
}

UAnimStateNode* FAnimStateMachineEditor::AddState(
	UAnimGraphNode_StateMachine* StateMachine,
	const FString& StateName,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	if (!StateMachine)
	{
		OutError = TEXT("Invalid state machine");
		return nullptr;
	}

	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, OutError);
	if (!SMGraph)
	{
		return nullptr;
	}

	// Check if state already exists
	if (FindStateNodeInGraph(SMGraph, StateName))
	{
		OutError = FString::Printf(TEXT("State '%s' already exists"), *StateName);
		return nullptr;
	}

	// Create state node using the schema action
	FGraphNodeCreator<UAnimStateNode> NodeCreator(*SMGraph);
	UAnimStateNode* StateNode = NodeCreator.CreateNode();

	if (!StateNode)
	{
		OutError = TEXT("Failed to create state node");
		return nullptr;
	}

	StateNode->NodePosX = static_cast<int32>(Position.X);
	StateNode->NodePosY = static_cast<int32>(Position.Y);

	// IMPORTANT: Finalize MUST be called before setting BoundGraph
	// The engine asserts that BoundGraph == 0 during node initialization
	NodeCreator.Finalize();

	// Create the bound graph (state's internal animation graph)
	// Use UAnimationStateGraphSchema which properly initializes state graphs
	UAnimationStateGraph* StateGraph = CastChecked<UAnimationStateGraph>(
		FBlueprintEditorUtils::CreateNewGraph(
			StateNode,
			FName(*StateName),
			UAnimationStateGraph::StaticClass(),
			UAnimationStateGraphSchema::StaticClass()
		)
	);
	StateNode->BoundGraph = StateGraph;

	// CRITICAL: Create the result node (Output Animation Pose) in the state graph
	// The schema's CreateDefaultNodesForGraph should handle this, but we ensure it exists
	const UAnimationStateGraphSchema* Schema = CastChecked<UAnimationStateGraphSchema>(StateGraph->GetSchema());
	if (Schema)
	{
		Schema->CreateDefaultNodesForGraph(*StateGraph);
	}

	// Verify the result node was created
	bool bHasResultNode = false;
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		if (Node->IsA<UAnimGraphNode_StateResult>())
		{
			bHasResultNode = true;
			break;
		}
	}

	// If schema didn't create the result node, create it manually
	if (!bHasResultNode)
	{
		FGraphNodeCreator<UAnimGraphNode_StateResult> ResultCreator(*StateGraph);
		UAnimGraphNode_StateResult* ResultNode = ResultCreator.CreateNode();
		if (ResultNode)
		{
			ResultNode->NodePosX = 200;
			ResultNode->NodePosY = 0;
			ResultCreator.Finalize();
		}
	}

	// Generate and set node ID
	OutNodeId = GenerateStateNodeId(StateName, SMGraph);
	SetNodeId(StateNode, OutNodeId);

	// Mark dirty
	SMGraph->Modify();
	StateGraph->Modify();

	return StateNode;
}

bool FAnimStateMachineEditor::RemoveState(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return false;

	return RemoveState(SM, StateName, OutError);
}

bool FAnimStateMachineEditor::RemoveState(
	UAnimGraphNode_StateMachine* StateMachine,
	const FString& StateName,
	FString& OutError)
{
	UAnimStateNode* StateNode = FindState(StateMachine, StateName, OutError);
	if (!StateNode)
	{
		return false;
	}

	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, OutError);
	if (!SMGraph)
	{
		return false;
	}

	// Remove all connected transitions first
	TArray<UEdGraphNode*> NodesToRemove;
	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Node))
		{
			// Check if this transition connects to/from our state
			UAnimStateNodeBase* PrevState = TransNode->GetPreviousState();
			UAnimStateNodeBase* NextState = TransNode->GetNextState();

			if (PrevState == StateNode || NextState == StateNode)
			{
				NodesToRemove.Add(TransNode);
			}
		}
	}

	// Remove transitions
	for (UEdGraphNode* Node : NodesToRemove)
	{
		SMGraph->RemoveNode(Node);
	}

	// Remove the state
	SMGraph->RemoveNode(StateNode);
	SMGraph->Modify();

	return true;
}

UAnimStateNode* FAnimStateMachineEditor::FindState(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return nullptr;

	return FindState(SM, StateName, OutError);
}

UAnimStateNode* FAnimStateMachineEditor::FindState(
	UAnimGraphNode_StateMachine* StateMachine,
	const FString& StateName,
	FString& OutError)
{
	if (!StateMachine)
	{
		OutError = TEXT("Invalid state machine");
		return nullptr;
	}

	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, OutError);
	if (!SMGraph)
	{
		return nullptr;
	}

	UAnimStateNode* State = FindStateNodeInGraph(SMGraph, StateName);
	if (!State)
	{
		TArray<FString> AvailableStates = GetStateNames(StateMachine);
		OutError = FString::Printf(TEXT("State '%s' not found. Available: %s"),
			*StateName,
			AvailableStates.Num() > 0 ? *FString::Join(AvailableStates, TEXT(", ")) : TEXT("(none)"));
	}

	return State;
}

UAnimStateNode* FAnimStateMachineEditor::FindStateById(
	UAnimGraphNode_StateMachine* StateMachine,
	const FString& NodeId)
{
	if (!StateMachine) return nullptr;

	FString Error;
	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, Error);
	if (!SMGraph) return nullptr;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			if (GetNodeId(StateNode) == NodeId)
			{
				return StateNode;
			}
		}
	}

	return nullptr;
}

TArray<FString> FAnimStateMachineEditor::GetStateNames(UAnimGraphNode_StateMachine* StateMachine)
{
	TArray<FString> Names;

	if (!StateMachine) return Names;

	FString Error;
	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, Error);
	if (!SMGraph) return Names;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			Names.Add(StateNode->GetStateName());
		}
	}

	return Names;
}

bool FAnimStateMachineEditor::SetEntryState(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return false;

	return SetEntryState(SM, StateName, OutError);
}

bool FAnimStateMachineEditor::SetEntryState(
	UAnimGraphNode_StateMachine* StateMachine,
	const FString& StateName,
	FString& OutError)
{
	if (!StateMachine)
	{
		OutError = TEXT("Invalid state machine");
		return false;
	}

	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, OutError);
	if (!SMGraph)
	{
		return false;
	}

	// Find the entry node
	UAnimStateEntryNode* EntryNode = SMGraph->EntryNode;
	if (!EntryNode)
	{
		// Try to find entry node in graph nodes
		for (UEdGraphNode* Node : SMGraph->Nodes)
		{
			if (UAnimStateEntryNode* FoundEntry = Cast<UAnimStateEntryNode>(Node))
			{
				EntryNode = FoundEntry;
				break;
			}
		}
	}

	if (!EntryNode)
	{
		OutError = TEXT("State machine has no entry node");
		return false;
	}

	// Find the target state
	UAnimStateNode* TargetState = FindStateNodeInGraph(SMGraph, StateName);
	if (!TargetState)
	{
		OutError = FString::Printf(TEXT("State '%s' not found in state machine"), *StateName);
		return false;
	}

	// Find the entry node's output pin
	UEdGraphPin* EntryOutputPin = nullptr;
	for (UEdGraphPin* Pin : EntryNode->Pins)
	{
		if (Pin->Direction == EGPD_Output)
		{
			EntryOutputPin = Pin;
			break;
		}
	}

	if (!EntryOutputPin)
	{
		OutError = TEXT("Entry node has no output pin");
		return false;
	}

	// Find the target state's input pin
	UEdGraphPin* StateInputPin = nullptr;
	for (UEdGraphPin* Pin : TargetState->Pins)
	{
		if (Pin->Direction == EGPD_Input)
		{
			StateInputPin = Pin;
			break;
		}
	}

	if (!StateInputPin)
	{
		OutError = FString::Printf(TEXT("State '%s' has no input pin"), *StateName);
		return false;
	}

	// Break existing entry connections
	EntryOutputPin->BreakAllPinLinks();

	// Connect entry to the new target state
	EntryOutputPin->MakeLinkTo(StateInputPin);
	SMGraph->Modify();

	return true;
}

FString FAnimStateMachineEditor::GetEntryStateName(UAnimGraphNode_StateMachine* StateMachine)
{
	if (!StateMachine)
	{
		return FString();
	}

	FString Error;
	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, Error);
	if (!SMGraph)
	{
		return FString();
	}

	// Find the entry node
	UAnimStateEntryNode* EntryNode = SMGraph->EntryNode;
	if (!EntryNode)
	{
		for (UEdGraphNode* Node : SMGraph->Nodes)
		{
			if (UAnimStateEntryNode* FoundEntry = Cast<UAnimStateEntryNode>(Node))
			{
				EntryNode = FoundEntry;
				break;
			}
		}
	}

	if (!EntryNode)
	{
		return FString();
	}

	// Find what state the entry is connected to
	for (UEdGraphPin* Pin : EntryNode->Pins)
	{
		if (Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
		{
			UEdGraphPin* LinkedPin = Pin->LinkedTo[0];
			if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(LinkedPin->GetOwningNode()))
			{
				return StateNode->GetStateName();
			}
		}
	}

	return FString();
}

UEdGraph* FAnimStateMachineEditor::GetStateBoundGraph(
	UAnimStateNode* StateNode,
	FString& OutError)
{
	if (!StateNode)
	{
		OutError = TEXT("Invalid state node");
		return nullptr;
	}

	UEdGraph* BoundGraph = StateNode->GetBoundGraph();
	if (!BoundGraph)
	{
		OutError = TEXT("State has no bound graph");
		return nullptr;
	}

	return BoundGraph;
}

// ===== Transition Management =====

UAnimStateTransitionNode* FAnimStateMachineEditor::CreateTransition(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	FString& OutNodeId,
	FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return nullptr;

	return CreateTransition(SM, FromState, ToState, OutNodeId, OutError);
}

UAnimStateTransitionNode* FAnimStateMachineEditor::CreateTransition(
	UAnimGraphNode_StateMachine* StateMachine,
	const FString& FromState,
	const FString& ToState,
	FString& OutNodeId,
	FString& OutError)
{
	if (!StateMachine)
	{
		OutError = TEXT("Invalid state machine");
		return nullptr;
	}

	// Find source and target states
	UAnimStateNode* SourceState = FindState(StateMachine, FromState, OutError);
	if (!SourceState) return nullptr;

	UAnimStateNode* TargetState = FindState(StateMachine, ToState, OutError);
	if (!TargetState) return nullptr;

	// Check if transition already exists
	UAnimStateTransitionNode* ExistingTransition = FindTransition(StateMachine, FromState, ToState, OutError);
	if (ExistingTransition)
	{
		OutError = FString::Printf(TEXT("Transition from '%s' to '%s' already exists"), *FromState, *ToState);
		return nullptr;
	}
	OutError.Empty(); // Clear error from FindTransition

	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, OutError);
	if (!SMGraph) return nullptr;

	// Create transition node
	FGraphNodeCreator<UAnimStateTransitionNode> NodeCreator(*SMGraph);
	UAnimStateTransitionNode* TransitionNode = NodeCreator.CreateNode();

	if (!TransitionNode)
	{
		OutError = TEXT("Failed to create transition node");
		return nullptr;
	}

	// Position between the two states
	TransitionNode->NodePosX = (SourceState->NodePosX + TargetState->NodePosX) / 2;
	TransitionNode->NodePosY = (SourceState->NodePosY + TargetState->NodePosY) / 2;

	NodeCreator.Finalize();

	// Connect the transition to states
	ConnectStateNodes(SourceState, TargetState, TransitionNode);

	// Create the transition rule graph
	UAnimationTransitionGraph* TransitionGraph = CastChecked<UAnimationTransitionGraph>(
		FBlueprintEditorUtils::CreateNewGraph(
			TransitionNode,
			NAME_None,
			UAnimationTransitionGraph::StaticClass(),
			UAnimationTransitionSchema::StaticClass()
		)
	);
	TransitionNode->BoundGraph = TransitionGraph;

	// CRITICAL: Create the result node (bCanEnterTransition) in the transition graph
	const UAnimationTransitionSchema* Schema = CastChecked<UAnimationTransitionSchema>(TransitionGraph->GetSchema());
	if (Schema)
	{
		Schema->CreateDefaultNodesForGraph(*TransitionGraph);
	}

	// Verify the result node was created
	bool bHasResultNode = false;
	for (UEdGraphNode* Node : TransitionGraph->Nodes)
	{
		if (Node->IsA<UAnimGraphNode_TransitionResult>())
		{
			bHasResultNode = true;
			break;
		}
	}

	// If schema didn't create the result node, create it manually
	if (!bHasResultNode)
	{
		FGraphNodeCreator<UAnimGraphNode_TransitionResult> ResultCreator(*TransitionGraph);
		UAnimGraphNode_TransitionResult* ResultNode = ResultCreator.CreateNode();
		if (ResultNode)
		{
			ResultNode->NodePosX = 200;
			ResultNode->NodePosY = 0;
			ResultCreator.Finalize();
		}
	}

	// Generate and set node ID
	OutNodeId = GenerateTransitionNodeId(FromState, ToState, SMGraph);
	SetNodeId(TransitionNode, OutNodeId);

	SMGraph->Modify();
	TransitionGraph->Modify();

	return TransitionNode;
}

bool FAnimStateMachineEditor::RemoveTransition(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return false;

	return RemoveTransition(SM, FromState, ToState, OutError);
}

bool FAnimStateMachineEditor::RemoveTransition(
	UAnimGraphNode_StateMachine* StateMachine,
	const FString& FromState,
	const FString& ToState,
	FString& OutError)
{
	UAnimStateTransitionNode* TransitionNode = FindTransition(StateMachine, FromState, ToState, OutError);
	if (!TransitionNode) return false;

	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, OutError);
	if (!SMGraph) return false;

	// Break all pin links
	TransitionNode->BreakAllNodeLinks();

	// Remove the node
	SMGraph->RemoveNode(TransitionNode);
	SMGraph->Modify();

	return true;
}

UAnimStateTransitionNode* FAnimStateMachineEditor::FindTransition(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return nullptr;

	return FindTransition(SM, FromState, ToState, OutError);
}

UAnimStateTransitionNode* FAnimStateMachineEditor::FindTransition(
	UAnimGraphNode_StateMachine* StateMachine,
	const FString& FromState,
	const FString& ToState,
	FString& OutError)
{
	if (!StateMachine)
	{
		OutError = TEXT("Invalid state machine");
		return nullptr;
	}

	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, OutError);
	if (!SMGraph) return nullptr;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Node))
		{
			UAnimStateNodeBase* PrevState = TransNode->GetPreviousState();
			UAnimStateNodeBase* NextState = TransNode->GetNextState();

			if (PrevState && NextState)
			{
				FString PrevName = PrevState->GetStateName();
				FString NextName = NextState->GetStateName();

				if (PrevName.Equals(FromState, ESearchCase::IgnoreCase) &&
					NextName.Equals(ToState, ESearchCase::IgnoreCase))
				{
					return TransNode;
				}
			}
		}
	}

	OutError = FString::Printf(TEXT("Transition from '%s' to '%s' not found"), *FromState, *ToState);
	return nullptr;
}

UAnimStateTransitionNode* FAnimStateMachineEditor::FindTransitionById(
	UAnimGraphNode_StateMachine* StateMachine,
	const FString& NodeId)
{
	if (!StateMachine) return nullptr;

	FString Error;
	UAnimationStateMachineGraph* SMGraph = GetStateMachineGraph(StateMachine, Error);
	if (!SMGraph) return nullptr;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Node))
		{
			if (GetNodeId(TransNode) == NodeId)
			{
				return TransNode;
			}
		}
	}

	return nullptr;
}

UEdGraph* FAnimStateMachineEditor::GetTransitionGraph(
	UAnimStateTransitionNode* Transition,
	FString& OutError)
{
	if (!Transition)
	{
		OutError = TEXT("Invalid transition node");
		return nullptr;
	}

	UEdGraph* BoundGraph = Transition->BoundGraph;
	if (!BoundGraph)
	{
		OutError = TEXT("Transition has no bound graph");
		return nullptr;
	}

	return BoundGraph;
}

bool FAnimStateMachineEditor::SetTransitionDuration(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	float Duration,
	FString& OutError)
{
	UAnimStateTransitionNode* Transition = FindTransition(AnimBP, StateMachineName, FromState, ToState, OutError);
	if (!Transition) return false;

	return SetTransitionDuration(Transition, Duration, OutError);
}

bool FAnimStateMachineEditor::SetTransitionDuration(
	UAnimStateTransitionNode* Transition,
	float Duration,
	FString& OutError)
{
	if (!Transition)
	{
		OutError = TEXT("Invalid transition node");
		return false;
	}

	Transition->CrossfadeDuration = Duration;
	Transition->Modify();

	return true;
}

bool FAnimStateMachineEditor::SetTransitionPriority(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	int32 Priority,
	FString& OutError)
{
	UAnimStateTransitionNode* Transition = FindTransition(AnimBP, StateMachineName, FromState, ToState, OutError);
	if (!Transition) return false;

	return SetTransitionPriority(Transition, Priority, OutError);
}

bool FAnimStateMachineEditor::SetTransitionPriority(
	UAnimStateTransitionNode* Transition,
	int32 Priority,
	FString& OutError)
{
	if (!Transition)
	{
		OutError = TEXT("Invalid transition node");
		return false;
	}

	Transition->PriorityOrder = Priority;
	Transition->Modify();

	return true;
}

// ===== Serialization =====

TSharedPtr<FJsonObject> FAnimStateMachineEditor::SerializeStateMachineInfo(
	UAnimGraphNode_StateMachine* StateMachine)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!StateMachine) return Json;

	Json->SetStringField(TEXT("name"), StateMachine->GetStateMachineName());
	Json->SetStringField(TEXT("node_id"), GetNodeId(StateMachine));

	// Add state list
	TArray<TSharedPtr<FJsonValue>> StatesArray;
	TArray<FString> StateNames = GetStateNames(StateMachine);
	for (const FString& Name : StateNames)
	{
		StatesArray.Add(MakeShared<FJsonValueString>(Name));
	}
	Json->SetArrayField(TEXT("states"), StatesArray);

	return Json;
}

TSharedPtr<FJsonObject> FAnimStateMachineEditor::SerializeStateInfo(UAnimStateNode* State)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!State) return Json;

	Json->SetStringField(TEXT("name"), State->GetStateName());
	Json->SetStringField(TEXT("node_id"), GetNodeId(State));
	Json->SetNumberField(TEXT("pos_x"), State->NodePosX);
	Json->SetNumberField(TEXT("pos_y"), State->NodePosY);

	return Json;
}

TSharedPtr<FJsonObject> FAnimStateMachineEditor::SerializeTransitionInfo(UAnimStateTransitionNode* Transition)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!Transition) return Json;

	Json->SetStringField(TEXT("node_id"), GetNodeId(Transition));
	Json->SetNumberField(TEXT("duration"), Transition->CrossfadeDuration);
	Json->SetNumberField(TEXT("priority"), Transition->PriorityOrder);

	if (UAnimStateNodeBase* PrevState = Transition->GetPreviousState())
	{
		Json->SetStringField(TEXT("from_state"), PrevState->GetStateName());
	}

	if (UAnimStateNodeBase* NextState = Transition->GetNextState())
	{
		Json->SetStringField(TEXT("to_state"), NextState->GetStateName());
	}

	return Json;
}

// ===== Node ID System =====

FString FAnimStateMachineEditor::GenerateStateNodeId(const FString& StateName, UEdGraph* Graph)
{
	int32 Counter = FPlatformAtomics::InterlockedIncrement(&NodeIdCounter);
	FString SafeName = StateName.Replace(TEXT(" "), TEXT("_"));
	FString NodeId = FString::Printf(TEXT("State_%s_%d"), *SafeName, Counter);

	// Verify uniqueness
	if (Graph)
	{
		bool bUnique = true;
		do
		{
			bUnique = true;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (GetNodeId(Node) == NodeId)
				{
					Counter = FPlatformAtomics::InterlockedIncrement(&NodeIdCounter);
					NodeId = FString::Printf(TEXT("State_%s_%d"), *SafeName, Counter);
					bUnique = false;
					break;
				}
			}
		} while (!bUnique);
	}

	return NodeId;
}

FString FAnimStateMachineEditor::GenerateTransitionNodeId(const FString& FromState, const FString& ToState, UEdGraph* Graph)
{
	int32 Counter = FPlatformAtomics::InterlockedIncrement(&NodeIdCounter);
	FString SafeFrom = FromState.Replace(TEXT(" "), TEXT("_"));
	FString SafeTo = ToState.Replace(TEXT(" "), TEXT("_"));
	FString NodeId = FString::Printf(TEXT("Transition_%s_To_%s_%d"), *SafeFrom, *SafeTo, Counter);

	// Verify uniqueness
	if (Graph)
	{
		bool bUnique = true;
		do
		{
			bUnique = true;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (GetNodeId(Node) == NodeId)
				{
					Counter = FPlatformAtomics::InterlockedIncrement(&NodeIdCounter);
					NodeId = FString::Printf(TEXT("Transition_%s_To_%s_%d"), *SafeFrom, *SafeTo, Counter);
					bUnique = false;
					break;
				}
			}
		} while (!bUnique);
	}

	return NodeId;
}

void FAnimStateMachineEditor::SetNodeId(UEdGraphNode* Node, const FString& NodeId)
{
	if (Node)
	{
		Node->NodeComment = NodeIdPrefix + NodeId;
	}
}

FString FAnimStateMachineEditor::GetNodeId(UEdGraphNode* Node)
{
	if (Node && Node->NodeComment.StartsWith(NodeIdPrefix))
	{
		return Node->NodeComment.Mid(NodeIdPrefix.Len());
	}
	return FString();
}

// ===== Private Helpers =====

UAnimStateNode* FAnimStateMachineEditor::FindStateNodeInGraph(UAnimationStateMachineGraph* Graph, const FString& StateName)
{
	if (!Graph) return nullptr;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			if (StateNode->GetStateName().Equals(StateName, ESearchCase::IgnoreCase))
			{
				return StateNode;
			}
		}
	}

	return nullptr;
}

void FAnimStateMachineEditor::ConnectStateNodes(UAnimStateNode* FromState, UAnimStateNode* ToState,
												UAnimStateTransitionNode* Transition)
{
	if (!FromState || !ToState || !Transition) return;

	// Get the output pin from source state
	UEdGraphPin* SourceOutputPin = FromState->GetOutputPin();

	// Get the input pin from transition
	UEdGraphPin* TransitionInputPin = Transition->GetInputPin();

	// Connect source to transition
	if (SourceOutputPin && TransitionInputPin)
	{
		SourceOutputPin->MakeLinkTo(TransitionInputPin);
	}

	// Get the output pin from transition
	UEdGraphPin* TransitionOutputPin = Transition->GetOutputPin();

	// Get the input pin from target state
	UEdGraphPin* TargetInputPin = ToState->GetInputPin();

	// Connect transition to target
	if (TransitionOutputPin && TargetInputPin)
	{
		TransitionOutputPin->MakeLinkTo(TargetInputPin);
	}
}
