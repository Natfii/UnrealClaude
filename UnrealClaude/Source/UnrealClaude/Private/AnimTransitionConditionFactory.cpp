// Copyright Natali Caggiano. All Rights Reserved.

#include "AnimTransitionConditionFactory.h"
#include "AnimNodePinUtils.h"
#include "AnimGraphEditor.h"
#include "Animation/AnimBlueprint.h"
#include "AnimationTransitionGraph.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateNodeBase.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_TransitionResult.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_TransitionRuleGetter.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Kismet/KismetMathLibrary.h"

UEdGraphNode* FAnimTransitionConditionFactory::CreateTransitionConditionNode(
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
		FString ComparisonOp = TEXT("Less"); // Default
		if (Params.IsValid() && Params->HasField(TEXT("comparison")))
		{
			ComparisonOp = Params->GetStringField(TEXT("comparison"));
		}
		Node = CreateComparisonNode(TransitionGraph, ComparisonOp, Params, Position, OutError);
	}
	else if (NormalizedType == TEXT("comparebool") || NormalizedType == TEXT("compare_bool"))
	{
		Node = CreateComparisonNode(TransitionGraph, TEXT("Equal"), Params, Position, OutError, true);
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
	else if (NormalizedType == TEXT("greater"))
	{
		Node = CreateComparisonNode(TransitionGraph, TEXT("Greater"), Params, Position, OutError);
	}
	else if (NormalizedType == TEXT("less"))
	{
		Node = CreateComparisonNode(TransitionGraph, TEXT("Less"), Params, Position, OutError);
	}
	else if (NormalizedType == TEXT("greaterequal") || NormalizedType == TEXT("greater_equal"))
	{
		Node = CreateComparisonNode(TransitionGraph, TEXT("GreaterEqual"), Params, Position, OutError);
	}
	else if (NormalizedType == TEXT("lessequal") || NormalizedType == TEXT("less_equal"))
	{
		Node = CreateComparisonNode(TransitionGraph, TEXT("LessEqual"), Params, Position, OutError);
	}
	else if (NormalizedType == TEXT("equal"))
	{
		Node = CreateComparisonNode(TransitionGraph, TEXT("Equal"), Params, Position, OutError);
	}
	else if (NormalizedType == TEXT("notequal") || NormalizedType == TEXT("not_equal"))
	{
		Node = CreateComparisonNode(TransitionGraph, TEXT("NotEqual"), Params, Position, OutError);
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown condition node type: %s. Supported: TimeRemaining, CompareFloat, CompareBool, Greater, Less, GreaterEqual, LessEqual, Equal, NotEqual, And, Or, Not"), *NodeType);
		return nullptr;
	}

	if (Node)
	{
		OutNodeId = FAnimGraphEditor::GenerateAnimNodeId(TEXT("Cond"), NodeType, TransitionGraph);
		FAnimGraphEditor::SetNodeId(Node, OutNodeId);
		TransitionGraph->Modify();
	}

	return Node;
}

bool FAnimTransitionConditionFactory::ConnectTransitionNodes(
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
	UEdGraphNode* SourceNode = FAnimGraphEditor::FindNodeById(TransitionGraph, SourceNodeId);
	if (!SourceNode)
	{
		OutError = FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId);
		return false;
	}

	// Find target node
	UEdGraphNode* TargetNode = FAnimGraphEditor::FindNodeById(TransitionGraph, TargetNodeId);
	if (!TargetNode)
	{
		OutError = FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId);
		return false;
	}

	// Find source pin with fallback
	UEdGraphPin* SourcePin = FAnimNodePinUtils::FindPinByName(SourceNode, SourcePinName, EGPD_Output);
	if (!SourcePin)
	{
		SourcePin = FAnimNodePinUtils::FindPinByName(SourceNode, TEXT("ReturnValue"), EGPD_Output);
	}
	if (!SourcePin)
	{
		SourcePin = FAnimNodePinUtils::FindPinByName(SourceNode, TEXT("Result"), EGPD_Output);
	}
	if (!SourcePin)
	{
		OutError = FAnimNodePinUtils::BuildAvailablePinsError(SourceNode, EGPD_Output, SourcePinName);
		return false;
	}

	// Find target pin with fallback
	UEdGraphPin* TargetPin = FAnimNodePinUtils::FindPinByName(TargetNode, TargetPinName, EGPD_Input);
	if (!TargetPin)
	{
		OutError = FAnimNodePinUtils::BuildAvailablePinsError(TargetNode, EGPD_Input, TargetPinName);
		return false;
	}

	// Make connection
	SourcePin->MakeLinkTo(TargetPin);
	TransitionGraph->Modify();

	return true;
}

