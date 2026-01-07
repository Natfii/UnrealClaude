// Copyright Natali Caggiano. All Rights Reserved.

/**
 * AnimGraphEditor - Facade for Animation Blueprint graph operations
 *
 * This class delegates to specialized helper classes:
 * - FAnimGraphFinder: Graph finding utilities
 * - FAnimNodePinUtils: Pin finding and connection utilities
 * - FAnimTransitionConditionFactory: Transition condition node creation
 * - FAnimAssetNodeFactory: Animation asset node creation
 *
 * See TECH_DEBT_REMEDIATION.md for details on the refactoring.
 */

#include "AnimGraphEditor.h"
#include "AnimGraphFinder.h"
#include "AnimNodePinUtils.h"
#include "AnimTransitionConditionFactory.h"
#include "AnimAssetNodeFactory.h"
#include "AnimStateMachineEditor.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateTransitionNode.h"
#include "AnimationGraph.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "HAL/PlatformAtomics.h"

volatile int32 FAnimGraphEditor::NodeIdCounter = 0;
const FString FAnimGraphEditor::NodeIdPrefix = TEXT("MCP_ANIM_ID:");

// ===== Graph Finding (delegates to FAnimGraphFinder) =====

UEdGraph* FAnimGraphEditor::FindAnimGraph(UAnimBlueprint* AnimBP, FString& OutError)
{
	return FAnimGraphFinder::FindAnimGraph(AnimBP, OutError);
}

UEdGraph* FAnimGraphEditor::FindStateBoundGraph(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	FString& OutError)
{
	return FAnimGraphFinder::FindStateBoundGraph(AnimBP, StateMachineName, StateName, OutError);
}

UEdGraph* FAnimGraphEditor::FindTransitionGraph(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	FString& OutError)
{
	return FAnimGraphFinder::FindTransitionGraph(AnimBP, StateMachineName, FromState, ToState, OutError);
}

// ===== Transition Condition Nodes (delegates to FAnimTransitionConditionFactory) =====

UEdGraphNode* FAnimGraphEditor::CreateTransitionConditionNode(
	UEdGraph* TransitionGraph,
	const FString& NodeType,
	const TSharedPtr<FJsonObject>& Params,
	int32 PosX,
	int32 PosY,
	FString& OutNodeId,
	FString& OutError)
{
	return FAnimTransitionConditionFactory::CreateTransitionConditionNode(
		TransitionGraph, NodeType, Params, PosX, PosY, OutNodeId, OutError);
}

bool FAnimGraphEditor::ConnectTransitionNodes(
	UEdGraph* TransitionGraph,
	const FString& SourceNodeId,
	const FString& SourcePinName,
	const FString& TargetNodeId,
	const FString& TargetPinName,
	FString& OutError)
{
	return FAnimTransitionConditionFactory::ConnectTransitionNodes(
		TransitionGraph, SourceNodeId, SourcePinName, TargetNodeId, TargetPinName, OutError);
}

bool FAnimGraphEditor::ConnectToTransitionResult(
	UEdGraph* TransitionGraph,
	const FString& ConditionNodeId,
	const FString& ConditionPinName,
	FString& OutError)
{
	return FAnimTransitionConditionFactory::ConnectToTransitionResult(
		TransitionGraph, ConditionNodeId, ConditionPinName, OutError);
}

// ===== Animation Asset Nodes (delegates to FAnimAssetNodeFactory) =====

UEdGraphNode* FAnimGraphEditor::CreateAnimSequenceNode(
	UEdGraph* StateGraph,
	UAnimSequence* AnimSequence,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	return FAnimAssetNodeFactory::CreateAnimSequenceNode(StateGraph, AnimSequence, Position, OutNodeId, OutError);
}

UEdGraphNode* FAnimGraphEditor::CreateBlendSpaceNode(
	UEdGraph* StateGraph,
	UBlendSpace* BlendSpace,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	return FAnimAssetNodeFactory::CreateBlendSpaceNode(StateGraph, BlendSpace, Position, OutNodeId, OutError);
}

