// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_AnimBlueprintModify.h"
#include "AnimationBlueprintUtils.h"
#include "AnimGraphEditor.h"
#include "Serialization/JsonSerializer.h"

FMCPToolResult FMCPTool_AnimBlueprintModify::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Extract required parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	// Validate blueprint path for security (block engine paths, path traversal, etc.)
	if (!ValidateBlueprintPathParam(BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString Operation;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	// Route to appropriate handler
	if (Operation == TEXT("get_info"))
	{
		return HandleGetInfo(BlueprintPath);
	}
	else if (Operation == TEXT("get_state_machine"))
	{
		return HandleGetStateMachine(BlueprintPath, Params);
	}
	else if (Operation == TEXT("create_state_machine"))
	{
		return HandleCreateStateMachine(BlueprintPath, Params);
	}
	else if (Operation == TEXT("add_state"))
	{
		return HandleAddState(BlueprintPath, Params);
	}
	else if (Operation == TEXT("remove_state"))
	{
		return HandleRemoveState(BlueprintPath, Params);
	}
	else if (Operation == TEXT("set_entry_state"))
	{
		return HandleSetEntryState(BlueprintPath, Params);
	}
	else if (Operation == TEXT("add_transition"))
	{
		return HandleAddTransition(BlueprintPath, Params);
	}
	else if (Operation == TEXT("remove_transition"))
	{
		return HandleRemoveTransition(BlueprintPath, Params);
	}
	else if (Operation == TEXT("set_transition_duration"))
	{
		return HandleSetTransitionDuration(BlueprintPath, Params);
	}
	else if (Operation == TEXT("set_transition_priority"))
	{
		return HandleSetTransitionPriority(BlueprintPath, Params);
	}
	else if (Operation == TEXT("add_condition_node"))
	{
		return HandleAddConditionNode(BlueprintPath, Params);
	}
	else if (Operation == TEXT("delete_condition_node"))
	{
		return HandleDeleteConditionNode(BlueprintPath, Params);
	}
	else if (Operation == TEXT("connect_condition_nodes"))
	{
		return HandleConnectConditionNodes(BlueprintPath, Params);
	}
	else if (Operation == TEXT("connect_to_result"))
	{
		return HandleConnectToResult(BlueprintPath, Params);
	}
	else if (Operation == TEXT("connect_state_machine_to_output"))
	{
		return HandleConnectStateMachineToOutput(BlueprintPath, Params);
	}
	else if (Operation == TEXT("set_state_animation"))
	{
		return HandleSetStateAnimation(BlueprintPath, Params);
	}
	else if (Operation == TEXT("find_animations"))
	{
		return HandleFindAnimations(BlueprintPath, Params);
	}
	else if (Operation == TEXT("batch"))
	{
		return HandleBatch(BlueprintPath, Params);
	}
	// NEW operations for enhanced pin/node introspection
	else if (Operation == TEXT("get_transition_nodes"))
	{
		return HandleGetTransitionNodes(BlueprintPath, Params);
	}
	else if (Operation == TEXT("inspect_node_pins"))
	{
		return HandleInspectNodePins(BlueprintPath, Params);
	}
	else if (Operation == TEXT("set_pin_default_value"))
	{
		return HandleSetPinDefaultValue(BlueprintPath, Params);
	}
	else if (Operation == TEXT("add_comparison_chain"))
	{
		return HandleAddComparisonChain(BlueprintPath, Params);
	}
	else if (Operation == TEXT("validate_blueprint"))
	{
		return HandleValidateBlueprint(BlueprintPath);
	}
	else if (Operation == TEXT("get_state_machine_diagram"))
	{
		return HandleGetStateMachineDiagram(BlueprintPath, Params);
	}
	// Bulk operation for setting up multiple transition conditions
	else if (Operation == TEXT("setup_transition_conditions"))
	{
		return HandleSetupTransitionConditions(BlueprintPath, Params);
	}

	return FMCPToolResult::Error(FString::Printf(TEXT("Unknown operation: %s"), *Operation));
}

FVector2D FMCPTool_AnimBlueprintModify::ExtractPosition(const TSharedRef<FJsonObject>& Params)
{
	FVector2D Position(0, 0);
	const TSharedPtr<FJsonObject>* PosObj;
	if (Params->TryGetObjectField(TEXT("position"), PosObj) && PosObj && (*PosObj).IsValid())
	{
		Position.X = (*PosObj)->GetNumberField(TEXT("x"));
		Position.Y = (*PosObj)->GetNumberField(TEXT("y"));
	}
	return Position;
}

TOptional<FMCPToolResult> FMCPTool_AnimBlueprintModify::LoadAnimBlueprintOrError(
	const FString& Path,
	UAnimBlueprint*& OutBP)
{
	FString Error;
	OutBP = FAnimationBlueprintUtils::LoadAnimBlueprint(Path, Error);
	if (!OutBP)
	{
		return FMCPToolResult::Error(Error);
	}
	return TOptional<FMCPToolResult>();
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleGetInfo(const FString& BlueprintPath)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	TSharedPtr<FJsonObject> Result = FAnimationBlueprintUtils::SerializeAnimBlueprintInfo(AnimBP);
	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleGetStateMachine(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	if (StateMachineName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine parameter required"));
	}

	TSharedPtr<FJsonObject> Result = FAnimationBlueprintUtils::SerializeStateMachineInfo(AnimBP, StateMachineName);
	if (!Result->GetBoolField(TEXT("success")))
	{
		return FMCPToolResult::Error(Result->GetStringField(TEXT("error")));
	}

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleCreateStateMachine(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString MachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	if (MachineName.IsEmpty())
	{
		MachineName = TEXT("Locomotion");
	}

	FVector2D Position = ExtractPosition(Params);
	FString NodeId, Error;

	UAnimGraphNode_StateMachine* SM = FAnimationBlueprintUtils::CreateStateMachine(
		AnimBP, MachineName, Position, NodeId, Error);

	if (!SM)
	{
		return FMCPToolResult::Error(Error);
	}

	// Compile
	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("node_id"), NodeId);
	Result->SetStringField(TEXT("name"), MachineName);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created state machine '%s'"), *MachineName));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleAddState(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString StateName = ExtractOptionalString(Params, TEXT("state_name"));

	if (StateMachineName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine parameter required"));
	}
	if (StateName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_name parameter required"));
	}

	FVector2D Position = ExtractPosition(Params);
	bool bIsEntry = ExtractOptionalBool(Params, TEXT("is_entry_state"), false);
	FString NodeId, Error;

	UAnimStateNode* State = FAnimationBlueprintUtils::AddState(
		AnimBP, StateMachineName, StateName, Position, bIsEntry, NodeId, Error);

	if (!State)
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("node_id"), NodeId);
	Result->SetStringField(TEXT("state_name"), StateName);
	Result->SetBoolField(TEXT("is_entry_state"), bIsEntry);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added state '%s' to '%s'"), *StateName, *StateMachineName));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleRemoveState(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString StateName = ExtractOptionalString(Params, TEXT("state_name"));

	if (StateMachineName.IsEmpty() || StateName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine and state_name parameters required"));
	}

	FString Error;
	if (!FAnimationBlueprintUtils::RemoveState(AnimBP, StateMachineName, StateName, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Removed state '%s' from '%s'"), *StateName, *StateMachineName));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleSetEntryState(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString StateName = ExtractOptionalString(Params, TEXT("state_name"));

	if (StateMachineName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine parameter required"));
	}
	if (StateName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_name parameter required"));
	}

	FString Error;
	if (!FAnimationBlueprintUtils::SetEntryState(AnimBP, StateMachineName, StateName, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state_machine"), StateMachineName);
	Result->SetStringField(TEXT("entry_state"), StateName);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set '%s' as entry state for '%s'"), *StateName, *StateMachineName));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleAddTransition(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, and to_state parameters required"));
	}

	FString NodeId, Error;
	UAnimStateTransitionNode* Transition = FAnimationBlueprintUtils::CreateTransition(
		AnimBP, StateMachineName, FromState, ToState, NodeId, Error);

	if (!Transition)
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("node_id"), NodeId);
	Result->SetStringField(TEXT("from_state"), FromState);
	Result->SetStringField(TEXT("to_state"), ToState);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created transition '%s' -> '%s'"), *FromState, *ToState));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleRemoveTransition(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, and to_state parameters required"));
	}

	FString Error;
	if (!FAnimationBlueprintUtils::RemoveTransition(AnimBP, StateMachineName, FromState, ToState, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Removed transition '%s' -> '%s'"), *FromState, *ToState));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleSetTransitionDuration(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));
	float Duration = ExtractOptionalNumber<float>(Params, TEXT("duration"), 0.2f);

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, and to_state parameters required"));
	}

	FString Error;
	if (!FAnimationBlueprintUtils::SetTransitionDuration(AnimBP, StateMachineName, FromState, ToState, Duration, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("duration"), Duration);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set transition duration to %.2fs"), Duration));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleSetTransitionPriority(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));
	int32 Priority = ExtractOptionalNumber<int32>(Params, TEXT("priority"), 1);

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, and to_state parameters required"));
	}

	FString Error;
	if (!FAnimationBlueprintUtils::SetTransitionPriority(AnimBP, StateMachineName, FromState, ToState, Priority, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("priority"), Priority);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set transition priority to %d"), Priority));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleAddConditionNode(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));
	FString NodeType = ExtractOptionalString(Params, TEXT("node_type"));

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty() || NodeType.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, to_state, and node_type parameters required"));
	}

	// Get node params
	TSharedPtr<FJsonObject> NodeParams = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* ParamsObj;
	if (Params->TryGetObjectField(TEXT("node_params"), ParamsObj))
	{
		NodeParams = *ParamsObj;
	}

	FVector2D Position = ExtractPosition(Params);
	FString NodeId, Error;

	UEdGraphNode* Node = FAnimationBlueprintUtils::AddConditionNode(
		AnimBP, StateMachineName, FromState, ToState,
		NodeType, NodeParams, Position, NodeId, Error);

	if (!Node)
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("node_id"), NodeId);
	Result->SetStringField(TEXT("node_type"), NodeType);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s condition node"), *NodeType));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleDeleteConditionNode(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));
	FString NodeId = ExtractOptionalString(Params, TEXT("node_id"));

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty() || NodeId.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, to_state, and node_id parameters required"));
	}

	FString Error;
	if (!FAnimationBlueprintUtils::DeleteConditionNode(
		AnimBP, StateMachineName, FromState, ToState, NodeId, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("deleted_node_id"), NodeId);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Deleted condition node %s"), *NodeId));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleConnectConditionNodes(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));
	FString SourceNodeId = ExtractOptionalString(Params, TEXT("source_node_id"));
	FString SourcePin = ExtractOptionalString(Params, TEXT("source_pin"));
	FString TargetNodeId = ExtractOptionalString(Params, TEXT("target_node_id"));
	FString TargetPin = ExtractOptionalString(Params, TEXT("target_pin"));

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty() ||
		SourceNodeId.IsEmpty() || TargetNodeId.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, to_state, source_node_id, and target_node_id required"));
	}

	FString Error;
	if (!FAnimationBlueprintUtils::ConnectConditionNodes(
		AnimBP, StateMachineName, FromState, ToState,
		SourceNodeId, SourcePin, TargetNodeId, TargetPin, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Connected %s -> %s"), *SourceNodeId, *TargetNodeId));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleConnectToResult(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));
	FString ConditionNodeId = ExtractOptionalString(Params, TEXT("source_node_id"));
	FString ConditionPin = ExtractOptionalString(Params, TEXT("source_pin"), TEXT("Result"));

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty() || ConditionNodeId.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, to_state, and source_node_id required"));
	}

	FString Error;
	if (!FAnimationBlueprintUtils::ConnectToTransitionResult(
		AnimBP, StateMachineName, FromState, ToState,
		ConditionNodeId, ConditionPin, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), TEXT("Connected condition to transition result"));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleConnectStateMachineToOutput(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	if (StateMachineName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine parameter required"));
	}

	FString Error;
	if (!FAnimGraphEditor::ConnectStateMachineToAnimGraphRoot(AnimBP, StateMachineName, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state_machine"), StateMachineName);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Connected State Machine '%s' to AnimGraph Output Pose"), *StateMachineName));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleSetStateAnimation(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString StateName = ExtractOptionalString(Params, TEXT("state_name"));
	FString AnimType = ExtractOptionalString(Params, TEXT("animation_type"), TEXT("sequence"));
	FString AnimPath = ExtractOptionalString(Params, TEXT("animation_path"));

	if (StateMachineName.IsEmpty() || StateName.IsEmpty() || AnimPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, state_name, and animation_path required"));
	}

	FString Error;
	bool bSuccess = false;

	if (AnimType == TEXT("sequence"))
	{
		bSuccess = FAnimationBlueprintUtils::SetStateAnimSequence(
			AnimBP, StateMachineName, StateName, AnimPath, Error);
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
		bSuccess = FAnimationBlueprintUtils::SetStateBlendSpace(
			AnimBP, StateMachineName, StateName, AnimPath, Bindings, Error);
	}
	else if (AnimType == TEXT("blendspace1d"))
	{
		FString Binding = ExtractOptionalString(Params, TEXT("parameter_bindings"));
		// Also try getting from object if it was passed that way
		if (Binding.IsEmpty())
		{
			const TSharedPtr<FJsonObject>* BindingsObj;
			if (Params->TryGetObjectField(TEXT("parameter_bindings"), BindingsObj))
			{
				// Get first value as the binding
				for (const auto& Pair : (*BindingsObj)->Values)
				{
					Binding = Pair.Value->AsString();
					break;
				}
			}
		}
		bSuccess = FAnimationBlueprintUtils::SetStateBlendSpace1D(
			AnimBP, StateMachineName, StateName, AnimPath, Binding, Error);
	}
	else if (AnimType == TEXT("montage"))
	{
		bSuccess = FAnimationBlueprintUtils::SetStateMontage(
			AnimBP, StateMachineName, StateName, AnimPath, Error);
	}
	else
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Unknown animation type: %s"), *AnimType));
	}

	if (!bSuccess)
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state_name"), StateName);
	Result->SetStringField(TEXT("animation_type"), AnimType);
	Result->SetStringField(TEXT("animation_path"), AnimPath);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %s animation for state '%s'"), *AnimType, *StateName));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleFindAnimations(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	FString Error;
	UAnimBlueprint* AnimBP = nullptr;

	// AnimBlueprint is optional for this operation - used for skeleton filtering
	if (!BlueprintPath.IsEmpty() && BlueprintPath != TEXT("*"))
	{
		AnimBP = FAnimationBlueprintUtils::LoadAnimBlueprint(BlueprintPath, Error);
	}

	FString SearchPattern = ExtractOptionalString(Params, TEXT("search_pattern"), TEXT("*"));
	FString AssetType = ExtractOptionalString(Params, TEXT("asset_type"), TEXT("All"));

	TArray<FString> Assets = FAnimationBlueprintUtils::FindAnimationAssets(
		SearchPattern, AssetType, AnimBP);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("count"), Assets.Num());
	Result->SetArrayField(TEXT("assets"), StringArrayToJsonArray(Assets));

	if (AnimBP)
	{
		Result->SetStringField(TEXT("skeleton_filter"), AnimBP->GetName());
	}

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleBatch(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	const TArray<TSharedPtr<FJsonValue>>* Operations;
	if (!Params->TryGetArrayField(TEXT("operations"), Operations))
	{
		return FMCPToolResult::Error(TEXT("operations array required for batch mode"));
	}

	FString Error;
	TSharedPtr<FJsonObject> Result = FAnimationBlueprintUtils::ExecuteBatchOperations(
		AnimBP, *Operations, Error);

	if (!Result->GetBoolField(TEXT("success")))
	{
		// Still return the partial results with the error
		return FMCPToolResult::Success(TEXT("Batch operation completed with errors"), Result);
	}

	return FMCPToolResult::Success(TEXT("Batch operation completed successfully"), Result);
}