bool FAnimTransitionConditionFactory::ConnectToTransitionResult(
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
	UEdGraphNode* ConditionNode = FAnimGraphEditor::FindNodeById(TransitionGraph, ConditionNodeId);
	if (!ConditionNode)
	{
		OutError = FString::Printf(TEXT("Condition node not found: %s"), *ConditionNodeId);
		return false;
	}

	// Find condition output pin using fallback strategy
	TArray<FName> ConditionPinNames;
	if (!ConditionPinName.IsEmpty())
	{
		ConditionPinNames.Add(FName(*ConditionPinName));
	}
	ConditionPinNames.Append({ FName("ReturnValue"), FName("Result"), FName("Output") });

	FPinSearchConfig ConditionConfig;
	ConditionConfig.PreferredNames = ConditionPinNames;
	ConditionConfig.Direction = EGPD_Output;
	ConditionConfig.FallbackCategory = UEdGraphSchema_K2::PC_Boolean;

	UEdGraphPin* ConditionPin = FAnimNodePinUtils::FindPinWithFallbacks(ConditionNode, ConditionConfig, &OutError);
	if (!ConditionPin)
	{
		return false;
	}

	// Find result node
	UEdGraphNode* ResultNode = FAnimNodePinUtils::FindResultNode(TransitionGraph);
	if (!ResultNode)
	{
		OutError = TEXT("Transition result node not found");
		return false;
	}

	// Find transition result input pin
	auto ResultConfig = FPinSearchConfig::Input({
		FName("bCanEnterTransition"),
		FName("CanEnterTransition"),
		FName("Result")
	}).WithCategory(UEdGraphSchema_K2::PC_Boolean).AcceptAny();

	UEdGraphPin* ResultPin = FAnimNodePinUtils::FindPinWithFallbacks(ResultNode, ResultConfig, &OutError);
	if (!ResultPin)
	{
		return false;
	}

	// Make connection
	ConditionPin->MakeLinkTo(ResultPin);
	TransitionGraph->Modify();

	return true;
}

