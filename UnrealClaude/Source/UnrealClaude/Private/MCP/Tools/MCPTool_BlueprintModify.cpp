// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_BlueprintModify.h"
#include "BlueprintUtils.h"
#include "MCP/MCPParamValidator.h"
#include "MCP/MCPBlueprintLoadContext.h"
#include "UnrealClaudeModule.h"
#include "Engine/Blueprint.h"

namespace BlueprintModifyOps
{
	static const FString Create = TEXT("create");
	static const FString AddVariable = TEXT("add_variable");
	static const FString RemoveVariable = TEXT("remove_variable");
	static const FString AddFunction = TEXT("add_function");
	static const FString RemoveFunction = TEXT("remove_function");
	static const FString AddNode = TEXT("add_node");
	static const FString AddNodes = TEXT("add_nodes");
	static const FString DeleteNode = TEXT("delete_node");
	static const FString ConnectPins = TEXT("connect_pins");
	static const FString DisconnectPins = TEXT("disconnect_pins");
	static const FString SetPinValue = TEXT("set_pin_value");
}

FMCPToolResult FMCPTool_BlueprintModify::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	if (Operation == BlueprintModifyOps::Create)
	{
		return ExecuteCreate(Params);
	}
	if (Operation == BlueprintModifyOps::AddVariable)
	{
		return ExecuteAddVariable(Params);
	}
	if (Operation == BlueprintModifyOps::RemoveVariable)
	{
		return ExecuteRemoveVariable(Params);
	}
	if (Operation == BlueprintModifyOps::AddFunction)
	{
		return ExecuteAddFunction(Params);
	}
	if (Operation == BlueprintModifyOps::RemoveFunction)
	{
		return ExecuteRemoveFunction(Params);
	}
	if (Operation == BlueprintModifyOps::AddNode)
	{
		return ExecuteAddNode(Params);
	}
	if (Operation == BlueprintModifyOps::AddNodes)
	{
		return ExecuteAddNodes(Params);
	}
	if (Operation == BlueprintModifyOps::DeleteNode)
	{
		return ExecuteDeleteNode(Params);
	}
	if (Operation == BlueprintModifyOps::ConnectPins)
	{
		return ExecuteConnectPins(Params);
	}
	if (Operation == BlueprintModifyOps::DisconnectPins)
	{
		return ExecuteDisconnectPins(Params);
	}
	if (Operation == BlueprintModifyOps::SetPinValue)
	{
		return ExecuteSetPinValue(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid: create, add_variable, remove_variable, add_function, remove_function, add_node, add_nodes, delete_node, connect_pins, disconnect_pins, set_pin_value"),
		*Operation));
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteCreate(const TSharedRef<FJsonObject>& Params)
{
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

	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(PackagePath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	if (!FMCPParamValidator::ValidateBlueprintVariableName(BlueprintName, ValidationError))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid Blueprint name: %s"), *ValidationError));
	}

	FString ClassError;
	UClass* ParentClass = FBlueprintUtils::FindParentClass(ParentClassName, ClassError);
	if (!ParentClass)
	{
		return FMCPToolResult::Error(ClassError);
	}

	EBlueprintType BlueprintType = ParseBlueprintType(BlueprintTypeStr);

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
	TOptional<FMCPToolResult> Error;
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

	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintVariableName(VariableName, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FEdGraphPinType PinType;
	FString TypeError;
	if (!FBlueprintUtils::ParsePinType(VariableType, PinType, TypeError))
	{
		return FMCPToolResult::Error(TypeError);
	}

	FString AddError;
	if (!FBlueprintUtils::AddVariable(Context.Blueprint, VariableName, PinType, AddError))
	{
		return FMCPToolResult::Error(AddError);
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Variable added")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("variable_name"), VariableName);
	ResultData->SetStringField(TEXT("variable_type"), VariableType);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added variable '%s' (%s) to Blueprint"), *VariableName, *VariableType),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteRemoveVariable(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString VariableName;
	if (!ExtractRequiredString(Params, TEXT("variable_name"), VariableName, Error))
	{
		return Error.GetValue();
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString RemoveError;
	if (!FBlueprintUtils::RemoveVariable(Context.Blueprint, VariableName, RemoveError))
	{
		return FMCPToolResult::Error(RemoveError);
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Variable removed")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("variable_name"), VariableName);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Removed variable '%s' from Blueprint"), *VariableName),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddFunction(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString FunctionName;
	if (!ExtractRequiredString(Params, TEXT("function_name"), FunctionName, Error))
	{
		return Error.GetValue();
	}

	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintFunctionName(FunctionName, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString AddError;
	if (!FBlueprintUtils::AddFunction(Context.Blueprint, FunctionName, AddError))
	{
		return FMCPToolResult::Error(AddError);
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Function added")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("function_name"), FunctionName);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added function '%s' to Blueprint"), *FunctionName),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteRemoveFunction(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString FunctionName;
	if (!ExtractRequiredString(Params, TEXT("function_name"), FunctionName, Error))
	{
		return Error.GetValue();
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString RemoveError;
	if (!FBlueprintUtils::RemoveFunction(Context.Blueprint, FunctionName, RemoveError))
	{
		return FMCPToolResult::Error(RemoveError);
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Function removed")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("function_name"), FunctionName);

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

	return BPTYPE_Normal;
}

// ===== Level 3: Node Operations =====

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddNode(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString NodeType;
	if (!ExtractRequiredString(Params, TEXT("node_type"), NodeType, Error))
	{
		return Error.GetValue();
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);
	int32 PosX = (int32)ExtractOptionalNumber(Params, TEXT("pos_x"), 0);
	int32 PosY = (int32)ExtractOptionalNumber(Params, TEXT("pos_y"), 0);

	TSharedPtr<FJsonObject> NodeParams;
	const TSharedPtr<FJsonObject>* NodeParamsPtr;
	if (Params->TryGetObjectField(TEXT("node_params"), NodeParamsPtr))
	{
		NodeParams = *NodeParamsPtr;
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	FString NodeId;
	FString CreateError;
	UEdGraphNode* NewNode = FBlueprintUtils::CreateNode(Graph, NodeType, NodeParams, PosX, PosY, NodeId, CreateError);
	if (!NewNode)
	{
		return FMCPToolResult::Error(CreateError);
	}

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
					FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, FString(*PinValue.Key), PinValueStr, PinError);
				}
			}
		}
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Node created")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = FBlueprintUtils::SerializeNodeInfo(NewNode);
	ResultData->SetStringField(TEXT("blueprint_path"), Context.Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("graph_name"), Graph->GetName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created node '%s' (type: %s)"), *NodeId, *NodeType),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddNodes(const TSharedRef<FJsonObject>& Params)
{
	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	const TArray<TSharedPtr<FJsonValue>>* NodesArray;
	if (!Params->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		return FMCPToolResult::Error(TEXT("'nodes' array is required"));
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	TArray<FString> CreatedNodeIds;
	TArray<TSharedPtr<FJsonValue>> CreatedNodes;
	FString CreateError;
	if (!CreateNodesFromSpec(Graph, *NodesArray, CreatedNodeIds, CreatedNodes, CreateError))
	{
		return FMCPToolResult::Error(CreateError);
	}

	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray;
	if (Params->TryGetArrayField(TEXT("connections"), ConnectionsArray))
	{
		ProcessNodeConnections(Graph, *ConnectionsArray, CreatedNodeIds);
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Nodes created")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("graph_name"), Graph->GetName());
	ResultData->SetArrayField(TEXT("nodes"), CreatedNodes);
	ResultData->SetNumberField(TEXT("node_count"), CreatedNodeIds.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created %d nodes"), CreatedNodeIds.Num()),
		ResultData
	);
}

bool FMCPTool_BlueprintModify::CreateNodesFromSpec(
	UEdGraph* Graph,
	const TArray<TSharedPtr<FJsonValue>>& NodesArray,
	TArray<FString>& OutCreatedNodeIds,
	TArray<TSharedPtr<FJsonValue>>& OutCreatedNodes,
	FString& OutError)
{
	for (int32 i = 0; i < NodesArray.Num(); i++)
	{
		const TSharedPtr<FJsonObject>* NodeSpec;
		if (!NodesArray[i]->TryGetObject(NodeSpec))
		{
			OutError = FString::Printf(TEXT("Node at index %d is not a valid object"), i);
			return false;
		}

		// Accept both 'type' (add_nodes shape) and 'node_type' (matches sibling add_node)
		FString NodeType = (*NodeSpec)->GetStringField(TEXT("type"));
		if (NodeType.IsEmpty())
		{
			NodeType = (*NodeSpec)->GetStringField(TEXT("node_type"));
		}
		if (NodeType.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Node at index %d missing 'type' (or 'node_type') field"), i);
			return false;
		}

		int32 PosX = (int32)(*NodeSpec)->GetNumberField(TEXT("pos_x"));
		int32 PosY = (int32)(*NodeSpec)->GetNumberField(TEXT("pos_y"));

		// Params can be inline on the node spec, nested under 'params', or 'node_params' (matches add_node)
		TSharedPtr<FJsonObject> NodeParams = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* ParamsPtr;
		if ((*NodeSpec)->TryGetObjectField(TEXT("params"), ParamsPtr) || (*NodeSpec)->TryGetObjectField(TEXT("node_params"), ParamsPtr))
		{
			NodeParams = *ParamsPtr;
		}
		else
		{
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

		FString NodeId;
		FString CreateError;
		UEdGraphNode* NewNode = FBlueprintUtils::CreateNode(Graph, NodeType, NodeParams, PosX, PosY, NodeId, CreateError);
		if (!NewNode)
		{
			OutError = FString::Printf(TEXT("Failed to create node %d: %s"), i, *CreateError);
			return false;
		}

		OutCreatedNodeIds.Add(NodeId);

		const TSharedPtr<FJsonObject>* PinValuesPtr;
		if ((*NodeSpec)->TryGetObjectField(TEXT("pin_values"), PinValuesPtr))
		{
			for (const auto& PinValue : (*PinValuesPtr)->Values)
			{
				FString PinValueStr;
				if (PinValue.Value->TryGetString(PinValueStr))
				{
					FString PinError;
					FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, FString(*PinValue.Key), PinValueStr, PinError);
				}
			}
		}

		TSharedPtr<FJsonObject> NodeInfo = FBlueprintUtils::SerializeNodeInfo(NewNode);
		NodeInfo->SetNumberField(TEXT("index"), i);
		OutCreatedNodes.Add(MakeShared<FJsonValueObject>(NodeInfo));
	}

	return true;
}

void FMCPTool_BlueprintModify::ProcessNodeConnections(
	UEdGraph* Graph,
	const TArray<TSharedPtr<FJsonValue>>& ConnectionsArray,
	const TArray<FString>& CreatedNodeIds)
{
	for (int32 i = 0; i < ConnectionsArray.Num(); i++)
	{
		const TSharedPtr<FJsonObject>* ConnSpec;
		if (!ConnectionsArray[i]->TryGetObject(ConnSpec))
		{
			continue;
		}

		// Accept BOTH naming pairs:
		//   - from_node/to_node/from_pin/to_pin (add_nodes-original shape)
		//   - source_node_id/target_node_id/source_pin/target_pin (matches sibling connect_pins)
		// Each side accepts a numeric index into CreatedNodeIds OR a literal node_id string.
		auto ResolveNodeRef = [&](const TCHAR* PrimaryKey, const TCHAR* AliasKey) -> FString
		{
			const TCHAR* Key = (*ConnSpec)->HasField(PrimaryKey) ? PrimaryKey : AliasKey;
			if ((*ConnSpec)->HasTypedField<EJson::Number>(Key))
			{
				int32 Index = (int32)(*ConnSpec)->GetNumberField(Key);
				if (Index >= 0 && Index < CreatedNodeIds.Num())
				{
					return CreatedNodeIds[Index];
				}
			}
			else if ((*ConnSpec)->HasTypedField<EJson::String>(Key))
			{
				return (*ConnSpec)->GetStringField(Key);
			}
			return FString();
		};
		auto ReadPin = [&](const TCHAR* PrimaryKey, const TCHAR* AliasKey) -> FString
		{
			FString V = (*ConnSpec)->GetStringField(PrimaryKey);
			if (V.IsEmpty()) V = (*ConnSpec)->GetStringField(AliasKey);
			return V;
		};

		FString SourceNodeId = ResolveNodeRef(TEXT("from_node"), TEXT("source_node_id"));
		FString TargetNodeId = ResolveNodeRef(TEXT("to_node"), TEXT("target_node_id"));
		FString SourcePin = ReadPin(TEXT("from_pin"), TEXT("source_pin"));
		FString TargetPin = ReadPin(TEXT("to_pin"), TEXT("target_pin"));

		if (!SourceNodeId.IsEmpty() && !TargetNodeId.IsEmpty())
		{
			FString ConnectError;
			FBlueprintUtils::ConnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, ConnectError);
		}
	}
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteDeleteNode(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString NodeId;
	if (!ExtractRequiredString(Params, TEXT("node_id"), NodeId, Error))
	{
		return Error.GetValue();
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	FString DeleteError;
	if (!FBlueprintUtils::DeleteNode(Graph, NodeId, DeleteError))
	{
		return FMCPToolResult::Error(DeleteError);
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Node deleted")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("node_id"), NodeId);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Deleted node '%s'"), *NodeId),
		ResultData
	);
}

// ===== Level 4: Connection Operations =====

FMCPToolResult FMCPTool_BlueprintModify::ExecuteConnectPins(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
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

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	FString ConnectError;
	if (!FBlueprintUtils::ConnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, ConnectError))
	{
		return FMCPToolResult::Error(ConnectError);
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Pins connected")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("source_node_id"), SourceNodeId);
	ResultData->SetStringField(TEXT("source_pin"), SourcePin.IsEmpty() ? TEXT("(auto exec)") : SourcePin);
	ResultData->SetStringField(TEXT("target_node_id"), TargetNodeId);
	ResultData->SetStringField(TEXT("target_pin"), TargetPin.IsEmpty() ? TEXT("(auto exec)") : TargetPin);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Connected '%s' -> '%s'"), *SourceNodeId, *TargetNodeId),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteDisconnectPins(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
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

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	FString DisconnectError;
	if (!FBlueprintUtils::DisconnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, DisconnectError))
	{
		return FMCPToolResult::Error(DisconnectError);
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Pins disconnected")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("source_node_id"), SourceNodeId);
	ResultData->SetStringField(TEXT("source_pin"), SourcePin);
	ResultData->SetStringField(TEXT("target_node_id"), TargetNodeId);
	ResultData->SetStringField(TEXT("target_pin"), TargetPin);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Disconnected '%s.%s' from '%s.%s'"), *SourceNodeId, *SourcePin, *TargetNodeId, *TargetPin),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteSetPinValue(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
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

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	FString SetError;
	if (!FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, PinName, PinValue, SetError))
	{
		return FMCPToolResult::Error(SetError);
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Pin value set")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("node_id"), NodeId);
	ResultData->SetStringField(TEXT("pin_name"), PinName);
	ResultData->SetStringField(TEXT("pin_value"), PinValue);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set '%s.%s' = '%s'"), *NodeId, *PinName, *PinValue),
		ResultData
	);
}