// ===== NEW Handlers for Enhanced Operations =====

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleGetTransitionNodes(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));

	if (StateMachineName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine parameter required"));
	}

	FString Error;
	TSharedPtr<FJsonObject> Result = FAnimationBlueprintUtils::GetTransitionNodes(
		AnimBP, StateMachineName, FromState, ToState, Error);

	if (!Result->GetBoolField(TEXT("success")))
	{
		return FMCPToolResult::Error(Result->GetStringField(TEXT("error")));
	}

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleInspectNodePins(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));
	FString NodeId = ExtractOptionalString(Params, TEXT("node_id"));

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty() || NodeId.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, to_state, and node_id parameters required"));
	}

	FString Error;
	TSharedPtr<FJsonObject> Result = FAnimationBlueprintUtils::InspectNodePins(
		AnimBP, StateMachineName, FromState, ToState, NodeId, Error);

	if (!Result->GetBoolField(TEXT("success")))
	{
		return FMCPToolResult::Error(Result->GetStringField(TEXT("error")));
	}

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleSetPinDefaultValue(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));
	FString NodeId = ExtractOptionalString(Params, TEXT("node_id"));
	FString PinName = ExtractOptionalString(Params, TEXT("pin_name"));
	FString PinValue = ExtractOptionalString(Params, TEXT("pin_value"));

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty() ||
		NodeId.IsEmpty() || PinName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, to_state, node_id, and pin_name parameters required"));
	}

	FString Error;
	if (!FAnimationBlueprintUtils::SetPinDefaultValue(
		AnimBP, StateMachineName, FromState, ToState, NodeId, PinName, PinValue, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("node_id"), NodeId);
	Result->SetStringField(TEXT("pin_name"), PinName);
	Result->SetStringField(TEXT("value"), PinValue);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set pin '%s' value to '%s'"), *PinName, *PinValue));

	return FMCPToolResult::Success(TEXT("Operation completed"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleAddComparisonChain(const FString& BlueprintPath, const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	FString FromState = ExtractOptionalString(Params, TEXT("from_state"));
	FString ToState = ExtractOptionalString(Params, TEXT("to_state"));
	FString VariableName = ExtractOptionalString(Params, TEXT("variable_name"));
	FString ComparisonType = ExtractOptionalString(Params, TEXT("comparison_type"), TEXT("Less"));
	FString CompareValue = ExtractOptionalString(Params, TEXT("compare_value"));
	FVector2D Position = ExtractPosition(Params);

	if (StateMachineName.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty() || VariableName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine, from_state, to_state, and variable_name parameters required"));
	}

	FString Error;
	TSharedPtr<FJsonObject> Result = FAnimationBlueprintUtils::AddComparisonChain(
		AnimBP, StateMachineName, FromState, ToState,
		VariableName, ComparisonType, CompareValue, Position, Error);

	if (!Result->GetBoolField(TEXT("success")))
	{
		return FMCPToolResult::Error(Result->GetStringField(TEXT("error")));
	}

	FAnimationBlueprintUtils::CompileAnimBlueprint(AnimBP, Error);

	return FMCPToolResult::Success(TEXT("Comparison chain created successfully"), Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleValidateBlueprint(const FString& BlueprintPath)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString Error;
	TSharedPtr<FJsonObject> Result = FAnimationBlueprintUtils::ValidateBlueprint(AnimBP, Error);

	if (!Result->GetBoolField(TEXT("success")))
	{
		return FMCPToolResult::Error(Result->GetStringField(TEXT("error")));
	}

	FString Message = Result->GetBoolField(TEXT("is_valid"))
		? TEXT("Blueprint is valid")
		: FString::Printf(TEXT("Blueprint has %d error(s), %d warning(s)"),
			static_cast<int32>(Result->GetNumberField(TEXT("error_count"))),
			static_cast<int32>(Result->GetNumberField(TEXT("warning_count"))));

	return FMCPToolResult::Success(Message, Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleGetStateMachineDiagram(
	const FString& BlueprintPath,
	const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	FString StateMachineName = Params->GetStringField(TEXT("state_machine"));
	if (StateMachineName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine parameter required"));
	}

	FString Error;
	TSharedPtr<FJsonObject> Result = FAnimationBlueprintUtils::GetStateMachineDiagram(AnimBP, StateMachineName, Error);

	if (!Result.IsValid() || !Result->GetBoolField(TEXT("success")))
	{
		return FMCPToolResult::Error(Error.IsEmpty() ? TEXT("Failed to generate diagram") : Error);
	}

	// Return ASCII diagram in message for easy viewing, with full JSON data
	FString AsciiDiagram = Result->GetStringField(TEXT("ascii_diagram"));
	return FMCPToolResult::Success(AsciiDiagram, Result);
}

FMCPToolResult FMCPTool_AnimBlueprintModify::HandleSetupTransitionConditions(
	const FString& BlueprintPath,
	const TSharedRef<FJsonObject>& Params)
{
	UAnimBlueprint* AnimBP = nullptr;
	if (auto ErrorResult = LoadAnimBlueprintOrError(BlueprintPath, AnimBP))
	{
		return ErrorResult.GetValue();
	}

	// Extract required parameters
	FString StateMachineName = ExtractOptionalString(Params, TEXT("state_machine"));
	if (StateMachineName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("state_machine parameter required"));
	}

	// Get rules array
	const TArray<TSharedPtr<FJsonValue>>* RulesArray;
	if (!Params->TryGetArrayField(TEXT("rules"), RulesArray))
	{
		return FMCPToolResult::Error(TEXT("rules array required for setup_transition_conditions"));
	}

	FString Error;
	TSharedPtr<FJsonObject> Result = FAnimationBlueprintUtils::SetupTransitionConditions(
		AnimBP, StateMachineName, *RulesArray, Error);

	if (!Result.IsValid())
	{
		return FMCPToolResult::Error(Error.IsEmpty() ? TEXT("Failed to setup transition conditions") : Error);
	}

	if (!Result->GetBoolField(TEXT("success")))
	{
		FString ErrorMsg = Result->HasField(TEXT("error"))
			? Result->GetStringField(TEXT("error"))
			: TEXT("Unknown error setting up transition conditions");
		// Return partial results with error
		return FMCPToolResult::Success(ErrorMsg, Result);
	}

	// Build success message with statistics
	int32 RulesProcessed = static_cast<int32>(Result->GetNumberField(TEXT("rules_processed")));
	int32 TransitionsModified = static_cast<int32>(Result->GetNumberField(TEXT("transitions_modified")));

	FString Message = FString::Printf(
		TEXT("Setup transition conditions: %d rules processed, %d transitions modified"),
		RulesProcessed, TransitionsModified);

	return FMCPToolResult::Success(Message, Result);
}