TSharedPtr<FJsonObject> FAnimTransitionConditionFactory::CreateComparisonChain(
	UAnimBlueprint* AnimBP,
	UEdGraph* TransitionGraph,
	const FString& VariableName,
	const FString& ComparisonType,
	const FString& CompareValue,
	FVector2D Position,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	auto MakeErrorResult = [&Result](const FString& Error) -> TSharedPtr<FJsonObject> {
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), Error);
		return Result;
	};

	if (!AnimBP || !TransitionGraph)
	{
		OutError = TEXT("Invalid AnimBlueprint or transition graph");
		return MakeErrorResult(OutError);
	}

	// Step 1: Create GetVariable node
	UEdGraphNode* GetVarNode = CreateVariableGetNode(TransitionGraph, AnimBP, VariableName, Position, OutError);
	if (!GetVarNode)
	{
		return MakeErrorResult(OutError);
	}
	FString GetVarNodeId = FAnimGraphEditor::GenerateAnimNodeId(TEXT("GetVar"), VariableName, TransitionGraph);
	FAnimGraphEditor::SetNodeId(GetVarNode, GetVarNodeId);
	GetVarNode->AllocateDefaultPins();

	// Find variable output pin and check if it's boolean
	auto VarOutputConfig = FPinSearchConfig::Output({}).AcceptAny();
	UEdGraphPin* VarOutputPin = FAnimNodePinUtils::FindPinWithFallbacks(GetVarNode, VarOutputConfig);
	bool bIsBooleanVariable = VarOutputPin && VarOutputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean;

	// Step 2: Create Comparison node
	FVector2D CompPos(Position.X + 200, Position.Y);
	TSharedPtr<FJsonObject> CompParams = MakeShared<FJsonObject>();
	UEdGraphNode* CompNode = CreateComparisonNode(TransitionGraph, ComparisonType, CompParams, CompPos, OutError, bIsBooleanVariable);
	if (!CompNode)
	{
		return MakeErrorResult(OutError);
	}
	FString CompNodeId = FAnimGraphEditor::GenerateAnimNodeId(TEXT("Comp"), ComparisonType, TransitionGraph);
	FAnimGraphEditor::SetNodeId(CompNode, CompNodeId);

	// Step 3: Connect variable to comparison input A
	auto CompInputConfig = FPinSearchConfig::Input({ FName("A") }).AcceptAny();
	UEdGraphPin* CompInputA = FAnimNodePinUtils::FindPinWithFallbacks(CompNode, CompInputConfig);
	if (VarOutputPin && CompInputA)
	{
		VarOutputPin->MakeLinkTo(CompInputA);
	}

	// Step 4: Set comparison value on pin B
	UEdGraphPin* CompInputB = FAnimNodePinUtils::FindPinByName(CompNode, TEXT("B"), EGPD_Input);
	if (CompInputB && !CompareValue.IsEmpty())
	{
		const UEdGraphSchema* Schema = TransitionGraph->GetSchema();
		if (Schema)
		{
			Schema->TrySetDefaultValue(*CompInputB, CompareValue);
		}
		else
		{
			CompInputB->DefaultValue = CompareValue;
		}
	}

	// Step 5: Find result node and its input pin
	UEdGraphNode* ResultNode = FAnimNodePinUtils::FindResultNode(TransitionGraph);
	UEdGraphPin* ResultInputPin = nullptr;
	UEdGraphPin* ExistingConnection = nullptr;

	if (ResultNode)
	{
		auto ResultConfig = FPinSearchConfig::Input({
			FName("bCanEnterTransition"),
			FName("CanEnterTransition")
		}).WithCategory(UEdGraphSchema_K2::PC_Boolean);

		ResultInputPin = FAnimNodePinUtils::FindPinWithFallbacks(ResultNode, ResultConfig);

		if (ResultInputPin && ResultInputPin->LinkedTo.Num() > 0)
		{
			ExistingConnection = ResultInputPin->LinkedTo[0];
		}
	}

	// Get comparison output pin
	auto CompOutputConfig = FPinSearchConfig::Output({
		FName("ReturnValue")
	}).WithCategory(UEdGraphSchema_K2::PC_Boolean);
	UEdGraphPin* CompOutputPin = FAnimNodePinUtils::FindPinWithFallbacks(CompNode, CompOutputConfig);

	// Step 6: Connect to result (with AND chaining if needed)
	FString AndNodeId;
	if (ExistingConnection && CompOutputPin && ResultInputPin)
	{
		// Create AND node for chaining
		FVector2D AndPos(Position.X + 400, Position.Y);
		UEdGraphNode* AndNode = CreateLogicNode(TransitionGraph, TEXT("And"), AndPos, OutError);
		if (AndNode)
		{
			AndNodeId = FAnimGraphEditor::GenerateAnimNodeId(TEXT("And"), TEXT("Chain"), TransitionGraph);
			FAnimGraphEditor::SetNodeId(AndNode, AndNodeId);

			ResultInputPin->BreakAllPinLinks();

			UEdGraphPin* AndInputA = FAnimNodePinUtils::FindPinByName(AndNode, TEXT("A"), EGPD_Input);
			UEdGraphPin* AndInputB = FAnimNodePinUtils::FindPinByName(AndNode, TEXT("B"), EGPD_Input);
			auto AndOutputConfig = FPinSearchConfig::Output({
				FName("ReturnValue")
			}).WithCategory(UEdGraphSchema_K2::PC_Boolean);
			UEdGraphPin* AndOutput = FAnimNodePinUtils::FindPinWithFallbacks(AndNode, AndOutputConfig);

			if (AndInputA) ExistingConnection->MakeLinkTo(AndInputA);
			if (AndInputB) CompOutputPin->MakeLinkTo(AndInputB);
			if (AndOutput) AndOutput->MakeLinkTo(ResultInputPin);
		}
	}
	else if (CompOutputPin && ResultInputPin)
	{
		ResultInputPin->BreakAllPinLinks();
		CompOutputPin->MakeLinkTo(ResultInputPin);
	}

	TransitionGraph->Modify();

	// Build success result
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("variable_node_id"), GetVarNodeId);
	Result->SetStringField(TEXT("comparison_node_id"), CompNodeId);
	Result->SetBoolField(TEXT("chained_with_existing"), !AndNodeId.IsEmpty());
	if (!AndNodeId.IsEmpty())
	{
		Result->SetStringField(TEXT("and_node_id"), AndNodeId);
	}
	Result->SetStringField(TEXT("variable"), VariableName);
	Result->SetStringField(TEXT("comparison"), ComparisonType);
	Result->SetStringField(TEXT("value"), CompareValue);

	return Result;
}

// ===== Private Helpers =====

