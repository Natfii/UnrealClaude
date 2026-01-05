// Copyright Your Name. All Rights Reserved.

#include "AnimGraphEditor.h"
#include "AnimStateMachineEditor.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimStateNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateTransitionNode.h"
#include "AnimationGraph.h"
#include "AnimationStateGraph.h"
#include "AnimationTransitionGraph.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_Base.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_TransitionRuleGetter.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "HAL/PlatformAtomics.h"

volatile int32 FAnimGraphEditor::NodeIdCounter = 0;
const FString FAnimGraphEditor::NodeIdPrefix = TEXT("MCP_ANIM_ID:");

// ===== Graph Finding =====

UEdGraph* FAnimGraphEditor::FindAnimGraph(UAnimBlueprint* AnimBP, FString& OutError)
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

UEdGraph* FAnimGraphEditor::FindStateBoundGraph(
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

UEdGraph* FAnimGraphEditor::FindTransitionGraph(
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

// ===== Transition Condition Nodes =====

UEdGraphNode* FAnimGraphEditor::CreateTransitionConditionNode(
	UEdGraph* TransitionGraph,
	const FString& NodeType,
	const TSharedPtr<FJsonObject>& Params,
	int32 PosX,
	int32 PosY,
	FString& OutNodeId,
	FString& OutError)
{
	if (!TransitionGraph)
	{
		OutError = TEXT("Invalid transition graph");
		return nullptr;
	}

	FVector2D Position(PosX, PosY);
	UEdGraphNode* Node = nullptr;

	FString NormalizedType = NodeType.ToLower();

	if (NormalizedType == TEXT("timeremaining"))
	{
		Node = CreateTimeRemainingNode(TransitionGraph, Params, Position, OutError);
	}
	else if (NormalizedType == TEXT("comparefloat") || NormalizedType == TEXT("compare_float"))
	{
		Node = CreateComparisonNode(TransitionGraph, TEXT("Float"), Params, Position, OutError);
	}
	else if (NormalizedType == TEXT("comparebool") || NormalizedType == TEXT("compare_bool"))
	{
		Node = CreateComparisonNode(TransitionGraph, TEXT("Bool"), Params, Position, OutError);
	}
	else if (NormalizedType == TEXT("and"))
	{
		Node = CreateLogicNode(TransitionGraph, TEXT("And"), Position, OutError);
	}
	else if (NormalizedType == TEXT("or"))
	{
		Node = CreateLogicNode(TransitionGraph, TEXT("Or"), Position, OutError);
	}
	else if (NormalizedType == TEXT("not"))
	{
		Node = CreateLogicNode(TransitionGraph, TEXT("Not"), Position, OutError);
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown condition node type: %s. Supported: TimeRemaining, CompareFloat, CompareBool, And, Or, Not"), *NodeType);
		return nullptr;
	}

	if (Node)
	{
		OutNodeId = GenerateAnimNodeId(TEXT("Cond"), NodeType, TransitionGraph);
		SetNodeId(Node, OutNodeId);
		TransitionGraph->Modify();
	}

	return Node;
}

bool FAnimGraphEditor::ConnectTransitionNodes(
	UEdGraph* TransitionGraph,
	const FString& SourceNodeId,
	const FString& SourcePinName,
	const FString& TargetNodeId,
	const FString& TargetPinName,
	FString& OutError)
{
	if (!TransitionGraph)
	{
		OutError = TEXT("Invalid transition graph");
		return false;
	}

	// Find source node
	UEdGraphNode* SourceNode = FindNodeById(TransitionGraph, SourceNodeId);
	if (!SourceNode)
	{
		OutError = FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId);
		return false;
	}

	// Find target node
	UEdGraphNode* TargetNode = FindNodeById(TransitionGraph, TargetNodeId);
	if (!TargetNode)
	{
		OutError = FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId);
		return false;
	}

	// Find pins
	UEdGraphPin* SourcePin = FindPinByName(SourceNode, SourcePinName, EGPD_Output);
	if (!SourcePin)
	{
		OutError = FString::Printf(TEXT("Source pin '%s' not found on node %s"), *SourcePinName, *SourceNodeId);
		return false;
	}

	UEdGraphPin* TargetPin = FindPinByName(TargetNode, TargetPinName, EGPD_Input);
	if (!TargetPin)
	{
		OutError = FString::Printf(TEXT("Target pin '%s' not found on node %s"), *TargetPinName, *TargetNodeId);
		return false;
	}

	// Make connection
	SourcePin->MakeLinkTo(TargetPin);
	TransitionGraph->Modify();

	return true;
}

