// Copyright Your Name. All Rights Reserved.

#include "MCPTool_BlueprintModify.h"
#include "BlueprintUtils.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Engine/Blueprint.h"

FMCPToolResult FMCPTool_BlueprintModify::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Get operation type
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	// Level 2: Variable/Function Operations
	if (Operation == TEXT("create"))
	{
		return ExecuteCreate(Params);
	}
	else if (Operation == TEXT("add_variable"))
	{
		return ExecuteAddVariable(Params);
	}
	else if (Operation == TEXT("remove_variable"))
	{
		return ExecuteRemoveVariable(Params);
	}
	else if (Operation == TEXT("add_function"))
	{
		return ExecuteAddFunction(Params);
	}
	else if (Operation == TEXT("remove_function"))
	{
		return ExecuteRemoveFunction(Params);
	}
	// Level 3: Node Operations
	else if (Operation == TEXT("add_node"))
	{
		return ExecuteAddNode(Params);
	}
	else if (Operation == TEXT("add_nodes"))
	{
		return ExecuteAddNodes(Params);
	}
	else if (Operation == TEXT("delete_node"))
	{
		return ExecuteDeleteNode(Params);
	}
	// Level 4: Connection Operations
	else if (Operation == TEXT("connect_pins"))
	{
		return ExecuteConnectPins(Params);
	}
	else if (Operation == TEXT("disconnect_pins"))
	{
		return ExecuteDisconnectPins(Params);
	}
	else if (Operation == TEXT("set_pin_value"))
	{
		return ExecuteSetPinValue(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid: create, add_variable, remove_variable, add_function, remove_function, add_node, add_nodes, delete_node, connect_pins, disconnect_pins, set_pin_value"),
		*Operation));
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteCreate(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString PackagePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("package_path"), PackagePath, Error))
	{
		return Error.GetValue();
	}

	FString BlueprintName;
	if (!ExtractRequiredString(Params, TEXT("blueprint_name"), BlueprintName, Error))
	{
		return Error.GetValue();
	}

	FString ParentClassName;
	if (!ExtractRequiredString(Params, TEXT("parent_class"), ParentClassName, Error))
	{
		return Error.GetValue();
	}

	FString BlueprintTypeStr = ExtractOptionalString(Params, TEXT("blueprint_type"), TEXT("Normal"));

	// Validate package path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(PackagePath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Validate Blueprint name
	if (!FMCPParamValidator::ValidateBlueprintVariableName(BlueprintName, ValidationError))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid Blueprint name: %s"), *ValidationError));
	}

	// Find parent class
	FString ClassError;
	UClass* ParentClass = FBlueprintUtils::FindParentClass(ParentClassName, ClassError);
	if (!ParentClass)
	{
		return FMCPToolResult::Error(ClassError);
	}

	// Parse Blueprint type
	EBlueprintType BlueprintType = ParseBlueprintType(BlueprintTypeStr);

	// Create the Blueprint
	FString CreateError;
	UBlueprint* NewBlueprint = FBlueprintUtils::CreateBlueprint(
		PackagePath,
		BlueprintName,
		ParentClass,
		BlueprintType,
		CreateError
	);

	if (!NewBlueprint)
	{
		return FMCPToolResult::Error(CreateError);
	}

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_name"), NewBlueprint->GetName());
	ResultData->SetStringField(TEXT("blueprint_path"), NewBlueprint->GetPathName());
	ResultData->SetStringField(TEXT("parent_class"), ParentClass->GetName());
	ResultData->SetStringField(TEXT("blueprint_type"), FBlueprintUtils::GetBlueprintTypeString(BlueprintType));
	ResultData->SetBoolField(TEXT("compiled"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created Blueprint: %s"), *NewBlueprint->GetPathName()),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddVariable(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString VariableName;
	if (!ExtractRequiredString(Params, TEXT("variable_name"), VariableName, Error))
	{
		return Error.GetValue();
	}

	FString VariableType;
	if (!ExtractRequiredString(Params, TEXT("variable_type"), VariableType, Error))
	{
		return Error.GetValue();
	}

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Validate variable name
	if (!FMCPParamValidator::ValidateBlueprintVariableName(VariableName, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if Blueprint is editable
	FString EditError;
	if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, EditError))
	{
		return FMCPToolResult::Error(EditError);
	}

	// Parse variable type
	FEdGraphPinType PinType;
	FString TypeError;
	if (!FBlueprintUtils::ParsePinType(VariableType, PinType, TypeError))
	{
		return FMCPToolResult::Error(TypeError);
	}

	// Add the variable
	FString AddError;
	if (!FBlueprintUtils::AddVariable(Blueprint, VariableName, PinType, AddError))
	{
		return FMCPToolResult::Error(AddError);
	}

	// Compile the Blueprint
	FString CompileError;
	if (!FBlueprintUtils::CompileBlueprint(Blueprint, CompileError))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Variable added but compilation failed: %s"), *CompileError));
	}

	// Mark dirty
	FBlueprintUtils::MarkBlueprintDirty(Blueprint);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("variable_name"), VariableName);
	ResultData->SetStringField(TEXT("variable_type"), VariableType);
	ResultData->SetBoolField(TEXT("compiled"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added variable '%s' (%s) to Blueprint"), *VariableName, *VariableType),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteRemoveVariable(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString VariableName;
	if (!ExtractRequiredString(Params, TEXT("variable_name"), VariableName, Error))
	{
		return Error.GetValue();
	}

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if Blueprint is editable
	FString EditError;
	if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, EditError))
	{
		return FMCPToolResult::Error(EditError);
	}

	// Remove the variable
	FString RemoveError;
	if (!FBlueprintUtils::RemoveVariable(Blueprint, VariableName, RemoveError))
	{
		return FMCPToolResult::Error(RemoveError);
	}

	// Compile the Blueprint
	FString CompileError;
	if (!FBlueprintUtils::CompileBlueprint(Blueprint, CompileError))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Variable removed but compilation failed: %s"), *CompileError));
	}

	// Mark dirty
	FBlueprintUtils::MarkBlueprintDirty(Blueprint);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("variable_name"), VariableName);
	ResultData->SetBoolField(TEXT("compiled"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Removed variable '%s' from Blueprint"), *VariableName),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddFunction(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString FunctionName;
	if (!ExtractRequiredString(Params, TEXT("function_name"), FunctionName, Error))
	{
		return Error.GetValue();
	}

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Validate function name
	if (!FMCPParamValidator::ValidateBlueprintFunctionName(FunctionName, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if Blueprint is editable
	FString EditError;
	if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, EditError))
	{
		return FMCPToolResult::Error(EditError);
	}

	// Add the function
	FString AddError;
	if (!FBlueprintUtils::AddFunction(Blueprint, FunctionName, AddError))
	{
		return FMCPToolResult::Error(AddError);
	}

	// Compile the Blueprint
	FString CompileError;
	if (!FBlueprintUtils::CompileBlueprint(Blueprint, CompileError))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Function added but compilation failed: %s"), *CompileError));
	}

	// Mark dirty
	FBlueprintUtils::MarkBlueprintDirty(Blueprint);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("function_name"), FunctionName);
	ResultData->SetBoolField(TEXT("compiled"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added function '%s' to Blueprint"), *FunctionName),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteRemoveFunction(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString FunctionName;
	if (!ExtractRequiredString(Params, TEXT("function_name"), FunctionName, Error))
	{
		return Error.GetValue();
	}

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if Blueprint is editable
	FString EditError;
	if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, EditError))
	{
		return FMCPToolResult::Error(EditError);
	}

	// Remove the function
	FString RemoveError;
	if (!FBlueprintUtils::RemoveFunction(Blueprint, FunctionName, RemoveError))
	{
		return FMCPToolResult::Error(RemoveError);
	}

	// Compile the Blueprint
	FString CompileError;
	if (!FBlueprintUtils::CompileBlueprint(Blueprint, CompileError))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Function removed but compilation failed: %s"), *CompileError));
	}

	// Mark dirty
	FBlueprintUtils::MarkBlueprintDirty(Blueprint);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("function_name"), FunctionName);
	ResultData->SetBoolField(TEXT("compiled"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Removed function '%s' from Blueprint"), *FunctionName),
		ResultData
	);
}

