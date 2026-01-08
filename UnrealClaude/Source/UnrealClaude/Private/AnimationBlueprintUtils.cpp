// Copyright Natali Caggiano. All Rights Reserved.

#include "AnimationBlueprintUtils.h"
#include "AnimTransitionConditionFactory.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace1D.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimationStateMachineGraph.h"
#include "AnimStateEntryNode.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_TransitionRuleGetter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Internationalization/Regex.h"

// ===== AnimBlueprint Access (Level 1) =====

UAnimBlueprint* FAnimationBlueprintUtils::LoadAnimBlueprint(const FString& BlueprintPath, FString& OutError)
{
	if (BlueprintPath.IsEmpty())
	{
		OutError = TEXT("Blueprint path is empty");
		return nullptr;
	}

	// Try to load the asset
	UObject* LoadedAsset = StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath);

	if (!LoadedAsset)
	{
		// Try with different path formats
		FString AdjustedPath = BlueprintPath;
		if (!AdjustedPath.StartsWith(TEXT("/")))
		{
			AdjustedPath = TEXT("/Game/") + AdjustedPath;
		}
		if (!AdjustedPath.EndsWith(TEXT(".")) && !AdjustedPath.Contains(TEXT(".")))
		{
			AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(AdjustedPath);
		}

		LoadedAsset = StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *AdjustedPath);
	}

	if (!LoadedAsset)
	{
		OutError = FString::Printf(TEXT("Failed to load Animation Blueprint: %s"), *BlueprintPath);
		return nullptr;
	}

	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(LoadedAsset);
	if (!AnimBP)
	{
		OutError = FString::Printf(TEXT("Asset is not an Animation Blueprint: %s"), *BlueprintPath);
		return nullptr;
	}

	return AnimBP;
}

bool FAnimationBlueprintUtils::IsAnimationBlueprint(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->IsA<UAnimBlueprint>();
}

bool FAnimationBlueprintUtils::CompileAnimBlueprint(UAnimBlueprint* AnimBP, FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("AnimBlueprint is null");
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(AnimBP);

	if (AnimBP->Status == BS_Error)
	{
		OutError = TEXT("Animation Blueprint compilation failed with errors");
		return false;
	}

	// Auto-save after successful compile
	FString AssetPath = AnimBP->GetPathName();
	if (!UEditorAssetLibrary::SaveAsset(AssetPath, false))
	{
		UE_LOG(LogTemp, Warning, TEXT("CompileAnimBlueprint: Failed to auto-save asset %s"), *AssetPath);
	}

	return true;
}

void FAnimationBlueprintUtils::MarkAnimBlueprintModified(UAnimBlueprint* AnimBP)
{
	if (AnimBP)
	{
		AnimBP->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	}
}

bool FAnimationBlueprintUtils::ValidateAnimBlueprintForOperation(UAnimBlueprint* AnimBP, FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("AnimBlueprint is null");
		return false;
	}

	if (!AnimBP->TargetSkeleton)
	{
		OutError = TEXT("AnimBlueprint has no target skeleton");
		return false;
	}

	return true;
}

// ===== State Machine Operations (Level 2) =====

UAnimGraphNode_StateMachine* FAnimationBlueprintUtils::CreateStateMachine(
	UAnimBlueprint* AnimBP,
	const FString& MachineName,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return nullptr;
	}

	UAnimGraphNode_StateMachine* Result = FAnimStateMachineEditor::CreateStateMachine(
		AnimBP, MachineName, Position, OutNodeId, OutError);

	if (Result)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return Result;
}

UAnimGraphNode_StateMachine* FAnimationBlueprintUtils::FindStateMachine(
	UAnimBlueprint* AnimBP,
	const FString& MachineName,
	FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("AnimBlueprint is null");
		return nullptr;
	}

	return FAnimStateMachineEditor::FindStateMachine(AnimBP, MachineName, OutError);
}

TArray<UAnimGraphNode_StateMachine*> FAnimationBlueprintUtils::GetAllStateMachines(UAnimBlueprint* AnimBP)
{
	if (!AnimBP)
	{
		return TArray<UAnimGraphNode_StateMachine*>();
	}

	return FAnimStateMachineEditor::GetAllStateMachines(AnimBP);
}

UAnimationStateMachineGraph* FAnimationBlueprintUtils::GetStateMachineGraph(
	UAnimGraphNode_StateMachine* StateMachineNode,
	FString& OutError)
{
	return FAnimStateMachineEditor::GetStateMachineGraph(StateMachineNode, OutError);
}

// ===== State Operations (Level 3) =====

UAnimStateNode* FAnimationBlueprintUtils::AddState(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	FVector2D Position,
	bool bIsEntryState,
	FString& OutNodeId,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return nullptr;
	}

	UAnimStateNode* Result = FAnimStateMachineEditor::AddState(
		AnimBP, StateMachineName, StateName, Position, bIsEntryState, OutNodeId, OutError);

	if (Result)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return Result;
}

bool FAnimationBlueprintUtils::RemoveState(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	bool bResult = FAnimStateMachineEditor::RemoveState(AnimBP, StateMachineName, StateName, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

UAnimStateNode* FAnimationBlueprintUtils::FindState(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("AnimBlueprint is null");
		return nullptr;
	}

	return FAnimStateMachineEditor::FindState(AnimBP, StateMachineName, StateName, OutError);
}

TArray<UAnimStateNode*> FAnimationBlueprintUtils::GetAllStates(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("AnimBlueprint is null");
		return TArray<UAnimStateNode*>();
	}

	return FAnimStateMachineEditor::GetAllStates(AnimBP, StateMachineName, OutError);
}

bool FAnimationBlueprintUtils::SetEntryState(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("AnimBlueprint is null");
		return false;
	}

	return FAnimStateMachineEditor::SetEntryState(AnimBP, StateMachineName, StateName, OutError);
}

// ===== Transition Operations (Level 3) =====

UAnimStateTransitionNode* FAnimationBlueprintUtils::CreateTransition(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromStateName,
	const FString& ToStateName,
	FString& OutNodeId,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return nullptr;
	}

	UAnimStateTransitionNode* Result = FAnimStateMachineEditor::CreateTransition(
		AnimBP, StateMachineName, FromStateName, ToStateName, OutNodeId, OutError);

	if (Result)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return Result;
}