bool FAnimGraphEditor::ConnectToTransitionResult(
	UEdGraph* TransitionGraph,
	const FString& ConditionNodeId,
	const FString& ConditionPinName,
	FString& OutError)
{
	if (!TransitionGraph)
	{
		OutError = TEXT("Invalid transition graph");
		return false;
	}

	// Find condition node
	UEdGraphNode* ConditionNode = FindNodeById(TransitionGraph, ConditionNodeId);
	if (!ConditionNode)
	{
		OutError = FString::Printf(TEXT("Condition node not found: %s"), *ConditionNodeId);
		return false;
	}

	// Find condition output pin
	FString PinName = ConditionPinName.IsEmpty() ? TEXT("ReturnValue") : ConditionPinName;
	UEdGraphPin* ConditionPin = FindPinByName(ConditionNode, PinName, EGPD_Output);
	if (!ConditionPin)
	{
		OutError = FString::Printf(TEXT("Condition output pin '%s' not found"), *PinName);
		return false;
	}

	// Find result node
	UEdGraphNode* ResultNode = FindResultNode(TransitionGraph);
	if (!ResultNode)
	{
		OutError = TEXT("Transition result node not found");
		return false;
	}

	// Find bCanEnterTransition pin
	UEdGraphPin* ResultPin = FindPinByName(ResultNode, TEXT("bCanEnterTransition"), EGPD_Input);
	if (!ResultPin)
	{
		// Try alternative names
		ResultPin = FindPinByName(ResultNode, TEXT("CanEnterTransition"), EGPD_Input);
	}

	if (!ResultPin)
	{
		OutError = TEXT("Cannot find transition result input pin");
		return false;
	}

	// Make connection
	ConditionPin->MakeLinkTo(ResultPin);
	TransitionGraph->Modify();

	return true;
}

// ===== Animation Asset Nodes =====

UEdGraphNode* FAnimGraphEditor::CreateAnimSequenceNode(
	UEdGraph* StateGraph,
	UAnimSequence* AnimSequence,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	if (!StateGraph)
	{
		OutError = TEXT("Invalid state graph");
		return nullptr;
	}

	if (!AnimSequence)
	{
		OutError = TEXT("Invalid animation sequence");
		return nullptr;
	}

	// Create sequence player node
	FGraphNodeCreator<UAnimGraphNode_SequencePlayer> NodeCreator(*StateGraph);
	UAnimGraphNode_SequencePlayer* SeqNode = NodeCreator.CreateNode();

	if (!SeqNode)
	{
		OutError = TEXT("Failed to create sequence player node");
		return nullptr;
	}

	SeqNode->NodePosX = static_cast<int32>(Position.X);
	SeqNode->NodePosY = static_cast<int32>(Position.Y);

	// Set the animation sequence
	SeqNode->Node.SetSequence(AnimSequence);

	NodeCreator.Finalize();

	// Generate ID
	OutNodeId = GenerateAnimNodeId(TEXT("Anim"), AnimSequence->GetName(), StateGraph);
	SetNodeId(SeqNode, OutNodeId);

	StateGraph->Modify();

	return SeqNode;
}

UEdGraphNode* FAnimGraphEditor::CreateBlendSpaceNode(
	UEdGraph* StateGraph,
	UBlendSpace* BlendSpace,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	if (!StateGraph)
	{
		OutError = TEXT("Invalid state graph");
		return nullptr;
	}

	if (!BlendSpace)
	{
		OutError = TEXT("Invalid BlendSpace");
		return nullptr;
	}

	// Create BlendSpace player node
	FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> NodeCreator(*StateGraph);
	UAnimGraphNode_BlendSpacePlayer* BSNode = NodeCreator.CreateNode();

	if (!BSNode)
	{
		OutError = TEXT("Failed to create BlendSpace player node");
		return nullptr;
	}

	BSNode->NodePosX = static_cast<int32>(Position.X);
	BSNode->NodePosY = static_cast<int32>(Position.Y);

	// Set the BlendSpace
	BSNode->Node.SetBlendSpace(BlendSpace);

	NodeCreator.Finalize();

	// Generate ID
	OutNodeId = GenerateAnimNodeId(TEXT("BlendSpace"), BlendSpace->GetName(), StateGraph);
	SetNodeId(BSNode, OutNodeId);

	StateGraph->Modify();

	return BSNode;
}