UEdGraphNode* FAnimGraphEditor::CreateBlendSpace1DNode(
	UEdGraph* StateGraph,
	UBlendSpace1D* BlendSpace,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	return FAnimAssetNodeFactory::CreateBlendSpace1DNode(StateGraph, BlendSpace, Position, OutNodeId, OutError);
}

bool FAnimGraphEditor::ConnectToOutputPose(
	UEdGraph* StateGraph,
	const FString& AnimNodeId,
	FString& OutError)
{
	return FAnimAssetNodeFactory::ConnectToOutputPose(StateGraph, AnimNodeId, OutError);
}

bool FAnimGraphEditor::ClearStateGraph(UEdGraph* StateGraph, FString& OutError)
{
	return FAnimAssetNodeFactory::ClearStateGraph(StateGraph, OutError);
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
	return FAnimNodePinUtils::FindResultNode(Graph);
}

UEdGraphPin* FAnimGraphEditor::FindPinByName(
	UEdGraphNode* Node,
	const FString& PinName,
	EEdGraphPinDirection Direction)
{
	return FAnimNodePinUtils::FindPinByName(Node, PinName, Direction);
}

UEdGraphPin* FAnimGraphEditor::FindPinWithFallbacks(
	UEdGraphNode* Node,
	const FPinSearchConfig& Config,
	FString* OutError)
{
	return FAnimNodePinUtils::FindPinWithFallbacks(Node, Config, OutError);
}

FString FAnimGraphEditor::BuildAvailablePinsError(
	UEdGraphNode* Node,
	EEdGraphPinDirection Direction,
	const FString& Context)
{
	return FAnimNodePinUtils::BuildAvailablePinsError(Node, Direction, Context);
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

// ===== AnimGraph Root Connection =====

UAnimGraphNode_Root* FAnimGraphEditor::FindAnimGraphRoot(UAnimBlueprint* AnimBP, FString& OutError)
{
	return FAnimGraphFinder::FindAnimGraphRoot(AnimBP, OutError);
}

bool FAnimGraphEditor::ConnectStateMachineToAnimGraphRoot(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("Invalid Animation Blueprint");
		return false;
	}

	// Find the AnimGraph
	UEdGraph* AnimGraph = FindAnimGraph(AnimBP, OutError);
	if (!AnimGraph)
	{
		return false;
	}

	// Find the State Machine node in the AnimGraph
	UAnimGraphNode_StateMachine* StateMachineNode = FAnimStateMachineEditor::FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!StateMachineNode)
	{
		return false;
	}

	// Find the AnimGraph root node
	UAnimGraphNode_Root* RootNode = FindAnimGraphRoot(AnimBP, OutError);
	if (!RootNode)
	{
		return false;
	}

	// Find State Machine's output pose pin
	UEdGraphPin* SMOutputPin = FindPinByName(StateMachineNode, TEXT("Pose"), EGPD_Output);
	if (!SMOutputPin) SMOutputPin = FindPinByName(StateMachineNode, TEXT("Output"), EGPD_Output);
	if (!SMOutputPin) SMOutputPin = FindPinByName(StateMachineNode, TEXT("Output Pose"), EGPD_Output);
	if (!SMOutputPin)
	{
		// Fallback: find any output pin
		for (UEdGraphPin* Pin : StateMachineNode->Pins)
		{
			if (Pin->Direction == EGPD_Output)
			{
				SMOutputPin = Pin;
				break;
			}
		}
	}

	if (!SMOutputPin)
	{
		OutError = FString::Printf(TEXT("State Machine '%s' has no output pose pin"), *StateMachineName);
		return false;
	}

	// Find root's input pin
	UEdGraphPin* RootInputPin = FindPinByName(RootNode, TEXT("Result"), EGPD_Input);
	if (!RootInputPin) RootInputPin = FindPinByName(RootNode, TEXT("Pose"), EGPD_Input);
	if (!RootInputPin) RootInputPin = FindPinByName(RootNode, TEXT("InPose"), EGPD_Input);
	if (!RootInputPin)
	{
		for (UEdGraphPin* Pin : RootNode->Pins)
		{
			if (Pin->Direction == EGPD_Input)
			{
				RootInputPin = Pin;
				break;
			}
		}
	}

	if (!RootInputPin)
	{
		OutError = TEXT("AnimGraph root node has no input pose pin");
		return false;
	}

	// Break existing connections and make new connection
	RootInputPin->BreakAllPinLinks();
	SMOutputPin->MakeLinkTo(RootInputPin);
	AnimGraph->Modify();

	return true;
}

