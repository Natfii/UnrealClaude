// Copyright Natali Caggiano. All Rights Reserved.

#include "AnimationBlueprintUtils.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace1D.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

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