UEdGraphNode* FAnimGraphEditor::CreateBlendSpace1DNode(
	UEdGraph* StateGraph,
	UBlendSpace1D* BlendSpace,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	if (!StateGraph)
	{
		OutError = TEXT("Invalid state graph");
		return nullptr;
	}

	if (!BlendSpace)
	{
		OutError = TEXT("Invalid BlendSpace1D");
		return nullptr;
	}

	// BlendSpace1D uses the same player node
	FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> NodeCreator(*StateGraph);
	UAnimGraphNode_BlendSpacePlayer* BSNode = NodeCreator.CreateNode();

	if (!BSNode)
	{
		OutError = TEXT("Failed to create BlendSpace1D player node");
		return nullptr;
	}

	BSNode->NodePosX = static_cast<int32>(Position.X);
	BSNode->NodePosY = static_cast<int32>(Position.Y);

	// Set the BlendSpace
	BSNode->Node.SetBlendSpace(BlendSpace);

	NodeCreator.Finalize();

	// Generate ID
	OutNodeId = GenerateAnimNodeId(TEXT("BlendSpace1D"), BlendSpace->GetName(), StateGraph);
	SetNodeId(BSNode, OutNodeId);

	StateGraph->Modify();

	return BSNode;
}

bool FAnimGraphEditor::ConnectToOutputPose(
	UEdGraph* StateGraph,
	const FString& AnimNodeId,
	FString& OutError)
{
	if (!StateGraph)
	{
		OutError = TEXT("Invalid state graph");
		return false;
	}

	// Find anim node
	UEdGraphNode* AnimNode = FindNodeById(StateGraph, AnimNodeId);
	if (!AnimNode)
	{
		OutError = FString::Printf(TEXT("Animation node not found: %s"), *AnimNodeId);
		return false;
	}

	// Find pose output pin on anim node
	UEdGraphPin* PosePin = FindPinByName(AnimNode, TEXT("Pose"), EGPD_Output);
	if (!PosePin)
	{
		PosePin = FindPinByName(AnimNode, TEXT("Output"), EGPD_Output);
	}

	if (!PosePin)
	{
		OutError = TEXT("Animation node has no pose output pin");
		return false;
	}

	// Find result node
	UEdGraphNode* ResultNode = FindResultNode(StateGraph);
	if (!ResultNode)
	{
		OutError = TEXT("State result node not found");
		return false;
	}

	// Find result input pin
	UEdGraphPin* ResultPin = FindPinByName(ResultNode, TEXT("Result"), EGPD_Input);
	if (!ResultPin)
	{
		ResultPin = FindPinByName(ResultNode, TEXT("Pose"), EGPD_Input);
	}

	if (!ResultPin)
	{
		OutError = TEXT("Cannot find state result input pin");
		return false;
	}

	// Make connection
	PosePin->MakeLinkTo(ResultPin);
	StateGraph->Modify();

	return true;
}

bool FAnimGraphEditor::ClearStateGraph(UEdGraph* StateGraph, FString& OutError)
{
	if (!StateGraph)
	{
		OutError = TEXT("Invalid state graph");
		return false;
	}

	// Collect nodes to remove (exclude result node)
	TArray<UEdGraphNode*> NodesToRemove;
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		// Keep result nodes
		if (Node->IsA<UAnimGraphNode_StateResult>() ||
			Node->IsA<UAnimGraphNode_TransitionResult>())
		{
			continue;
		}

		NodesToRemove.Add(Node);
	}

	// Remove nodes
	for (UEdGraphNode* Node : NodesToRemove)
	{
		Node->BreakAllNodeLinks();
		StateGraph->RemoveNode(Node);
	}

	StateGraph->Modify();

	return true;
}

// ===== Node Finding =====

UEdGraphNode* FAnimGraphEditor::FindNodeById(UEdGraph* Graph, const FString& NodeId)
{
	if (!Graph) return nullptr;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (GetNodeId(Node) == NodeId)
		{
			return Node;
		}
	}

	return nullptr;
}

UEdGraphNode* FAnimGraphEditor::FindResultNode(UEdGraph* Graph)
{
	if (!Graph) return nullptr;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node->IsA<UAnimGraphNode_StateResult>() ||
			Node->IsA<UAnimGraphNode_TransitionResult>())
		{
			return Node;
		}
	}

	return nullptr;
}

UEdGraphPin* FAnimGraphEditor::FindPinByName(
	UEdGraphNode* Node,
	const FString& PinName,
	EEdGraphPinDirection Direction)
{
	if (!Node) return nullptr;

	for (UEdGraphPin* Pin : Node->Pins)
	{
		bool bDirectionMatch = (Direction == EGPD_MAX) || (Pin->Direction == Direction);
		if (bDirectionMatch && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			return Pin;
		}
	}

	return nullptr;
}

// ===== Node ID System =====