// ===== Transition Graph Node Operations =====

TSharedPtr<FJsonObject> FAnimGraphEditor::SerializeDetailedPinInfo(UEdGraphPin* Pin)
{
	TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();

	if (!Pin) return PinObj;

	PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
	PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));

	// Detailed type info
	FString TypeStr;
	FString SubCategoryStr;

	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		TypeStr = TEXT("bool");
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		TypeStr = TEXT("int32");
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		TypeStr = TEXT("int64");
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		TypeStr = (Pin->PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double) ? TEXT("double") : TEXT("float");
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		TypeStr = TEXT("FString");
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		TypeStr = TEXT("FName");
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		TypeStr = TEXT("FText");
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		TypeStr = TEXT("exec");
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (UScriptStruct* Struct = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get()))
		{
			TypeStr = TEXT("struct");
			SubCategoryStr = Struct->GetName();
		}
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		if (UClass* Class = Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get()))
		{
			TypeStr = TEXT("object");
			SubCategoryStr = Class->GetName();
		}
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
	{
		TypeStr = TEXT("class");
	}
	else
	{
		TypeStr = Pin->PinType.PinCategory.ToString();
	}

	PinObj->SetStringField(TEXT("type"), TypeStr);
	if (!SubCategoryStr.IsEmpty())
	{
		PinObj->SetStringField(TEXT("sub_type"), SubCategoryStr);
	}

	// Default value
	if (!Pin->DefaultValue.IsEmpty())
	{
		PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
	}
	if (!Pin->AutogeneratedDefaultValue.IsEmpty())
	{
		PinObj->SetStringField(TEXT("auto_default_value"), Pin->AutogeneratedDefaultValue);
	}

	// Connection info
	PinObj->SetBoolField(TEXT("is_connected"), Pin->LinkedTo.Num() > 0);
	PinObj->SetNumberField(TEXT("connection_count"), Pin->LinkedTo.Num());

	// Connected to
	if (Pin->LinkedTo.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ConnectedTo;
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (LinkedPin && LinkedPin->GetOwningNode())
			{
				TSharedPtr<FJsonObject> LinkObj = MakeShared<FJsonObject>();
				LinkObj->SetStringField(TEXT("node_id"), GetNodeId(LinkedPin->GetOwningNode()));
				LinkObj->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
				ConnectedTo.Add(MakeShared<FJsonValueObject>(LinkObj));
			}
		}
		PinObj->SetArrayField(TEXT("connected_to"), ConnectedTo);
	}

	return PinObj;
}