EBlueprintType FMCPTool_BlueprintModify::ParseBlueprintType(const FString& TypeString)
{
	FString LowerType = TypeString.ToLower();

	if (LowerType == TEXT("normal") || LowerType == TEXT("actor") || LowerType == TEXT("object"))
	{
		return BPTYPE_Normal;
	}
	if (LowerType == TEXT("functionlibrary") || LowerType == TEXT("function_library"))
	{
		return BPTYPE_FunctionLibrary;
	}
	if (LowerType == TEXT("interface"))
	{
		return BPTYPE_Interface;
	}
	if (LowerType == TEXT("macrolibrary") || LowerType == TEXT("macro_library") || LowerType == TEXT("macro"))
	{
		return BPTYPE_MacroLibrary;
	}

	// Default to normal
	return BPTYPE_Normal;
}

// ===== Level 3: Node Operations =====

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddNode(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString NodeType;
	if (!ExtractRequiredString(Params, TEXT("node_type"), NodeType, Error))
	{
		return Error.GetValue();
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);
	int32 PosX = (int32)ExtractOptionalNumber(Params, TEXT("pos_x"), 0);
	int32 PosY = (int32)ExtractOptionalNumber(Params, TEXT("pos_y"), 0);

	// Get node params object
	TSharedPtr<FJsonObject> NodeParams;
	const TSharedPtr<FJsonObject>* NodeParamsPtr;
	if (Params->TryGetObjectField(TEXT("node_params"), NodeParamsPtr))
	{
		NodeParams = *NodeParamsPtr;
	}

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if Blueprint is editable
	FString EditError;
	if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, EditError))
	{
		return FMCPToolResult::Error(EditError);
	}

	// Find graph
	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	// Create the node
	FString NodeId;
	FString CreateError;
	UEdGraphNode* NewNode = FBlueprintUtils::CreateNode(Graph, NodeType, NodeParams, PosX, PosY, NodeId, CreateError);
	if (!NewNode)
	{
		return FMCPToolResult::Error(CreateError);
	}

	// Apply pin default values if provided
	if (NodeParams.IsValid())
	{
		const TSharedPtr<FJsonObject>* PinValuesPtr;
		if (NodeParams->TryGetObjectField(TEXT("pin_values"), PinValuesPtr))
		{
			for (const auto& PinValue : (*PinValuesPtr)->Values)
			{
				FString PinValueStr;
				if (PinValue.Value->TryGetString(PinValueStr))
				{
					FString PinError;
					FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, PinValue.Key, PinValueStr, PinError);
					// Continue even if pin value setting fails (non-fatal)
				}
			}
		}
	}

	// Compile the Blueprint
	FString CompileError;
	if (!FBlueprintUtils::CompileBlueprint(Blueprint, CompileError))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Node created but compilation failed: %s"), *CompileError));
	}

	// Mark dirty
	FBlueprintUtils::MarkBlueprintDirty(Blueprint);

	// Build result
	TSharedPtr<FJsonObject> ResultData = FBlueprintUtils::SerializeNodeInfo(NewNode);
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("graph_name"), Graph->GetName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created node '%s' (type: %s)"), *NodeId, *NodeType),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddNodes(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	// Get nodes array
	const TArray<TSharedPtr<FJsonValue>>* NodesArray;
	if (!Params->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		return FMCPToolResult::Error(TEXT("'nodes' array is required"));
	}

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if Blueprint is editable
	FString EditError;
	if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, EditError))
	{
		return FMCPToolResult::Error(EditError);
	}

	// Find graph
	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	// Create all nodes
	TArray<FString> CreatedNodeIds;
	TArray<TSharedPtr<FJsonValue>> CreatedNodes;

	for (int32 i = 0; i < NodesArray->Num(); i++)
	{
		const TSharedPtr<FJsonObject>* NodeSpec;
		if (!(*NodesArray)[i]->TryGetObject(NodeSpec))
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Node at index %d is not a valid object"), i));
		}

		FString NodeType = (*NodeSpec)->GetStringField(TEXT("type"));
		if (NodeType.IsEmpty())
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Node at index %d missing 'type' field"), i));
		}

		int32 PosX = (int32)(*NodeSpec)->GetNumberField(TEXT("pos_x"));
		int32 PosY = (int32)(*NodeSpec)->GetNumberField(TEXT("pos_y"));

		// Get params (could be inline or nested)
		TSharedPtr<FJsonObject> NodeParams = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* ParamsPtr;
		if ((*NodeSpec)->TryGetObjectField(TEXT("params"), ParamsPtr))
		{
			NodeParams = *ParamsPtr;
		}
		else
		{
			// Copy common fields to params
			if ((*NodeSpec)->HasField(TEXT("function")))
				NodeParams->SetStringField(TEXT("function"), (*NodeSpec)->GetStringField(TEXT("function")));
			if ((*NodeSpec)->HasField(TEXT("target_class")))
				NodeParams->SetStringField(TEXT("target_class"), (*NodeSpec)->GetStringField(TEXT("target_class")));
			if ((*NodeSpec)->HasField(TEXT("event")))
				NodeParams->SetStringField(TEXT("event"), (*NodeSpec)->GetStringField(TEXT("event")));
			if ((*NodeSpec)->HasField(TEXT("variable")))
				NodeParams->SetStringField(TEXT("variable"), (*NodeSpec)->GetStringField(TEXT("variable")));
			if ((*NodeSpec)->HasField(TEXT("num_outputs")))
				NodeParams->SetNumberField(TEXT("num_outputs"), (*NodeSpec)->GetNumberField(TEXT("num_outputs")));
		}

		// Create node
		FString NodeId;
		FString CreateError;
		UEdGraphNode* NewNode = FBlueprintUtils::CreateNode(Graph, NodeType, NodeParams, PosX, PosY, NodeId, CreateError);
		if (!NewNode)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create node %d: %s"), i, *CreateError));
		}

		CreatedNodeIds.Add(NodeId);

		// Apply pin default values if provided
		const TSharedPtr<FJsonObject>* PinValuesPtr;
		if ((*NodeSpec)->TryGetObjectField(TEXT("pin_values"), PinValuesPtr))
		{
			for (const auto& PinValue : (*PinValuesPtr)->Values)
			{
				FString PinValueStr;
				if (PinValue.Value->TryGetString(PinValueStr))
				{
					FString PinError;
					FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, PinValue.Key, PinValueStr, PinError);
				}
			}
		}

		// Add to result
		TSharedPtr<FJsonObject> NodeInfo = FBlueprintUtils::SerializeNodeInfo(NewNode);
		NodeInfo->SetNumberField(TEXT("index"), i);
		CreatedNodes.Add(MakeShared<FJsonValueObject>(NodeInfo));
	}

	// Process connections
	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray;
	if (Params->TryGetArrayField(TEXT("connections"), ConnectionsArray))
	{
		for (int32 i = 0; i < ConnectionsArray->Num(); i++)
		{
			const TSharedPtr<FJsonObject>* ConnSpec;
			if (!(*ConnectionsArray)[i]->TryGetObject(ConnSpec))
			{
				continue;
			}

			// Get source - can be index or node_id
			FString SourceNodeId;
			if ((*ConnSpec)->HasTypedField<EJson::Number>(TEXT("from_node")))
			{
				int32 FromIndex = (int32)(*ConnSpec)->GetNumberField(TEXT("from_node"));
				if (FromIndex >= 0 && FromIndex < CreatedNodeIds.Num())
				{
					SourceNodeId = CreatedNodeIds[FromIndex];
				}
			}
			else if ((*ConnSpec)->HasTypedField<EJson::String>(TEXT("from_node")))
			{
				SourceNodeId = (*ConnSpec)->GetStringField(TEXT("from_node"));
			}

			// Get target - can be index or node_id
			FString TargetNodeId;
			if ((*ConnSpec)->HasTypedField<EJson::Number>(TEXT("to_node")))
			{
				int32 ToIndex = (int32)(*ConnSpec)->GetNumberField(TEXT("to_node"));
				if (ToIndex >= 0 && ToIndex < CreatedNodeIds.Num())
				{
					TargetNodeId = CreatedNodeIds[ToIndex];
				}
			}
			else if ((*ConnSpec)->HasTypedField<EJson::String>(TEXT("to_node")))
			{
				TargetNodeId = (*ConnSpec)->GetStringField(TEXT("to_node"));
			}

			FString SourcePin = (*ConnSpec)->GetStringField(TEXT("from_pin"));
			FString TargetPin = (*ConnSpec)->GetStringField(TEXT("to_pin"));

			if (!SourceNodeId.IsEmpty() && !TargetNodeId.IsEmpty())
			{
				FString ConnectError;
				FBlueprintUtils::ConnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, ConnectError);
				// Continue even if connection fails (non-fatal)
			}
		}
	}

	// Compile the Blueprint
	FString CompileError;
	if (!FBlueprintUtils::CompileBlueprint(Blueprint, CompileError))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Nodes created but compilation failed: %s"), *CompileError));
	}

	// Mark dirty
	FBlueprintUtils::MarkBlueprintDirty(Blueprint);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("graph_name"), Graph->GetName());
	ResultData->SetArrayField(TEXT("nodes"), CreatedNodes);
	ResultData->SetNumberField(TEXT("node_count"), CreatedNodeIds.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created %d nodes"), CreatedNodeIds.Num()),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteDeleteNode(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString NodeId;
	if (!ExtractRequiredString(Params, TEXT("node_id"), NodeId, Error))
	{
		return Error.GetValue();
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if Blueprint is editable
	FString EditError;
	if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, EditError))
	{
		return FMCPToolResult::Error(EditError);
	}

	// Find graph
	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	// Delete the node
	FString DeleteError;
	if (!FBlueprintUtils::DeleteNode(Graph, NodeId, DeleteError))
	{
		return FMCPToolResult::Error(DeleteError);
	}

	// Compile the Blueprint
	FString CompileError;
	if (!FBlueprintUtils::CompileBlueprint(Blueprint, CompileError))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Node deleted but compilation failed: %s"), *CompileError));
	}

	// Mark dirty
	FBlueprintUtils::MarkBlueprintDirty(Blueprint);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("node_id"), NodeId);
	ResultData->SetBoolField(TEXT("compiled"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Deleted node '%s'"), *NodeId),
		ResultData
	);
}