FString FAnimGraphEditor::GenerateAnimNodeId(
	const FString& NodeType,
	const FString& Context,
	UEdGraph* Graph)
{
	int32 Counter = FPlatformAtomics::InterlockedIncrement(&NodeIdCounter);
	FString SafeContext = Context.Replace(TEXT(" "), TEXT("_"));
	FString NodeId = FString::Printf(TEXT("%s_%s_%d"), *NodeType, *SafeContext, Counter);

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
					NodeId = FString::Printf(TEXT("%s_%s_%d"), *NodeType, *SafeContext, Counter);
					bUnique = false;
					break;
				}
			}
		} while (!bUnique);
	}

	return NodeId;
}

void FAnimGraphEditor::SetNodeId(UEdGraphNode* Node, const FString& NodeId)
{
	if (Node)
	{
		Node->NodeComment = NodeIdPrefix + NodeId;
	}
}

FString FAnimGraphEditor::GetNodeId(UEdGraphNode* Node)
{
	if (Node && Node->NodeComment.StartsWith(NodeIdPrefix))
	{
		return Node->NodeComment.Mid(NodeIdPrefix.Len());
	}
	return FString();
}

TSharedPtr<FJsonObject> FAnimGraphEditor::SerializeAnimNodeInfo(UEdGraphNode* Node)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!Node) return Json;

	Json->SetStringField(TEXT("node_id"), GetNodeId(Node));
	Json->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
	Json->SetNumberField(TEXT("pos_x"), Node->NodePosX);
	Json->SetNumberField(TEXT("pos_y"), Node->NodePosY);

	// Add pin info
	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
		PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
		PinJson->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
		PinJson->SetBoolField(TEXT("connected"), Pin->LinkedTo.Num() > 0);
		PinsArray.Add(MakeShared<FJsonValueObject>(PinJson));
	}
	Json->SetArrayField(TEXT("pins"), PinsArray);

	return Json;
}

// ===== Private Helpers =====

UEdGraphNode* FAnimGraphEditor::CreateTimeRemainingNode(UEdGraph* Graph,
	const TSharedPtr<FJsonObject>& Params, FVector2D Position, FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Invalid graph");
		return nullptr;
	}

	// Create the TimeRemaining getter node using UK2Node_TransitionRuleGetter
	FGraphNodeCreator<UK2Node_TransitionRuleGetter> NodeCreator(*Graph);
	UK2Node_TransitionRuleGetter* GetterNode = NodeCreator.CreateNode();

	if (!GetterNode)
	{
		OutError = TEXT("Failed to create TimeRemaining getter node");
		return nullptr;
	}

	GetterNode->NodePosX = static_cast<int32>(Position.X);
	GetterNode->NodePosY = static_cast<int32>(Position.Y);

	// Set to get time remaining from current animation asset
	GetterNode->GetterType = ETransitionGetter::AnimationAsset_GetTimeFromEnd;

	// Try to find an associated animation player node in the source state
	// This is needed for the getter to know which animation to query
	UAnimationTransitionGraph* TransitionGraph = Cast<UAnimationTransitionGraph>(Graph);
	if (TransitionGraph)
	{
		// Get the transition node that owns this graph via GetOuter
		UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(TransitionGraph->GetOuter());
		if (TransitionNode)
		{
			UAnimStateNodeBase* PreviousState = TransitionNode->GetPreviousState();
			UEdGraph* StateGraph = PreviousState ? PreviousState->GetBoundGraph() : nullptr;
			if (StateGraph)
			{
				// Find animation player node in the source state
				for (UEdGraphNode* Node : StateGraph->Nodes)
				{
					if (UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node))
					{
						// Check if this is an animation asset player (sequence player, blend space, etc.)
						if (AnimNode->IsA<UAnimGraphNode_SequencePlayer>() ||
							AnimNode->IsA<UAnimGraphNode_BlendSpacePlayer>())
						{
							GetterNode->AssociatedAnimAssetPlayerNode = AnimNode;
							break;
						}
					}
				}
			}
		}
	}

	NodeCreator.Finalize();

	// Allocate default pins
	GetterNode->AllocateDefaultPins();

	return GetterNode;
}