TSharedPtr<FJsonObject> FAnimGraphEditor::GetTransitionGraphNodes(
	UEdGraph* TransitionGraph,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!TransitionGraph)
	{
		OutError = TEXT("Invalid transition graph");
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("graph_name"), TransitionGraph->GetName());

	TArray<TSharedPtr<FJsonValue>> NodesArray;

	for (UEdGraphNode* Node : TransitionGraph->Nodes)
	{
		if (!Node) continue;

		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();

		// Basic node info
		FString NodeId = GetNodeId(Node);
		NodeObj->SetStringField(TEXT("node_id"), NodeId.IsEmpty() ? TEXT("(unnamed)") : NodeId);
		NodeObj->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
		NodeObj->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
		NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);

		// Check if this is the result node
		bool bIsResultNode = Node->IsA<UAnimGraphNode_TransitionResult>();
		NodeObj->SetBoolField(TEXT("is_result_node"), bIsResultNode);

		// Detailed pins
		TArray<TSharedPtr<FJsonValue>> InputPins;
		TArray<TSharedPtr<FJsonValue>> OutputPins;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;

			TSharedPtr<FJsonObject> PinObj = SerializeDetailedPinInfo(Pin);

			if (Pin->Direction == EGPD_Input)
			{
				InputPins.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			else
			{
				OutputPins.Add(MakeShared<FJsonValueObject>(PinObj));
			}
		}

		NodeObj->SetArrayField(TEXT("input_pins"), InputPins);
		NodeObj->SetArrayField(TEXT("output_pins"), OutputPins);

		NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	Result->SetArrayField(TEXT("nodes"), NodesArray);
	Result->SetNumberField(TEXT("node_count"), NodesArray.Num());

	return Result;
}

TSharedPtr<FJsonObject> FAnimGraphEditor::GetAllTransitionNodes(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!AnimBP)
	{
		OutError = TEXT("Invalid Animation Blueprint");
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	// Find state machine
	UAnimGraphNode_StateMachine* StateMachine = FAnimStateMachineEditor::FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!StateMachine)
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	FString SMName = StateMachine->GetStateMachineName();
	TArray<UAnimStateTransitionNode*> Transitions = FAnimStateMachineEditor::GetAllTransitions(AnimBP, SMName, OutError);

	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state_machine"), StateMachineName);

	TArray<TSharedPtr<FJsonValue>> TransitionsArray;

	for (UAnimStateTransitionNode* Transition : Transitions)
	{
		if (!Transition) continue;

		TSharedPtr<FJsonObject> TransitionObj = MakeShared<FJsonObject>();

		FString FromState, ToState;
		if (Transition->GetPreviousState())
		{
			FromState = Transition->GetPreviousState()->GetStateName();
		}
		if (Transition->GetNextState())
		{
			ToState = Transition->GetNextState()->GetStateName();
		}

		TransitionObj->SetStringField(TEXT("from_state"), FromState);
		TransitionObj->SetStringField(TEXT("to_state"), ToState);
		TransitionObj->SetStringField(TEXT("transition_name"), FString::Printf(TEXT("%s -> %s"), *FromState, *ToState));

		UEdGraph* TransGraph = FAnimStateMachineEditor::GetTransitionGraph(Transition, OutError);
		if (TransGraph)
		{
			TSharedPtr<FJsonObject> NodesInfo = GetTransitionGraphNodes(TransGraph, OutError);
			TransitionObj->SetObjectField(TEXT("graph"), NodesInfo);
		}

		TransitionsArray.Add(MakeShared<FJsonValueObject>(TransitionObj));
	}

	Result->SetArrayField(TEXT("transitions"), TransitionsArray);
	Result->SetNumberField(TEXT("transition_count"), TransitionsArray.Num());

	return Result;
}

bool FAnimGraphEditor::ValidatePinValueType(
	UEdGraphPin* Pin,
	const FString& Value,
	FString& OutError)
{
	return FAnimNodePinUtils::ValidatePinValueType(Pin, Value, OutError);
}

bool FAnimGraphEditor::SetPinDefaultValueWithValidation(
	UEdGraph* Graph,
	const FString& NodeId,
	const FString& PinName,
	const FString& Value,
	FString& OutError)
{
	return FAnimNodePinUtils::SetPinDefaultValueWithValidation(Graph, NodeId, PinName, Value, OutError);
}

TSharedPtr<FJsonObject> FAnimGraphEditor::CreateComparisonChain(
	UAnimBlueprint* AnimBP,
	UEdGraph* TransitionGraph,
	const FString& VariableName,
	const FString& ComparisonType,
	const FString& CompareValue,
	FVector2D Position,
	FString& OutError)
{
	return FAnimTransitionConditionFactory::CreateComparisonChain(
		AnimBP, TransitionGraph, VariableName, ComparisonType, CompareValue, Position, OutError);
}
