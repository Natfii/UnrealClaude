// Copyright Natali Caggiano. All Rights Reserved.
//
// Phase 8: Serialization helpers extracted from AnimationBlueprintUtils.cpp
// to keep the main utility file smaller. Function names and signatures are unchanged.
// Callers continue to use FAnimationBlueprintUtils::Serialize*.

#include "AnimationBlueprintUtils.h"
#include "AnimAssetManager.h"
#include "AnimStateMachineEditor.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

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

	USkeleton* Skeleton = FAnimAssetManager::GetTargetSkeleton(AnimBP);
	if (Skeleton)
	{
		Result->SetStringField(TEXT("skeleton"), Skeleton->GetName());
		Result->SetStringField(TEXT("skeleton_path"), Skeleton->GetPathName());
	}

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

	TArray<TSharedPtr<FJsonValue>> StatesArray;
	TArray<UAnimStateNode*> States = GetAllStates(AnimBP, StateMachineName, Error);
	for (UAnimStateNode* State : States)
	{
		StatesArray.Add(MakeShared<FJsonValueObject>(SerializeStateInfo(State)));
	}
	Result->SetArrayField(TEXT("states"), StatesArray);

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