// ===== Level 4: Connection Operations =====

FMCPToolResult FMCPTool_BlueprintModify::ExecuteConnectPins(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString SourceNodeId;
	if (!ExtractRequiredString(Params, TEXT("source_node_id"), SourceNodeId, Error))
	{
		return Error.GetValue();
	}

	FString TargetNodeId;
	if (!ExtractRequiredString(Params, TEXT("target_node_id"), TargetNodeId, Error))
	{
		return Error.GetValue();
	}

	FString SourcePin = ExtractOptionalString(Params, TEXT("source_pin"), TEXT(""));
	FString TargetPin = ExtractOptionalString(Params, TEXT("target_pin"), TEXT(""));
	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if Blueprint is editable
	FString EditError;
	if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, EditError))
	{
		return FMCPToolResult::Error(EditError);
	}

	// Find graph
	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	// Connect the pins
	FString ConnectError;
	if (!FBlueprintUtils::ConnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, ConnectError))
	{
		return FMCPToolResult::Error(ConnectError);
	}

	// Compile the Blueprint
	FString CompileError;
	if (!FBlueprintUtils::CompileBlueprint(Blueprint, CompileError))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Pins connected but compilation failed: %s"), *CompileError));
	}

	// Mark dirty
	FBlueprintUtils::MarkBlueprintDirty(Blueprint);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("source_node_id"), SourceNodeId);
	ResultData->SetStringField(TEXT("source_pin"), SourcePin.IsEmpty() ? TEXT("(auto exec)") : SourcePin);
	ResultData->SetStringField(TEXT("target_node_id"), TargetNodeId);
	ResultData->SetStringField(TEXT("target_pin"), TargetPin.IsEmpty() ? TEXT("(auto exec)") : TargetPin);
	ResultData->SetBoolField(TEXT("compiled"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Connected '%s' -> '%s'"), *SourceNodeId, *TargetNodeId),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteDisconnectPins(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString SourceNodeId;
	if (!ExtractRequiredString(Params, TEXT("source_node_id"), SourceNodeId, Error))
	{
		return Error.GetValue();
	}

	FString SourcePin;
	if (!ExtractRequiredString(Params, TEXT("source_pin"), SourcePin, Error))
	{
		return Error.GetValue();
	}

	FString TargetNodeId;
	if (!ExtractRequiredString(Params, TEXT("target_node_id"), TargetNodeId, Error))
	{
		return Error.GetValue();
	}

	FString TargetPin;
	if (!ExtractRequiredString(Params, TEXT("target_pin"), TargetPin, Error))
	{
		return Error.GetValue();
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if Blueprint is editable
	FString EditError;
	if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, EditError))
	{
		return FMCPToolResult::Error(EditError);
	}

	// Find graph
	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	// Disconnect the pins
	FString DisconnectError;
	if (!FBlueprintUtils::DisconnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, DisconnectError))
	{
		return FMCPToolResult::Error(DisconnectError);
	}

	// Compile the Blueprint
	FString CompileError;
	if (!FBlueprintUtils::CompileBlueprint(Blueprint, CompileError))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Pins disconnected but compilation failed: %s"), *CompileError));
	}

	// Mark dirty
	FBlueprintUtils::MarkBlueprintDirty(Blueprint);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("source_node_id"), SourceNodeId);
	ResultData->SetStringField(TEXT("source_pin"), SourcePin);
	ResultData->SetStringField(TEXT("target_node_id"), TargetNodeId);
	ResultData->SetStringField(TEXT("target_pin"), TargetPin);
	ResultData->SetBoolField(TEXT("compiled"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Disconnected '%s.%s' from '%s.%s'"), *SourceNodeId, *SourcePin, *TargetNodeId, *TargetPin),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteSetPinValue(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	FString NodeId;
	if (!ExtractRequiredString(Params, TEXT("node_id"), NodeId, Error))
	{
		return Error.GetValue();
	}

	FString PinName;
	if (!ExtractRequiredString(Params, TEXT("pin_name"), PinName, Error))
	{
		return Error.GetValue();
	}

	FString PinValue;
	if (!ExtractRequiredString(Params, TEXT("pin_value"), PinValue, Error))
	{
		return Error.GetValue();
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if Blueprint is editable
	FString EditError;
	if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, EditError))
	{
		return FMCPToolResult::Error(EditError);
	}

	// Find graph
	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	// Set the pin value
	FString SetError;
	if (!FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, PinName, PinValue, SetError))
	{
		return FMCPToolResult::Error(SetError);
	}

	// Compile the Blueprint
	FString CompileError;
	if (!FBlueprintUtils::CompileBlueprint(Blueprint, CompileError))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Pin value set but compilation failed: %s"), *CompileError));
	}

	// Mark dirty
	FBlueprintUtils::MarkBlueprintDirty(Blueprint);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("node_id"), NodeId);
	ResultData->SetStringField(TEXT("pin_name"), PinName);
	ResultData->SetStringField(TEXT("pin_value"), PinValue);
	ResultData->SetBoolField(TEXT("compiled"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set '%s.%s' = '%s'"), *NodeId, *PinName, *PinValue),
		ResultData
	);
}