bool FAnimationBlueprintUtils::RemoveTransition(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromStateName,
	const FString& ToStateName,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	bool bResult = FAnimStateMachineEditor::RemoveTransition(
		AnimBP, StateMachineName, FromStateName, ToStateName, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

UAnimStateTransitionNode* FAnimationBlueprintUtils::FindTransition(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromStateName,
	const FString& ToStateName,
	FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("AnimBlueprint is null");
		return nullptr;
	}

	return FAnimStateMachineEditor::FindTransition(
		AnimBP, StateMachineName, FromStateName, ToStateName, OutError);
}

bool FAnimationBlueprintUtils::SetTransitionDuration(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromStateName,
	const FString& ToStateName,
	float Duration,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	bool bResult = FAnimStateMachineEditor::SetTransitionDuration(
		AnimBP, StateMachineName, FromStateName, ToStateName, Duration, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

bool FAnimationBlueprintUtils::SetTransitionPriority(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromStateName,
	const FString& ToStateName,
	int32 Priority,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	bool bResult = FAnimStateMachineEditor::SetTransitionPriority(
		AnimBP, StateMachineName, FromStateName, ToStateName, Priority, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

TArray<UAnimStateTransitionNode*> FAnimationBlueprintUtils::GetAllTransitions(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("AnimBlueprint is null");
		return TArray<UAnimStateTransitionNode*>();
	}

	return FAnimStateMachineEditor::GetAllTransitions(AnimBP, StateMachineName, OutError);
}

// ===== Transition Condition Graph Operations (Level 4) =====

UEdGraph* FAnimationBlueprintUtils::GetTransitionGraph(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromStateName,
	const FString& ToStateName,
	FString& OutError)
{
	return FAnimGraphEditor::FindTransitionGraph(AnimBP, StateMachineName, FromStateName, ToStateName, OutError);
}

UEdGraphNode* FAnimationBlueprintUtils::AddConditionNode(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromStateName,
	const FString& ToStateName,
	const FString& NodeType,
	const TSharedPtr<FJsonObject>& Params,
	FVector2D Position,
	FString& OutNodeId,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return nullptr;
	}

	// Find transition graph
	UEdGraph* TransitionGraph = FAnimGraphEditor::FindTransitionGraph(
		AnimBP, StateMachineName, FromStateName, ToStateName, OutError);

	if (!TransitionGraph)
	{
		return nullptr;
	}

	UEdGraphNode* Result = FAnimGraphEditor::CreateTransitionConditionNode(
		TransitionGraph, NodeType, Params, Position.X, Position.Y, OutNodeId, OutError);

	if (Result)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return Result;
}

bool FAnimationBlueprintUtils::DeleteConditionNode(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromStateName,
	const FString& ToStateName,
	const FString& NodeId,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	// Find transition graph
	UEdGraph* TransitionGraph = FAnimGraphEditor::FindTransitionGraph(
		AnimBP, StateMachineName, FromStateName, ToStateName, OutError);

	if (!TransitionGraph)
	{
		return false;
	}

	// Find the node by ID
	UEdGraphNode* NodeToDelete = FAnimGraphEditor::FindNodeById(TransitionGraph, NodeId);
	if (!NodeToDelete)
	{
		OutError = FString::Printf(TEXT("Node with ID '%s' not found in transition graph"), *NodeId);
		return false;
	}

	// Don't allow deleting the result node
	UEdGraphNode* ResultNode = FAnimGraphEditor::FindResultNode(TransitionGraph);
	if (NodeToDelete == ResultNode)
	{
		OutError = TEXT("Cannot delete the transition result node");
		return false;
	}

	// Break all connections first
	NodeToDelete->BreakAllNodeLinks();

	// Remove the node from the graph
	TransitionGraph->RemoveNode(NodeToDelete);

	MarkAnimBlueprintModified(AnimBP);

	return true;
}

bool FAnimationBlueprintUtils::ConnectConditionNodes(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromStateName,
	const FString& ToStateName,
	const FString& SourceNodeId,
	const FString& SourcePinName,
	const FString& TargetNodeId,
	const FString& TargetPinName,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	UEdGraph* TransitionGraph = FAnimGraphEditor::FindTransitionGraph(
		AnimBP, StateMachineName, FromStateName, ToStateName, OutError);

	if (!TransitionGraph)
	{
		return false;
	}

	bool bResult = FAnimGraphEditor::ConnectTransitionNodes(
		TransitionGraph, SourceNodeId, SourcePinName, TargetNodeId, TargetPinName, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

bool FAnimationBlueprintUtils::ConnectToTransitionResult(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromStateName,
	const FString& ToStateName,
	const FString& ConditionNodeId,
	const FString& ConditionPinName,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	UEdGraph* TransitionGraph = FAnimGraphEditor::FindTransitionGraph(
		AnimBP, StateMachineName, FromStateName, ToStateName, OutError);

	if (!TransitionGraph)
	{
		return false;
	}

	bool bResult = FAnimGraphEditor::ConnectToTransitionResult(
		TransitionGraph, ConditionNodeId, ConditionPinName, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

// ===== Animation Assignment Operations (Level 5) =====

bool FAnimationBlueprintUtils::SetStateAnimSequence(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	const FString& AnimSequencePath,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	// Load the animation sequence
	UAnimSequence* AnimSequence = FAnimAssetManager::LoadAnimSequence(AnimSequencePath, OutError);
	if (!AnimSequence)
	{
		return false;
	}

	// Validate compatibility
	if (!FAnimAssetManager::ValidateAnimationCompatibility(AnimBP, AnimSequence, OutError))
	{
		return false;
	}

	// Set the animation
	bool bResult = FAnimAssetManager::SetStateAnimSequence(
		AnimBP, StateMachineName, StateName, AnimSequence, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

bool FAnimationBlueprintUtils::SetStateBlendSpace(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	const FString& BlendSpacePath,
	const TMap<FString, FString>& ParameterBindings,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	UBlendSpace* BlendSpace = FAnimAssetManager::LoadBlendSpace(BlendSpacePath, OutError);
	if (!BlendSpace)
	{
		return false;
	}

	if (!FAnimAssetManager::ValidateAnimationCompatibility(AnimBP, Cast<UAnimationAsset>(BlendSpace), OutError))
	{
		return false;
	}

	bool bResult = FAnimAssetManager::SetStateBlendSpace(
		AnimBP, StateMachineName, StateName, BlendSpace, ParameterBindings, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

bool FAnimationBlueprintUtils::SetStateBlendSpace1D(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	const FString& BlendSpacePath,
	const FString& ParameterBinding,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	UBlendSpace1D* BlendSpace = FAnimAssetManager::LoadBlendSpace1D(BlendSpacePath, OutError);
	if (!BlendSpace)
	{
		return false;
	}

	if (!FAnimAssetManager::ValidateAnimationCompatibility(AnimBP, Cast<UAnimationAsset>(BlendSpace), OutError))
	{
		return false;
	}

	bool bResult = FAnimAssetManager::SetStateBlendSpace1D(
		AnimBP, StateMachineName, StateName, BlendSpace, ParameterBinding, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

bool FAnimationBlueprintUtils::SetStateMontage(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	const FString& MontagePath,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	UAnimMontage* Montage = FAnimAssetManager::LoadMontage(MontagePath, OutError);
	if (!Montage)
	{
		return false;
	}

	if (!FAnimAssetManager::ValidateAnimationCompatibility(AnimBP, Montage, OutError))
	{
		return false;
	}

	bool bResult = FAnimAssetManager::SetStateMontage(
		AnimBP, StateMachineName, StateName, Montage, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

TArray<FString> FAnimationBlueprintUtils::FindAnimationAssets(
	const FString& SearchPattern,
	const FString& AssetType,
	UAnimBlueprint* AnimBPForSkeleton)
{
	USkeleton* Skeleton = nullptr;
	if (AnimBPForSkeleton)
	{
		Skeleton = FAnimAssetManager::GetTargetSkeleton(AnimBPForSkeleton);
	}

	return FAnimAssetManager::FindAnimationAssets(SearchPattern, AssetType, Skeleton);
}

// ===== Serialization =====

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::SerializeAnimBlueprintInfo(UAnimBlueprint* AnimBP)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!AnimBP)
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("AnimBlueprint is null"));
		return Result;
	}

	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("name"), AnimBP->GetName());
	Result->SetStringField(TEXT("path"), AnimBP->GetPathName());

	// Skeleton info
	USkeleton* Skeleton = FAnimAssetManager::GetTargetSkeleton(AnimBP);
	if (Skeleton)
	{
		Result->SetStringField(TEXT("skeleton"), Skeleton->GetName());
		Result->SetStringField(TEXT("skeleton_path"), Skeleton->GetPathName());
	}

	// State machines
	TArray<TSharedPtr<FJsonValue>> StateMachinesArray;
	TArray<UAnimGraphNode_StateMachine*> StateMachines = GetAllStateMachines(AnimBP);
	for (UAnimGraphNode_StateMachine* SM : StateMachines)
	{
		TSharedPtr<FJsonObject> SMInfo = FAnimStateMachineEditor::SerializeStateMachineInfo(SM);
		StateMachinesArray.Add(MakeShared<FJsonValueObject>(SMInfo));
	}
	Result->SetArrayField(TEXT("state_machines"), StateMachinesArray);

	return Result;
}

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::SerializeStateMachineInfo(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName)
{
	FString Error;
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, Error);

	if (!SM)
	{
		TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
		ErrorResult->SetBoolField(TEXT("success"), false);
		ErrorResult->SetStringField(TEXT("error"), Error);
		return ErrorResult;
	}

	TSharedPtr<FJsonObject> Result = FAnimStateMachineEditor::SerializeStateMachineInfo(SM);
	Result->SetBoolField(TEXT("success"), true);

	// Add states
	TArray<TSharedPtr<FJsonValue>> StatesArray;
	TArray<UAnimStateNode*> States = GetAllStates(AnimBP, StateMachineName, Error);
	for (UAnimStateNode* State : States)
	{
		StatesArray.Add(MakeShared<FJsonValueObject>(SerializeStateInfo(State)));
	}
	Result->SetArrayField(TEXT("states"), StatesArray);

	// Add transitions
	TArray<TSharedPtr<FJsonValue>> TransitionsArray;
	TArray<UAnimStateTransitionNode*> Transitions = GetAllTransitions(AnimBP, StateMachineName, Error);
	for (UAnimStateTransitionNode* Transition : Transitions)
	{
		TransitionsArray.Add(MakeShared<FJsonValueObject>(SerializeTransitionInfo(Transition)));
	}
	Result->SetArrayField(TEXT("transitions"), TransitionsArray);

	return Result;
}

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::SerializeStateInfo(UAnimStateNode* StateNode)
{
	return FAnimStateMachineEditor::SerializeStateInfo(StateNode);
}

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::SerializeTransitionInfo(UAnimStateTransitionNode* TransitionNode)
{
	return FAnimStateMachineEditor::SerializeTransitionInfo(TransitionNode);
}

// ===== Batch Operations =====

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::ExecuteBatchOperations(
	UAnimBlueprint* AnimBP,
	const TArray<TSharedPtr<FJsonValue>>& Operations,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	int32 SuccessCount = 0;
	int32 FailureCount = 0;

	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	for (const TSharedPtr<FJsonValue>& OpValue : Operations)
	{
		TSharedPtr<FJsonObject> OpResult = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* OpObj;

		if (!OpValue->TryGetObject(OpObj) || !OpObj->IsValid())
		{
			OpResult->SetBoolField(TEXT("success"), false);
			OpResult->SetStringField(TEXT("error"), TEXT("Invalid operation format"));
			ResultsArray.Add(MakeShared<FJsonValueObject>(OpResult));
			FailureCount++;
			continue;
		}

		FString OpType = (*OpObj)->GetStringField(TEXT("type"));
		const TSharedPtr<FJsonObject>* ParamsObj;
		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
		if ((*OpObj)->TryGetObjectField(TEXT("params"), ParamsObj))
		{
			Params = *ParamsObj;
		}

		FString OpError;
		bool bOpSuccess = false;

		// Execute operation based on type
		if (OpType == TEXT("add_state"))
		{
			FString NodeId;
			UAnimStateNode* State = AddState(
				AnimBP,
				Params->GetStringField(TEXT("state_machine")),
				Params->GetStringField(TEXT("state_name")),
				FVector2D(Params->GetNumberField(TEXT("x")), Params->GetNumberField(TEXT("y"))),
				Params->GetBoolField(TEXT("is_entry")),
				NodeId,
				OpError
			);
			bOpSuccess = (State != nullptr);
			if (bOpSuccess)
			{
				OpResult->SetStringField(TEXT("node_id"), NodeId);
			}
		}
		else if (OpType == TEXT("remove_state"))
		{
			bOpSuccess = RemoveState(
				AnimBP,
				Params->GetStringField(TEXT("state_machine")),
				Params->GetStringField(TEXT("state_name")),
				OpError
			);
		}
		else if (OpType == TEXT("add_transition"))
		{
			FString NodeId;
			UAnimStateTransitionNode* Transition = CreateTransition(
				AnimBP,
				Params->GetStringField(TEXT("state_machine")),
				Params->GetStringField(TEXT("from_state")),
				Params->GetStringField(TEXT("to_state")),
				NodeId,
				OpError
			);
			bOpSuccess = (Transition != nullptr);
			if (bOpSuccess)
			{
				OpResult->SetStringField(TEXT("node_id"), NodeId);
			}
		}
		else if (OpType == TEXT("remove_transition"))
		{
			bOpSuccess = RemoveTransition(
				AnimBP,
				Params->GetStringField(TEXT("state_machine")),
				Params->GetStringField(TEXT("from_state")),
				Params->GetStringField(TEXT("to_state")),
				OpError
			);
		}
		else if (OpType == TEXT("set_transition_duration"))
		{
			bOpSuccess = SetTransitionDuration(
				AnimBP,
				Params->GetStringField(TEXT("state_machine")),
				Params->GetStringField(TEXT("from_state")),
				Params->GetStringField(TEXT("to_state")),
				Params->GetNumberField(TEXT("duration")),
				OpError
			);
		}
		else if (OpType == TEXT("set_state_animation"))
		{
			FString AnimType = Params->GetStringField(TEXT("animation_type"));
			if (AnimType == TEXT("sequence") || AnimType.IsEmpty())
			{
				bOpSuccess = SetStateAnimSequence(
					AnimBP,
					Params->GetStringField(TEXT("state_machine")),
					Params->GetStringField(TEXT("state_name")),
					Params->GetStringField(TEXT("animation_path")),
					OpError
				);
			}
			else if (AnimType == TEXT("blendspace"))
			{
				TMap<FString, FString> Bindings;
				const TSharedPtr<FJsonObject>* BindingsObj;
				if (Params->TryGetObjectField(TEXT("parameter_bindings"), BindingsObj))
				{
					for (const auto& Pair : (*BindingsObj)->Values)
					{
						Bindings.Add(Pair.Key, Pair.Value->AsString());
					}
				}
				bOpSuccess = SetStateBlendSpace(
					AnimBP,
					Params->GetStringField(TEXT("state_machine")),
					Params->GetStringField(TEXT("state_name")),
					Params->GetStringField(TEXT("animation_path")),
					Bindings,
					OpError
				);
			}
			else if (AnimType == TEXT("blendspace1d"))
			{
				bOpSuccess = SetStateBlendSpace1D(
					AnimBP,
					Params->GetStringField(TEXT("state_machine")),
					Params->GetStringField(TEXT("state_name")),
					Params->GetStringField(TEXT("animation_path")),
					Params->GetStringField(TEXT("parameter_binding")),
					OpError
				);
			}
			else if (AnimType == TEXT("montage"))
			{
				bOpSuccess = SetStateMontage(
					AnimBP,
					Params->GetStringField(TEXT("state_machine")),
					Params->GetStringField(TEXT("state_name")),
					Params->GetStringField(TEXT("animation_path")),
					OpError
				);
			}
		}
		else if (OpType == TEXT("add_comparison_chain"))
		{
			FVector2D Position(
				Params->GetNumberField(TEXT("x")),
				Params->GetNumberField(TEXT("y"))
			);
			TSharedPtr<FJsonObject> ChainResult = AddComparisonChain(
				AnimBP,
				Params->GetStringField(TEXT("state_machine")),
				Params->GetStringField(TEXT("from_state")),
				Params->GetStringField(TEXT("to_state")),
				Params->GetStringField(TEXT("variable_name")),
				Params->GetStringField(TEXT("comparison_type")),
				Params->GetStringField(TEXT("compare_value")),
				Position,
				OpError
			);
			bOpSuccess = ChainResult.IsValid() && ChainResult->GetBoolField(TEXT("success"));
			if (bOpSuccess)
			{
				// Include node IDs in the result
				if (ChainResult->HasField(TEXT("variable_node_id")))
				{
					OpResult->SetStringField(TEXT("variable_node_id"), ChainResult->GetStringField(TEXT("variable_node_id")));
				}
				if (ChainResult->HasField(TEXT("comparison_node_id")))
				{
					OpResult->SetStringField(TEXT("comparison_node_id"), ChainResult->GetStringField(TEXT("comparison_node_id")));
				}
			}
			else if (ChainResult.IsValid() && ChainResult->HasField(TEXT("error")))
			{
				OpError = ChainResult->GetStringField(TEXT("error"));
			}
		}
		else if (OpType == TEXT("add_condition_node"))
		{
			FVector2D Position(
				Params->GetNumberField(TEXT("x")),
				Params->GetNumberField(TEXT("y"))
			);
			FString NodeId;
			TSharedPtr<FJsonObject> NodeParams;
			const TSharedPtr<FJsonObject>* NodeParamsPtr;
			if (Params->TryGetObjectField(TEXT("node_params"), NodeParamsPtr))
			{
				NodeParams = *NodeParamsPtr;
			}
			UEdGraphNode* Node = AddConditionNode(
				AnimBP,
				Params->GetStringField(TEXT("state_machine")),
				Params->GetStringField(TEXT("from_state")),
				Params->GetStringField(TEXT("to_state")),
				Params->GetStringField(TEXT("node_type")),
				NodeParams,
				Position,
				NodeId,
				OpError
			);
			bOpSuccess = (Node != nullptr);
			if (bOpSuccess)
			{
				OpResult->SetStringField(TEXT("node_id"), NodeId);
			}
		}
		else if (OpType == TEXT("connect_condition_nodes"))
		{
			bOpSuccess = ConnectConditionNodes(
				AnimBP,
				Params->GetStringField(TEXT("state_machine")),
				Params->GetStringField(TEXT("from_state")),
				Params->GetStringField(TEXT("to_state")),
				Params->GetStringField(TEXT("source_node_id")),
				Params->GetStringField(TEXT("source_pin")),
				Params->GetStringField(TEXT("target_node_id")),
				Params->GetStringField(TEXT("target_pin")),
				OpError
			);
		}
		else if (OpType == TEXT("connect_to_result"))
		{
			bOpSuccess = ConnectToTransitionResult(
				AnimBP,
				Params->GetStringField(TEXT("state_machine")),
				Params->GetStringField(TEXT("from_state")),
				Params->GetStringField(TEXT("to_state")),
				Params->GetStringField(TEXT("source_node_id")),
				Params->GetStringField(TEXT("source_pin")),
				OpError
			);
		}
		else
		{
			OpError = FString::Printf(TEXT("Unknown operation type: %s"), *OpType);
		}

		OpResult->SetStringField(TEXT("type"), OpType);
		OpResult->SetBoolField(TEXT("success"), bOpSuccess);
		if (!bOpSuccess)
		{
			OpResult->SetStringField(TEXT("error"), OpError);
			FailureCount++;
		}
		else
		{
			SuccessCount++;
		}

		ResultsArray.Add(MakeShared<FJsonValueObject>(OpResult));
	}

	// Compile after all operations
	FString CompileError;
	bool bCompiled = CompileAnimBlueprint(AnimBP, CompileError);

	Result->SetBoolField(TEXT("success"), FailureCount == 0 && bCompiled);
	Result->SetNumberField(TEXT("success_count"), SuccessCount);
	Result->SetNumberField(TEXT("failure_count"), FailureCount);
	Result->SetBoolField(TEXT("compiled"), bCompiled);
	if (!bCompiled)
	{
		Result->SetStringField(TEXT("compile_error"), CompileError);
	}
	Result->SetArrayField(TEXT("results"), ResultsArray);

	return Result;
}

// ===== New Operations for MCP Tool Enhancements =====

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::GetTransitionNodes(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
		ErrorResult->SetBoolField(TEXT("success"), false);
		ErrorResult->SetStringField(TEXT("error"), OutError);
		return ErrorResult;
	}

	// If FromState and ToState are empty, get all transitions in state machine
	if (FromState.IsEmpty() && ToState.IsEmpty())
	{
		return FAnimGraphEditor::GetAllTransitionNodes(AnimBP, StateMachineName, OutError);
	}

	// Get single transition graph nodes
	UEdGraph* TransitionGraph = FAnimGraphEditor::FindTransitionGraph(
		AnimBP, StateMachineName, FromState, ToState, OutError);

	if (!TransitionGraph)
	{
		TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
		ErrorResult->SetBoolField(TEXT("success"), false);
		ErrorResult->SetStringField(TEXT("error"), OutError);
		return ErrorResult;
	}

	TSharedPtr<FJsonObject> Result = FAnimGraphEditor::GetTransitionGraphNodes(TransitionGraph, OutError);
	Result->SetStringField(TEXT("from_state"), FromState);
	Result->SetStringField(TEXT("to_state"), ToState);

	return Result;
}

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::InspectNodePins(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	const FString& NodeId,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	// Find transition graph
	UEdGraph* TransitionGraph = FAnimGraphEditor::FindTransitionGraph(
		AnimBP, StateMachineName, FromState, ToState, OutError);

	if (!TransitionGraph)
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	// Find the node
	UEdGraphNode* Node = FAnimGraphEditor::FindNodeById(TransitionGraph, NodeId);
	if (!Node)
	{
		OutError = FString::Printf(TEXT("Node with ID '%s' not found"), *NodeId);
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("node_id"), NodeId);
	Result->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
	Result->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Result->SetNumberField(TEXT("pos_x"), Node->NodePosX);
	Result->SetNumberField(TEXT("pos_y"), Node->NodePosY);

	// Detailed pins with full type info
	TArray<TSharedPtr<FJsonValue>> InputPins;
	TArray<TSharedPtr<FJsonValue>> OutputPins;

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;

		TSharedPtr<FJsonObject> PinObj = FAnimGraphEditor::SerializeDetailedPinInfo(Pin);

		if (Pin->Direction == EGPD_Input)
		{
			InputPins.Add(MakeShared<FJsonValueObject>(PinObj));
		}
		else
		{
			OutputPins.Add(MakeShared<FJsonValueObject>(PinObj));
		}
	}

	Result->SetArrayField(TEXT("input_pins"), InputPins);
	Result->SetArrayField(TEXT("output_pins"), OutputPins);

	return Result;
}

bool FAnimationBlueprintUtils::SetPinDefaultValue(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	const FString& NodeId,
	const FString& PinName,
	const FString& Value,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		return false;
	}

	// Find transition graph
	UEdGraph* TransitionGraph = FAnimGraphEditor::FindTransitionGraph(
		AnimBP, StateMachineName, FromState, ToState, OutError);

	if (!TransitionGraph)
	{
		return false;
	}

	bool bResult = FAnimGraphEditor::SetPinDefaultValueWithValidation(
		TransitionGraph, NodeId, PinName, Value, OutError);

	if (bResult)
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return bResult;
}

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::AddComparisonChain(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& FromState,
	const FString& ToState,
	const FString& VariableName,
	const FString& ComparisonType,
	const FString& CompareValue,
	FVector2D Position,
	FString& OutError)
{
	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
		ErrorResult->SetBoolField(TEXT("success"), false);
		ErrorResult->SetStringField(TEXT("error"), OutError);
		return ErrorResult;
	}

	// Find transition graph
	UEdGraph* TransitionGraph = FAnimGraphEditor::FindTransitionGraph(
		AnimBP, StateMachineName, FromState, ToState, OutError);

	if (!TransitionGraph)
	{
		TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
		ErrorResult->SetBoolField(TEXT("success"), false);
		ErrorResult->SetStringField(TEXT("error"), OutError);
		return ErrorResult;
	}

	TSharedPtr<FJsonObject> Result = FAnimGraphEditor::CreateComparisonChain(
		AnimBP, TransitionGraph, VariableName, ComparisonType, CompareValue, Position, OutError);

	if (Result->GetBoolField(TEXT("success")))
	{
		MarkAnimBlueprintModified(AnimBP);
	}

	return Result;
}

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::ValidateBlueprint(
	UAnimBlueprint* AnimBP,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!AnimBP)
	{
		OutError = TEXT("Invalid Animation Blueprint");
		Result->SetBoolField(TEXT("success"), false);
		Result->SetBoolField(TEXT("is_valid"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("blueprint_path"), AnimBP->GetPathName());
	Result->SetStringField(TEXT("blueprint_name"), AnimBP->GetName());

	// Compile the blueprint to get fresh status
	FKismetEditorUtilities::CompileBlueprint(AnimBP);

	// Build result based on status
	bool bIsValid = (AnimBP->Status != BS_Error);
	Result->SetBoolField(TEXT("is_valid"), bIsValid);

	// Status string
	FString StatusStr;
	switch (AnimBP->Status)
	{
	case BS_Unknown: StatusStr = TEXT("Unknown"); break;
	case BS_Dirty: StatusStr = TEXT("Dirty"); break;
	case BS_Error: StatusStr = TEXT("Error"); break;
	case BS_UpToDate: StatusStr = TEXT("UpToDate"); break;
	case BS_UpToDateWithWarnings: StatusStr = TEXT("UpToDateWithWarnings"); break;
	default: StatusStr = TEXT("Unknown"); break;
	}
	Result->SetStringField(TEXT("status"), StatusStr);

	// Collect info about state machines and their states
	TArray<TSharedPtr<FJsonValue>> StateMachinesInfo;
	TArray<UAnimGraphNode_StateMachine*> StateMachines = GetAllStateMachines(AnimBP);
	int32 TotalStates = 0;
	int32 TotalTransitions = 0;

	for (UAnimGraphNode_StateMachine* SM : StateMachines)
	{
		if (!SM) continue;

		TSharedPtr<FJsonObject> SMObj = MakeShared<FJsonObject>();
		FString SMName = SM->GetStateMachineName();
		SMObj->SetStringField(TEXT("name"), SMName);

		// Get states
		FString Error;
		TArray<UAnimStateNode*> States = FAnimStateMachineEditor::GetAllStates(AnimBP, SMName, Error);
		SMObj->SetNumberField(TEXT("state_count"), States.Num());
		TotalStates += States.Num();

		// Get transitions
		TArray<UAnimStateTransitionNode*> Transitions = FAnimStateMachineEditor::GetAllTransitions(AnimBP, SMName, Error);
		SMObj->SetNumberField(TEXT("transition_count"), Transitions.Num());
		TotalTransitions += Transitions.Num();

		StateMachinesInfo.Add(MakeShared<FJsonValueObject>(SMObj));
	}
	Result->SetArrayField(TEXT("state_machines"), StateMachinesInfo);
	Result->SetNumberField(TEXT("total_state_machines"), StateMachines.Num());
	Result->SetNumberField(TEXT("total_states"), TotalStates);
	Result->SetNumberField(TEXT("total_transitions"), TotalTransitions);

	// Check for common issues
	TArray<TSharedPtr<FJsonValue>> IssuesArray;
	int32 ErrorCount = 0;
	int32 WarningCount = 0;

	// Check if skeleton is assigned
	if (!AnimBP->TargetSkeleton)
	{
		TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
		Issue->SetStringField(TEXT("type"), TEXT("MissingSkeleton"));
		Issue->SetStringField(TEXT("message"), TEXT("Animation Blueprint has no target skeleton assigned"));
		Issue->SetStringField(TEXT("severity"), TEXT("Error"));
		IssuesArray.Add(MakeShared<FJsonValueObject>(Issue));
		ErrorCount++;
	}

	// Check for empty state machines
	for (UAnimGraphNode_StateMachine* SM : StateMachines)
	{
		if (!SM) continue;
		FString Error;
		FString SMName = SM->GetStateMachineName();
		TArray<UAnimStateNode*> States = FAnimStateMachineEditor::GetAllStates(AnimBP, SMName, Error);
		if (States.Num() == 0)
		{
			TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
			Issue->SetStringField(TEXT("type"), TEXT("EmptyStateMachine"));
			Issue->SetStringField(TEXT("message"), FString::Printf(TEXT("State machine '%s' has no states"),
				*SMName));
			Issue->SetStringField(TEXT("severity"), TEXT("Warning"));
			IssuesArray.Add(MakeShared<FJsonValueObject>(Issue));
			WarningCount++;
		}
	}

	// Update is_valid based on errors found
	if (ErrorCount > 0)
	{
		bIsValid = false;
		Result->SetBoolField(TEXT("is_valid"), false);
	}

	Result->SetArrayField(TEXT("issues"), IssuesArray);
	Result->SetNumberField(TEXT("error_count"), ErrorCount);
	Result->SetNumberField(TEXT("warning_count"), WarningCount);

	return Result;
}

// ===== Bulk Transition Condition Setup =====

bool FAnimationBlueprintUtils::MatchesWildcard(const FString& StateName, const FString& Pattern)
{
	// "*" matches anything
	if (Pattern == TEXT("*"))
	{
		return true;
	}

	// "Attack_*" matches "Attack_1H_1", "Attack_2H_2", etc.
	if (Pattern.EndsWith(TEXT("*")))
	{
		FString Prefix = Pattern.Left(Pattern.Len() - 1);
		return StateName.StartsWith(Prefix);
	}

	// "*_Idle" matches "Sword_Idle", "Axe_Idle", etc.
	if (Pattern.StartsWith(TEXT("*")))
	{
		FString Suffix = Pattern.Right(Pattern.Len() - 1);
		return StateName.EndsWith(Suffix);
	}

	// "*Combat*" matches "InCombatIdle", "CombatRun", etc.
	if (Pattern.StartsWith(TEXT("*")) && Pattern.EndsWith(TEXT("*")))
	{
		FString Middle = Pattern.Mid(1, Pattern.Len() - 2);
		return StateName.Contains(Middle);
	}

	// Exact match
	return StateName.Equals(Pattern, ESearchCase::IgnoreCase);
}

bool FAnimationBlueprintUtils::MatchesRegex(const FString& StateName, const FString& Pattern)
{
	// Check if pattern looks like regex (starts with ^ or contains special chars)
	bool bIsRegex = Pattern.StartsWith(TEXT("^")) || Pattern.EndsWith(TEXT("$")) ||
		Pattern.Contains(TEXT("\\d")) || Pattern.Contains(TEXT("\\w")) ||
		Pattern.Contains(TEXT("[")) || Pattern.Contains(TEXT("+")) ||
		Pattern.Contains(TEXT("?")) || (Pattern.Contains(TEXT(".")) && Pattern.Contains(TEXT("*")));

	if (!bIsRegex)
	{
		return false;
	}

	FRegexPattern RegexPattern(Pattern);
	FRegexMatcher Matcher(RegexPattern, StateName);
	return Matcher.FindNext();
}

bool FAnimationBlueprintUtils::MatchesPattern(const FString& StateName, const TSharedPtr<FJsonValue>& Pattern)
{
	if (!Pattern.IsValid())
	{
		return false;
	}

	// String pattern (exact, wildcard, or regex)
	if (Pattern->Type == EJson::String)
	{
		FString PatternStr = Pattern->AsString();

		// Try regex first (if it looks like regex)
		if (MatchesRegex(StateName, PatternStr))
		{
			return true;
		}

		// Try wildcard matching
		return MatchesWildcard(StateName, PatternStr);
	}

	// Array pattern (list of states)
	if (Pattern->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& PatternList = Pattern->AsArray();
		for (const TSharedPtr<FJsonValue>& Item : PatternList)
		{
			if (Item.IsValid() && Item->Type == EJson::String)
			{
				if (StateName.Equals(Item->AsString(), ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}
		return false;
	}

	return false;
}

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::SetupTransitionConditions(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const TArray<TSharedPtr<FJsonValue>>& Rules,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	int32 SuccessCount = 0;
	int32 FailureCount = 0;
	int32 MatchedTransitions = 0;

	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	// Get all transitions in the state machine
	TArray<UAnimStateTransitionNode*> AllTransitions = FAnimStateMachineEditor::GetAllTransitions(
		AnimBP, StateMachineName, OutError);

	if (AllTransitions.Num() == 0)
	{
		OutError = FString::Printf(TEXT("No transitions found in state machine '%s'"), *StateMachineName);
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	// Process each rule
	for (int32 RuleIndex = 0; RuleIndex < Rules.Num(); RuleIndex++)
	{
		const TSharedPtr<FJsonValue>& RuleValue = Rules[RuleIndex];
		const TSharedPtr<FJsonObject>* RuleObjPtr;

		if (!RuleValue->TryGetObject(RuleObjPtr) || !RuleObjPtr->IsValid())
		{
			TSharedPtr<FJsonObject> RuleResult = MakeShared<FJsonObject>();
			RuleResult->SetNumberField(TEXT("rule_index"), RuleIndex);
			RuleResult->SetBoolField(TEXT("success"), false);
			RuleResult->SetStringField(TEXT("error"), TEXT("Invalid rule format"));
			ResultsArray.Add(MakeShared<FJsonValueObject>(RuleResult));
			FailureCount++;
			continue;
		}

		const TSharedPtr<FJsonObject>& RuleObj = *RuleObjPtr;

		// Get match patterns
		const TSharedPtr<FJsonObject>* MatchObjPtr;
		if (!RuleObj->TryGetObjectField(TEXT("match"), MatchObjPtr) || !MatchObjPtr->IsValid())
		{
			TSharedPtr<FJsonObject> RuleResult = MakeShared<FJsonObject>();
			RuleResult->SetNumberField(TEXT("rule_index"), RuleIndex);
			RuleResult->SetBoolField(TEXT("success"), false);
			RuleResult->SetStringField(TEXT("error"), TEXT("Rule missing 'match' field"));
			ResultsArray.Add(MakeShared<FJsonValueObject>(RuleResult));
			FailureCount++;
			continue;
		}

		const TSharedPtr<FJsonObject>& MatchObj = *MatchObjPtr;
		TSharedPtr<FJsonValue> FromPattern = MatchObj->TryGetField(TEXT("from"));
		TSharedPtr<FJsonValue> ToPattern = MatchObj->TryGetField(TEXT("to"));

		if (!FromPattern.IsValid() || !ToPattern.IsValid())
		{
			TSharedPtr<FJsonObject> RuleResult = MakeShared<FJsonObject>();
			RuleResult->SetNumberField(TEXT("rule_index"), RuleIndex);
			RuleResult->SetBoolField(TEXT("success"), false);
			RuleResult->SetStringField(TEXT("error"), TEXT("Rule match missing 'from' or 'to' patterns"));
			ResultsArray.Add(MakeShared<FJsonValueObject>(RuleResult));
			FailureCount++;
			continue;
		}

		// Get conditions
		const TArray<TSharedPtr<FJsonValue>>* ConditionsPtr;
		if (!RuleObj->TryGetArrayField(TEXT("conditions"), ConditionsPtr) || !ConditionsPtr)
		{
			TSharedPtr<FJsonObject> RuleResult = MakeShared<FJsonObject>();
			RuleResult->SetNumberField(TEXT("rule_index"), RuleIndex);
			RuleResult->SetBoolField(TEXT("success"), false);
			RuleResult->SetStringField(TEXT("error"), TEXT("Rule missing 'conditions' array"));
			ResultsArray.Add(MakeShared<FJsonValueObject>(RuleResult));
			FailureCount++;
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>& Conditions = *ConditionsPtr;
		FString Logic = RuleObj->GetStringField(TEXT("logic"));
		if (Logic.IsEmpty()) Logic = TEXT("AND");

		// Track matched transitions for this rule
		TArray<TSharedPtr<FJsonValue>> RuleMatchedTransitions;

		// Find matching transitions
		for (UAnimStateTransitionNode* Transition : AllTransitions)
		{
			if (!Transition) continue;

			UAnimStateNodeBase* PrevState = Transition->GetPreviousState();
			UAnimStateNodeBase* NextState = Transition->GetNextState();

			if (!PrevState || !NextState) continue;

			FString FromStateName = PrevState->GetStateName();
			FString ToStateName = NextState->GetStateName();

			// Check if this transition matches the pattern
			bool bFromMatches = MatchesPattern(FromStateName, FromPattern);
			bool bToMatches = MatchesPattern(ToStateName, ToPattern);

			if (!bFromMatches || !bToMatches) continue;

			MatchedTransitions++;

			// Apply conditions to this transition
			TSharedPtr<FJsonObject> TransitionResult = MakeShared<FJsonObject>();
			TransitionResult->SetStringField(TEXT("from_state"), FromStateName);
			TransitionResult->SetStringField(TEXT("to_state"), ToStateName);

			FString ConditionError;
			bool bConditionSuccess = true;
			TArray<TSharedPtr<FJsonValue>> AppliedConditions;

			// Get transition graph
			UEdGraph* TransitionGraph = GetTransitionGraph(AnimBP, StateMachineName, FromStateName, ToStateName, ConditionError);
			if (!TransitionGraph)
			{
				TransitionResult->SetBoolField(TEXT("success"), false);
				TransitionResult->SetStringField(TEXT("error"), ConditionError);
				RuleMatchedTransitions.Add(MakeShared<FJsonValueObject>(TransitionResult));
				FailureCount++;
				continue;
			}

			// Track node IDs for connecting with logic operators
			TArray<FString> ConditionNodeIds;
			int32 PosX = 100;
			int32 PosY = 100;

			// Apply each condition
			for (const TSharedPtr<FJsonValue>& CondValue : Conditions)
			{
				const TSharedPtr<FJsonObject>* CondObjPtr;
				if (!CondValue->TryGetObject(CondObjPtr) || !CondObjPtr->IsValid()) continue;

				const TSharedPtr<FJsonObject>& CondObj = *CondObjPtr;
				TSharedPtr<FJsonObject> CondResult = MakeShared<FJsonObject>();

				FString CondType = CondObj->GetStringField(TEXT("type"));
				FString Variable = CondObj->GetStringField(TEXT("variable"));
				FString Comparison = CondObj->GetStringField(TEXT("comparison"));
				FString Value = CondObj->GetStringField(TEXT("value"));

				// Handle TimeRemaining condition
				if (CondType.Equals(TEXT("TimeRemaining"), ESearchCase::IgnoreCase))
				{
					FString NodeError;
					FString TimeNodeId;

					// Create TimeRemaining node
					UEdGraphNode* TimeNode = AddConditionNode(
						AnimBP, StateMachineName, FromStateName, ToStateName,
						TEXT("TimeRemaining"), nullptr, FVector2D(PosX, PosY),
						TimeNodeId, NodeError);

					if (!TimeNode)
					{
						CondResult->SetBoolField(TEXT("success"), false);
						CondResult->SetStringField(TEXT("error"), NodeError);
						bConditionSuccess = false;
					}
					else
					{
						// Create comparison node
						FString CompNodeId;
						UEdGraphNode* CompNode = AddConditionNode(
							AnimBP, StateMachineName, FromStateName, ToStateName,
							Comparison, nullptr, FVector2D(PosX + 200, PosY),
							CompNodeId, NodeError);

						if (CompNode)
						{
							// Connect TimeRemaining to comparison A
							ConnectConditionNodes(
								AnimBP, StateMachineName, FromStateName, ToStateName,
								TimeNodeId, TEXT("ReturnValue"),
								CompNodeId, TEXT("A"),
								NodeError);

							// Set comparison value on B
							SetPinDefaultValue(AnimBP, StateMachineName, FromStateName, ToStateName,
								CompNodeId, TEXT("B"), Value, NodeError);

							ConditionNodeIds.Add(CompNodeId);
							CondResult->SetBoolField(TEXT("success"), true);
							CondResult->SetStringField(TEXT("time_node_id"), TimeNodeId);
							CondResult->SetStringField(TEXT("comparison_node_id"), CompNodeId);
						}
						else
						{
							CondResult->SetBoolField(TEXT("success"), false);
							CondResult->SetStringField(TEXT("error"), NodeError);
							bConditionSuccess = false;
						}
					}
					CondResult->SetStringField(TEXT("type"), TEXT("TimeRemaining"));
				}
				// Handle variable comparison condition
				else if (!Variable.IsEmpty())
				{
					FString ChainError;
					TSharedPtr<FJsonObject> ChainResult = FAnimTransitionConditionFactory::CreateComparisonChain(
						AnimBP, TransitionGraph, Variable, Comparison, Value,
						FVector2D(PosX, PosY), ChainError);

					if (ChainResult.IsValid() && ChainResult->GetBoolField(TEXT("success")))
					{
						FString CompNodeId = ChainResult->GetStringField(TEXT("comparison_node_id"));
						ConditionNodeIds.Add(CompNodeId);
						CondResult->SetBoolField(TEXT("success"), true);
						CondResult->SetStringField(TEXT("variable"), Variable);
						CondResult->SetStringField(TEXT("comparison"), Comparison);
						CondResult->SetStringField(TEXT("value"), Value);
						CondResult->SetStringField(TEXT("comparison_node_id"), CompNodeId);
					}
					else
					{
						CondResult->SetBoolField(TEXT("success"), false);
						CondResult->SetStringField(TEXT("error"), ChainError);
						bConditionSuccess = false;
					}
				}
				else
				{
					CondResult->SetBoolField(TEXT("success"), false);
					CondResult->SetStringField(TEXT("error"), TEXT("Condition must have 'type' or 'variable'"));
					bConditionSuccess = false;
				}

				AppliedConditions.Add(MakeShared<FJsonValueObject>(CondResult));
				PosY += 150;
			}

			// If multiple conditions and using OR logic, we need to connect with OR nodes
			// (AND logic is handled automatically by CreateComparisonChain)
			if (ConditionNodeIds.Num() > 1 && Logic.Equals(TEXT("OR"), ESearchCase::IgnoreCase))
			{
				// Create OR chain
				FString OrError;
				FString PreviousNodeId = ConditionNodeIds[0];

				for (int32 i = 1; i < ConditionNodeIds.Num(); i++)
				{
					FString OrNodeId;
					UEdGraphNode* OrNode = AddConditionNode(
						AnimBP, StateMachineName, FromStateName, ToStateName,
						TEXT("Or"), nullptr, FVector2D(PosX + 400, 100 + (i * 100)),
						OrNodeId, OrError);

					if (OrNode)
					{
						// Connect previous result to OR input A
						ConnectConditionNodes(
							AnimBP, StateMachineName, FromStateName, ToStateName,
							PreviousNodeId, TEXT("ReturnValue"),
							OrNodeId, TEXT("A"),
							OrError);

						// Connect current condition to OR input B
						ConnectConditionNodes(
							AnimBP, StateMachineName, FromStateName, ToStateName,
							ConditionNodeIds[i], TEXT("ReturnValue"),
							OrNodeId, TEXT("B"),
							OrError);

						PreviousNodeId = OrNodeId;
					}
				}

				// Connect final OR output to result
				if (!PreviousNodeId.IsEmpty())
				{
					ConnectToTransitionResult(
						AnimBP, StateMachineName, FromStateName, ToStateName,
						PreviousNodeId, TEXT("ReturnValue"), OrError);
				}
			}

			TransitionResult->SetBoolField(TEXT("success"), bConditionSuccess);
			TransitionResult->SetArrayField(TEXT("conditions"), AppliedConditions);
			RuleMatchedTransitions.Add(MakeShared<FJsonValueObject>(TransitionResult));

			if (bConditionSuccess)
			{
				SuccessCount++;
			}
			else
			{
				FailureCount++;
			}
		}

		// Add rule result
		TSharedPtr<FJsonObject> RuleResult = MakeShared<FJsonObject>();
		RuleResult->SetNumberField(TEXT("rule_index"), RuleIndex);
		RuleResult->SetNumberField(TEXT("matched_count"), RuleMatchedTransitions.Num());
		RuleResult->SetArrayField(TEXT("transitions"), RuleMatchedTransitions);
		ResultsArray.Add(MakeShared<FJsonValueObject>(RuleResult));
	}

	// Compile once after all operations
	FString CompileError;
	bool bCompiled = CompileAnimBlueprint(AnimBP, CompileError);

	Result->SetBoolField(TEXT("success"), FailureCount == 0 && bCompiled);
	Result->SetNumberField(TEXT("total_transitions_in_machine"), AllTransitions.Num());
	Result->SetNumberField(TEXT("matched_transitions"), MatchedTransitions);
	Result->SetNumberField(TEXT("success_count"), SuccessCount);
	Result->SetNumberField(TEXT("failure_count"), FailureCount);
	Result->SetNumberField(TEXT("rules_processed"), Rules.Num());
	Result->SetBoolField(TEXT("compiled"), bCompiled);
	if (!bCompiled)
	{
		Result->SetStringField(TEXT("compile_error"), CompileError);
	}
	Result->SetArrayField(TEXT("results"), ResultsArray);

	return Result;
}

// ===== ASCII Diagram Generation =====

FString FAnimationBlueprintUtils::AbbreviateTransitionCondition(UAnimStateTransitionNode* TransitionNode)
{
	if (!TransitionNode)
	{
		return TEXT("(none)");
	}

	UEdGraph* TransitionGraph = TransitionNode->BoundGraph;
	if (!TransitionGraph)
	{
		return TEXT("(none)");
	}

	// Find what's connected to the result node
	TArray<FString> ConditionParts;

	for (UEdGraphNode* Node : TransitionGraph->Nodes)
	{
		if (!Node) continue;

		// Check for UK2Node_CallFunction comparison nodes
		if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			FName FunctionName = CallNode->FunctionReference.GetMemberName();
			FString FuncStr = FunctionName.ToString();

			// Detect comparison functions
			FString CompOp;
			bool bIsComparison = false;

			if (FuncStr.Contains(TEXT("Greater_")) && !FuncStr.Contains(TEXT("Equal")))
			{
				CompOp = TEXT(">");
				bIsComparison = true;
			}
			else if (FuncStr.Contains(TEXT("Less_")) && !FuncStr.Contains(TEXT("Equal")))
			{
				CompOp = TEXT("<");
				bIsComparison = true;
			}
			else if (FuncStr.Contains(TEXT("GreaterEqual")))
			{
				CompOp = TEXT(">=");
				bIsComparison = true;
			}
			else if (FuncStr.Contains(TEXT("LessEqual")))
			{
				CompOp = TEXT("<=");
				bIsComparison = true;
			}
			else if (FuncStr.Contains(TEXT("EqualEqual")))
			{
				CompOp = TEXT("==");
				bIsComparison = true;
			}
			else if (FuncStr.Contains(TEXT("NotEqual")))
			{
				CompOp = TEXT("!=");
				bIsComparison = true;
			}

			if (bIsComparison)
			{
				// Get what's connected to input A (usually variable)
				FString LeftSide = TEXT("?");
				FString RightSide = TEXT("?");

				for (UEdGraphPin* Pin : CallNode->Pins)
				{
					if (!Pin) continue;

					if (Pin->GetFName() == TEXT("A") && Pin->Direction == EGPD_Input)
					{
						if (Pin->LinkedTo.Num() > 0)
						{
							UEdGraphPin* SourcePin = Pin->LinkedTo[0];
							if (SourcePin && SourcePin->GetOwningNode())
							{
								// Check if source is a variable get node
								if (UK2Node_VariableGet* VarNode = Cast<UK2Node_VariableGet>(SourcePin->GetOwningNode()))
								{
									LeftSide = VarNode->GetVarName().ToString();
								}
								else if (UK2Node_TransitionRuleGetter* GetterNode = Cast<UK2Node_TransitionRuleGetter>(SourcePin->GetOwningNode()))
								{
									// It's a TimeRemaining node
									LeftSide = TEXT("TimeRem");
								}
							}
						}
						else if (!Pin->DefaultValue.IsEmpty())
						{
							LeftSide = Pin->DefaultValue;
						}
					}
					else if (Pin->GetFName() == TEXT("B") && Pin->Direction == EGPD_Input)
					{
						if (!Pin->DefaultValue.IsEmpty())
						{
							RightSide = Pin->DefaultValue;
						}
						else if (Pin->LinkedTo.Num() > 0)
						{
							RightSide = TEXT("(link)");
						}
					}
				}

				ConditionParts.Add(FString::Printf(TEXT("%s%s%s"), *LeftSide, *CompOp, *RightSide));
			}
		}
		// Check for TimeRemaining getter that's directly connected
		else if (UK2Node_TransitionRuleGetter* GetterNode = Cast<UK2Node_TransitionRuleGetter>(Node))
		{
			// Check if output is directly connected to result (no comparison)
			for (UEdGraphPin* Pin : GetterNode->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
				{
					// If connected directly to result node input
					UEdGraphNode* TargetNode = Pin->LinkedTo[0]->GetOwningNode();
					if (TargetNode && TargetNode->IsA<UAnimGraphNode_TransitionResult>())
					{
						ConditionParts.Add(TEXT("TimeRem"));
					}
				}
			}
		}
	}

	if (ConditionParts.Num() == 0)
	{
		return TEXT("(auto)");
	}

	return FString::Join(ConditionParts, TEXT(" & "));
}

void FAnimationBlueprintUtils::CalculateStateLayout(
	TArray<FDiagramState>& States,
	const TArray<FDiagramTransition>& Transitions)
{
	if (States.Num() == 0) return;

	// Simple layout: find entry state, put it on left
	// Then place connected states to the right, spreading vertically

	// Find entry state and put at (0, 0)
	int32 EntryIndex = -1;
	for (int32 i = 0; i < States.Num(); i++)
	{
		if (States[i].bIsEntry)
		{
			EntryIndex = i;
			States[i].GridX = 0;
			States[i].GridY = 0;
			break;
		}
	}

	// If no entry state found, use first state
	if (EntryIndex == -1 && States.Num() > 0)
	{
		EntryIndex = 0;
		States[0].GridX = 0;
		States[0].GridY = 0;
	}

	// Build adjacency for BFS layout
	TMap<FString, TArray<FString>> Outgoing;
	for (const FDiagramTransition& Trans : Transitions)
	{
		Outgoing.FindOrAdd(Trans.FromState).Add(Trans.ToState);
	}

	// BFS to assign grid positions
	TSet<FString> Visited;
	TArray<FString> Queue;

	if (EntryIndex >= 0)
	{
		Queue.Add(States[EntryIndex].Name);
		Visited.Add(States[EntryIndex].Name);
	}

	int32 CurrentX = 0;
	while (Queue.Num() > 0)
	{
		// Get all states at current X level
		TArray<FString> CurrentLevel = Queue;
		Queue.Empty();

		int32 YOffset = 0;
		for (const FString& StateName : CurrentLevel)
		{
			// Find and set Y position for states at this X level
			for (FDiagramState& State : States)
			{
				if (State.Name == StateName && State.GridX == CurrentX)
				{
					State.GridY = YOffset;
					YOffset++;
				}
			}

			// Add connected states to next level
			if (TArray<FString>* Connected = Outgoing.Find(StateName))
			{
				for (const FString& NextState : *Connected)
				{
					if (!Visited.Contains(NextState))
					{
						Visited.Add(NextState);
						Queue.Add(NextState);

						// Assign X position
						for (FDiagramState& State : States)
						{
							if (State.Name == NextState)
							{
								State.GridX = CurrentX + 1;
								break;
							}
						}
					}
				}
			}
		}
		CurrentX++;
	}

	// Handle disconnected states
	int32 DisconnectedY = 0;
	for (FDiagramState& State : States)
	{
		if (!Visited.Contains(State.Name))
		{
			State.GridX = CurrentX;
			State.GridY = DisconnectedY++;
		}
	}
}

FString FAnimationBlueprintUtils::GenerateASCIIDiagram(
	const TArray<FDiagramState>& States,
	const TArray<FDiagramTransition>& Transitions)
{
	if (States.Num() == 0)
	{
		return TEXT("(empty state machine)");
	}

	// Find grid dimensions
	int32 MaxX = 0, MaxY = 0;
	for (const FDiagramState& State : States)
	{
		MaxX = FMath::Max(MaxX, State.GridX);
		MaxY = FMath::Max(MaxY, State.GridY);
	}

	// Build a grid-based diagram
	// Each cell is roughly: "[ StateName ]" with transitions between
	const int32 CellWidth = 20;  // Width for each state cell
	const int32 CellHeight = 3;  // Height for each state cell row

	// Create output lines
	TArray<FString> Lines;
	FString TitleLine = TEXT("State Machine Diagram:");
	Lines.Add(TitleLine);
	Lines.Add(TEXT(""));

	// Build state map by grid position
	TMap<TPair<int32, int32>, const FDiagramState*> StateGrid;
	for (const FDiagramState& State : States)
	{
		StateGrid.Add(TPair<int32, int32>(State.GridX, State.GridY), &State);
	}

	// Build transition map
	TMap<TPair<FString, FString>, const FDiagramTransition*> TransitionMap;
	for (const FDiagramTransition& Trans : Transitions)
	{
		TransitionMap.Add(TPair<FString, FString>(Trans.FromState, Trans.ToState), &Trans);
	}

	// Generate diagram row by row
	for (int32 y = 0; y <= MaxY; y++)
	{
		// State row
		FString StateRow;
		FString ArrowRow;
		FString ConditionRow;

		for (int32 x = 0; x <= MaxX; x++)
		{
			if (const FDiagramState* const* StatePtr = StateGrid.Find(TPair<int32, int32>(x, y)))
			{
				const FDiagramState& State = **StatePtr;

				// Format state name with brackets
				FString StateName = State.Name;
				if (StateName.Len() > 12)
				{
					StateName = StateName.Left(11) + TEXT(".");
				}

				FString StateBox;
				if (State.bIsEntry)
				{
					StateBox = FString::Printf(TEXT("->[ %-12s ]"), *StateName);
				}
				else
				{
					StateBox = FString::Printf(TEXT("  [ %-12s ]"), *StateName);
				}
				StateRow += StateBox;

				// Find transition to next column
				if (x < MaxX)
				{
					// Look for any transition from this state to states in next column
					bool bFoundTransition = false;
					for (int32 nextY = 0; nextY <= MaxY; nextY++)
					{
						if (const FDiagramState* const* NextStatePtr = StateGrid.Find(TPair<int32, int32>(x + 1, nextY)))
						{
							const FDiagramState& NextState = **NextStatePtr;
							if (const FDiagramTransition* const* TransPtr = TransitionMap.Find(
								TPair<FString, FString>(State.Name, NextState.Name)))
							{
								ArrowRow += TEXT(" ----> ");
								// Add abbreviated condition
								FString Cond = (*TransPtr)->ConditionAbbrev;
								if (Cond.Len() > 15)
								{
									Cond = Cond.Left(14) + TEXT(".");
								}
								ConditionRow += FString::Printf(TEXT(" %-15s"), *Cond);
								bFoundTransition = true;
								break;
							}
						}
					}

					if (!bFoundTransition)
					{
						ArrowRow += TEXT("       ");
						ConditionRow += TEXT("               ");
					}
				}
			}
			else
			{
				// Empty cell
				StateRow += FString::Printf(TEXT("%*s"), CellWidth, TEXT(""));
				if (x < MaxX)
				{
					ArrowRow += TEXT("       ");
					ConditionRow += TEXT("               ");
				}
			}
		}

		Lines.Add(StateRow);
		if (!ArrowRow.IsEmpty())
		{
			Lines.Add(ArrowRow);
		}
		if (!ConditionRow.TrimStartAndEnd().IsEmpty())
		{
			Lines.Add(ConditionRow);
		}
		Lines.Add(TEXT(""));
	}

	// Add legend
	Lines.Add(TEXT("Legend:"));
	Lines.Add(TEXT("  -> = Entry state"));
	Lines.Add(TEXT("  [...] = State name"));
	Lines.Add(TEXT("  ----> = Transition (condition below)"));

	// Add transition list
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("All Transitions:"));
	for (const FDiagramTransition& Trans : Transitions)
	{
		Lines.Add(FString::Printf(TEXT("  %s -> %s: %s (%.2fs)"),
			*Trans.FromState, *Trans.ToState, *Trans.ConditionAbbrev, Trans.Duration));
	}

	return FString::Join(Lines, TEXT("\n"));
}

TSharedPtr<FJsonObject> FAnimationBlueprintUtils::GetStateMachineDiagram(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	FString& OutError)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!ValidateAnimBlueprintForOperation(AnimBP, OutError))
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	// Find state machine
	UAnimGraphNode_StateMachine* StateMachine = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!StateMachine)
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), OutError);
		return Result;
	}

	// Get all states
	TArray<UAnimStateNode*> States = GetAllStates(AnimBP, StateMachineName, OutError);
	TArray<UAnimStateTransitionNode*> Transitions = GetAllTransitions(AnimBP, StateMachineName, OutError);

	// Build diagram data structures
	TArray<FDiagramState> DiagramStates;
	TArray<FDiagramTransition> DiagramTransitions;

	// Find entry state
	UAnimationStateMachineGraph* Graph = nullptr;
	if (StateMachine->EditorStateMachineGraph)
	{
		Graph = StateMachine->EditorStateMachineGraph;
	}

	FString EntryStateName;
	if (Graph && Graph->EntryNode)
	{
		// Find what the entry node connects to
		for (UEdGraphPin* Pin : Graph->EntryNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
			{
				UEdGraphNode* ConnectedNode = Pin->LinkedTo[0]->GetOwningNode();
				if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(ConnectedNode))
				{
					EntryStateName = StateNode->GetStateName();
					break;
				}
			}
		}
	}

	// Populate states
	for (UAnimStateNode* State : States)
	{
		if (!State) continue;

		FDiagramState DS;
		DS.Name = State->GetStateName();
		DS.bIsEntry = (DS.Name == EntryStateName);

		// Get animation name if available
		UEdGraph* StateGraph = State->GetBoundGraph();
		if (StateGraph)
		{
			for (UEdGraphNode* Node : StateGraph->Nodes)
			{
				if (UAnimGraphNode_SequencePlayer* SeqNode = Cast<UAnimGraphNode_SequencePlayer>(Node))
				{
					// Get animation sequence name
					if (SeqNode->GetAnimationAsset())
					{
						DS.AnimationName = SeqNode->GetAnimationAsset()->GetName();
					}
					break;
				}
				else if (UAnimGraphNode_BlendSpacePlayer* BSNode = Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
				{
					if (BSNode->GetAnimationAsset())
					{
						DS.AnimationName = BSNode->GetAnimationAsset()->GetName();
					}
					break;
				}
			}
		}

		DiagramStates.Add(DS);
	}

	// Populate transitions
	for (UAnimStateTransitionNode* Trans : Transitions)
	{
		if (!Trans) continue;

		UAnimStateNodeBase* PrevState = Trans->GetPreviousState();
		UAnimStateNodeBase* NextState = Trans->GetNextState();

		if (!PrevState || !NextState) continue;

		FDiagramTransition DT;
		DT.FromState = PrevState->GetStateName();
		DT.ToState = NextState->GetStateName();
		DT.Duration = Trans->CrossfadeDuration;
		DT.ConditionAbbrev = AbbreviateTransitionCondition(Trans);

		DiagramTransitions.Add(DT);
	}

	// Calculate layout
	CalculateStateLayout(DiagramStates, DiagramTransitions);

	// Generate ASCII diagram
	FString ASCIIDiagram = GenerateASCIIDiagram(DiagramStates, DiagramTransitions);

	// Build enhanced JSON info
	TSharedPtr<FJsonObject> EnhancedInfo = MakeShared<FJsonObject>();

	// States with positions
	TArray<TSharedPtr<FJsonValue>> StatesArray;
	for (const FDiagramState& DS : DiagramStates)
	{
		TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
		StateObj->SetStringField(TEXT("name"), DS.Name);
		StateObj->SetNumberField(TEXT("grid_x"), DS.GridX);
		StateObj->SetNumberField(TEXT("grid_y"), DS.GridY);
		StateObj->SetBoolField(TEXT("is_entry"), DS.bIsEntry);
		if (!DS.AnimationName.IsEmpty())
		{
			StateObj->SetStringField(TEXT("animation"), DS.AnimationName);
		}
		StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
	}
	EnhancedInfo->SetArrayField(TEXT("states"), StatesArray);

	// Transitions with conditions
	TArray<TSharedPtr<FJsonValue>> TransitionsArray;
	for (const FDiagramTransition& DT : DiagramTransitions)
	{
		TSharedPtr<FJsonObject> TransObj = MakeShared<FJsonObject>();
		TransObj->SetStringField(TEXT("from"), DT.FromState);
		TransObj->SetStringField(TEXT("to"), DT.ToState);
		TransObj->SetStringField(TEXT("condition_summary"), DT.ConditionAbbrev);
		TransObj->SetNumberField(TEXT("duration"), DT.Duration);
		TransitionsArray.Add(MakeShared<FJsonValueObject>(TransObj));
	}
	EnhancedInfo->SetArrayField(TEXT("transitions"), TransitionsArray);

	// Build result
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state_machine"), StateMachineName);
	Result->SetStringField(TEXT("ascii_diagram"), ASCIIDiagram);
	Result->SetObjectField(TEXT("enhanced_info"), EnhancedInfo);

	return Result;
}