UEdGraphNode* FAnimTransitionConditionFactory::CreateTimeRemainingNode(
	UEdGraph* Graph,
	const TSharedPtr<FJsonObject>& Params,
	FVector2D Position,
	FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Invalid graph");
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_TransitionRuleGetter> NodeCreator(*Graph);
	UK2Node_TransitionRuleGetter* GetterNode = NodeCreator.CreateNode();

	if (!GetterNode)
	{
		OutError = TEXT("Failed to create TimeRemaining getter node");
		return nullptr;
	}

	GetterNode->NodePosX = static_cast<int32>(Position.X);
	GetterNode->NodePosY = static_cast<int32>(Position.Y);
	GetterNode->GetterType = ETransitionGetter::AnimationAsset_GetTimeFromEnd;

	// Try to find an associated animation player node in the source state
	UAnimationTransitionGraph* TransitionGraph = Cast<UAnimationTransitionGraph>(Graph);
	if (TransitionGraph)
	{
		UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(TransitionGraph->GetOuter());
		if (TransitionNode)
		{
			UAnimStateNodeBase* PreviousState = TransitionNode->GetPreviousState();
			UEdGraph* StateGraph = PreviousState ? PreviousState->GetBoundGraph() : nullptr;
			if (StateGraph)
			{
				for (UEdGraphNode* Node : StateGraph->Nodes)
				{
					if (UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node))
					{
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
	GetterNode->AllocateDefaultPins();

	return GetterNode;
}

UEdGraphNode* FAnimTransitionConditionFactory::CreateComparisonNode(
	UEdGraph* Graph,
	const FString& ComparisonType,
	const TSharedPtr<FJsonObject>& Params,
	FVector2D Position,
	FString& OutError,
	bool bIsBooleanType)
{
	if (!Graph)
	{
		OutError = TEXT("Invalid graph");
		return nullptr;
	}

	FName FunctionName;

	if (bIsBooleanType)
	{
		if (ComparisonType.Equals(TEXT("Equal"), ESearchCase::IgnoreCase))
		{
			FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_BoolBool);
		}
		else if (ComparisonType.Equals(TEXT("NotEqual"), ESearchCase::IgnoreCase))
		{
			FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_BoolBool);
		}
		else
		{
			OutError = FString::Printf(TEXT("Boolean variables only support Equal/NotEqual comparisons, not '%s'."), *ComparisonType);
			return nullptr;
		}
	}
	else
	{
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
			OutError = FString::Printf(TEXT("Unknown comparison type: %s"), *ComparisonType);
			return nullptr;
		}
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
	if (!CallNode)
	{
		OutError = TEXT("Failed to create comparison call function node");
		return nullptr;
	}

	CallNode->FunctionReference.SetExternalMember(FunctionName, UKismetMathLibrary::StaticClass());
	CallNode->NodePosX = static_cast<int32>(Position.X);
	CallNode->NodePosY = static_cast<int32>(Position.Y);

	Graph->AddNode(CallNode, false, false);
	CallNode->ReconstructNode();

	return CallNode;
}

UEdGraphNode* FAnimTransitionConditionFactory::CreateLogicNode(
	UEdGraph* Graph,
	const FString& LogicType,
	FVector2D Position,
	FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Invalid graph");
		return nullptr;
	}

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
		OutError = FString::Printf(TEXT("Unknown logic type: %s"), *LogicType);
		return nullptr;
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
	if (!CallNode)
	{
		OutError = TEXT("Failed to create logic call function node");
		return nullptr;
	}

	CallNode->FunctionReference.SetExternalMember(FunctionName, UKismetMathLibrary::StaticClass());
	CallNode->NodePosX = static_cast<int32>(Position.X);
	CallNode->NodePosY = static_cast<int32>(Position.Y);

	Graph->AddNode(CallNode, false, false);
	CallNode->ReconstructNode();

	return CallNode;
}

UEdGraphNode* FAnimTransitionConditionFactory::CreateVariableGetNode(
	UEdGraph* Graph,
	UAnimBlueprint* AnimBP,
	const FString& VariableName,
	FVector2D Position,
	FString& OutError)
{
	if (!Graph || !AnimBP)
	{
		OutError = TEXT("Invalid graph or AnimBlueprint");
		return nullptr;
	}

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

	UK2Node_VariableGet* VarNode = NewObject<UK2Node_VariableGet>(Graph);
	if (!VarNode)
	{
		OutError = TEXT("Failed to create variable get node");
		return nullptr;
	}

	VarNode->VariableReference.SetSelfMember(FName(*VariableName));
	VarNode->NodePosX = static_cast<int32>(Position.X);
	VarNode->NodePosY = static_cast<int32>(Position.Y);

	Graph->AddNode(VarNode, false, false);
	VarNode->ReconstructNode();

	return VarNode;
}