UEdGraphNode* FAnimGraphEditor::CreateComparisonNode(UEdGraph* Graph, const FString& ComparisonType,
	const TSharedPtr<FJsonObject>& Params, FVector2D Position, FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Invalid graph");
		return nullptr;
	}

	// Map comparison type to KismetMathLibrary function name
	FName FunctionName;
	if (ComparisonType.Equals(TEXT("Greater"), ESearchCase::IgnoreCase))
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Greater_DoubleDouble);
	}
	else if (ComparisonType.Equals(TEXT("Less"), ESearchCase::IgnoreCase))
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_DoubleDouble);
	}
	else if (ComparisonType.Equals(TEXT("GreaterEqual"), ESearchCase::IgnoreCase))
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, GreaterEqual_DoubleDouble);
	}
	else if (ComparisonType.Equals(TEXT("LessEqual"), ESearchCase::IgnoreCase))
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_DoubleDouble);
	}
	else if (ComparisonType.Equals(TEXT("Equal"), ESearchCase::IgnoreCase))
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_DoubleDouble);
	}
	else if (ComparisonType.Equals(TEXT("NotEqual"), ESearchCase::IgnoreCase))
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_DoubleDouble);
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown comparison type: %s. Supported: Greater, Less, GreaterEqual, LessEqual, Equal, NotEqual"), *ComparisonType);
		return nullptr;
	}

	// Find the function in KismetMathLibrary
	UFunction* Function = UKismetMathLibrary::StaticClass()->FindFunctionByName(FunctionName);
	if (!Function)
	{
		OutError = FString::Printf(TEXT("Failed to find comparison function: %s"), *FunctionName.ToString());
		return nullptr;
	}

	// Create the call function node
	FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
	UK2Node_CallFunction* CallNode = NodeCreator.CreateNode();

	if (!CallNode)
	{
		OutError = TEXT("Failed to create comparison call function node");
		return nullptr;
	}

	CallNode->NodePosX = static_cast<int32>(Position.X);
	CallNode->NodePosY = static_cast<int32>(Position.Y);

	// Set the function reference - this is the critical step that was missing before
	CallNode->SetFromFunction(Function);

	NodeCreator.Finalize();

	// Allocate pins based on the function signature
	CallNode->AllocateDefaultPins();

	return CallNode;
}

UEdGraphNode* FAnimGraphEditor::CreateLogicNode(UEdGraph* Graph, const FString& LogicType,
	FVector2D Position, FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Invalid graph");
		return nullptr;
	}

	// Map logic type to KismetMathLibrary function name
	FName FunctionName;
	if (LogicType.Equals(TEXT("And"), ESearchCase::IgnoreCase))
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND);
	}
	else if (LogicType.Equals(TEXT("Or"), ESearchCase::IgnoreCase))
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanOR);
	}
	else if (LogicType.Equals(TEXT("Not"), ESearchCase::IgnoreCase))
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool);
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown logic type: %s. Supported: And, Or, Not"), *LogicType);
		return nullptr;
	}

	// Find the function in KismetMathLibrary
	UFunction* Function = UKismetMathLibrary::StaticClass()->FindFunctionByName(FunctionName);
	if (!Function)
	{
		OutError = FString::Printf(TEXT("Failed to find logic function: %s"), *FunctionName.ToString());
		return nullptr;
	}

	// Create the call function node
	FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
	UK2Node_CallFunction* CallNode = NodeCreator.CreateNode();

	if (!CallNode)
	{
		OutError = TEXT("Failed to create logic call function node");
		return nullptr;
	}

	CallNode->NodePosX = static_cast<int32>(Position.X);
	CallNode->NodePosY = static_cast<int32>(Position.Y);

	// Set the function reference - this is the critical step that was missing before
	CallNode->SetFromFunction(Function);

	NodeCreator.Finalize();

	// Allocate pins based on the function signature
	CallNode->AllocateDefaultPins();

	return CallNode;
}

UEdGraphNode* FAnimGraphEditor::CreateVariableGetNode(UEdGraph* Graph, UAnimBlueprint* AnimBP,
	const FString& VariableName, FVector2D Position, FString& OutError)
{
	if (!Graph || !AnimBP)
	{
		OutError = TEXT("Invalid graph or AnimBlueprint");
		return nullptr;
	}

	// Find the variable property
	FProperty* Property = FindFProperty<FProperty>(AnimBP->GeneratedClass, *VariableName);
	if (!Property)
	{
		Property = FindFProperty<FProperty>(AnimBP->SkeletonGeneratedClass, *VariableName);
	}

	if (!Property)
	{
		OutError = FString::Printf(TEXT("Variable '%s' not found in AnimBlueprint"), *VariableName);
		return nullptr;
	}

	// Create variable get node
	FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*Graph);
	UK2Node_VariableGet* VarNode = NodeCreator.CreateNode();

	if (!VarNode)
	{
		OutError = TEXT("Failed to create variable get node");
		return nullptr;
	}

	VarNode->NodePosX = static_cast<int32>(Position.X);
	VarNode->NodePosY = static_cast<int32>(Position.Y);
	VarNode->VariableReference.SetSelfMember(FName(*VariableName));

	NodeCreator.Finalize();

	return VarNode;
}
