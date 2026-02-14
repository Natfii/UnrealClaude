// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

/**
 * REFACTORED: BlueprintUtils - Facade Header
 *
 * This header provides backward compatibility by including the split modules.
 * The original 1,901 LOC god class has been decomposed into:
 *
 * - BlueprintLoader.h: Loading, validation, compilation (~200 LOC)
 * - BlueprintEditor.h: Variable/function management (~450 LOC)
 * - BlueprintGraphEditor.h: Node/connection management (~750 LOC)
 *
 * For new code, prefer using the specific module headers directly.
 * This facade remains for existing code that includes BlueprintUtils.h
 */

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"

// Include the split modules
#include "BlueprintLoader.h"
#include "BlueprintEditor.h"
#include "BlueprintGraphEditor.h"

/**
 * Facade class for backward compatibility with existing code.
 *
 * All methods forward to the appropriate split module:
 * - FBlueprintLoader: Loading, validation, compilation
 * - FBlueprintEditor: Variable and function management
 * - FBlueprintGraphEditor: Node and connection management
 *
 * New code should use the specific module classes directly.
 */
class FBlueprintUtils
{
public:
	// ===== Blueprint Loading & Validation (-> FBlueprintLoader) =====

	FORCEINLINE static UBlueprint* LoadBlueprint(const FString& BlueprintPath, FString& OutError)
	{
		return FBlueprintLoader::LoadBlueprint(BlueprintPath, OutError);
	}

	FORCEINLINE static bool ValidateBlueprintPath(const FString& BlueprintPath, FString& OutError)
	{
		return FBlueprintLoader::ValidateBlueprintPath(BlueprintPath, OutError);
	}

	FORCEINLINE static bool IsBlueprintEditable(UBlueprint* Blueprint, FString& OutError)
	{
		return FBlueprintLoader::IsBlueprintEditable(Blueprint, OutError);
	}

	FORCEINLINE static bool CompileBlueprint(UBlueprint* Blueprint, FString& OutError)
	{
		return FBlueprintLoader::CompileBlueprint(Blueprint, OutError);
	}

	/** Compile with detailed result including error messages */
	FORCEINLINE static FBlueprintCompileResult CompileBlueprintWithResult(UBlueprint* Blueprint)
	{
		return FBlueprintLoader::CompileBlueprintWithResult(Blueprint);
	}

	FORCEINLINE static void MarkBlueprintDirty(UBlueprint* Blueprint)
	{
		FBlueprintLoader::MarkBlueprintDirty(Blueprint);
	}

	FORCEINLINE static UBlueprint* CreateBlueprint(
		const FString& PackagePath,
		const FString& BlueprintName,
		UClass* ParentClass,
		EBlueprintType BlueprintType,
		FString& OutError)
	{
		return FBlueprintLoader::CreateBlueprint(PackagePath, BlueprintName, ParentClass, BlueprintType, OutError);
	}

	FORCEINLINE static UClass* FindParentClass(const FString& ParentClassName, FString& OutError)
	{
		return FBlueprintLoader::FindParentClass(ParentClassName, OutError);
	}

	// ===== Variable Management (-> FBlueprintEditor) =====

	FORCEINLINE static bool AddVariable(
		UBlueprint* Blueprint,
		const FString& VariableName,
		const FEdGraphPinType& PinType,
		FString& OutError)
	{
		return FBlueprintEditor::AddVariable(Blueprint, VariableName, PinType, OutError);
	}

	FORCEINLINE static bool RemoveVariable(
		UBlueprint* Blueprint,
		const FString& VariableName,
		FString& OutError)
	{
		return FBlueprintEditor::RemoveVariable(Blueprint, VariableName, OutError);
	}

	FORCEINLINE static bool ParsePinType(
		const FString& TypeString,
		FEdGraphPinType& OutPinType,
		FString& OutError)
	{
		return FBlueprintEditor::ParsePinType(TypeString, OutPinType, OutError);
	}

	FORCEINLINE static FString PinTypeToString(const FEdGraphPinType& PinType)
	{
		return FBlueprintEditor::PinTypeToString(PinType);
	}

	FORCEINLINE static bool ValidateVariableName(const FString& VariableName, FString& OutError)
	{
		return FBlueprintEditor::ValidateVariableName(VariableName, OutError);
	}

	FORCEINLINE static bool ValidateFunctionName(const FString& FunctionName, FString& OutError)
	{
		return FBlueprintEditor::ValidateFunctionName(FunctionName, OutError);
	}

	// ===== Function Management (-> FBlueprintEditor) =====

	FORCEINLINE static bool AddFunction(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		FString& OutError)
	{
		return FBlueprintEditor::AddFunction(Blueprint, FunctionName, OutError);
	}

	FORCEINLINE static bool RemoveFunction(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		FString& OutError)
	{
		return FBlueprintEditor::RemoveFunction(Blueprint, FunctionName, OutError);
	}

	// ===== Graph & Node Management (-> FBlueprintGraphEditor) =====

	FORCEINLINE static UEdGraph* FindGraph(
		UBlueprint* Blueprint,
		const FString& GraphName,
		bool bFunctionGraph,
		FString& OutError)
	{
		return FBlueprintGraphEditor::FindGraph(Blueprint, GraphName, bFunctionGraph, OutError);
	}

	FORCEINLINE static UEdGraphNode* FindNodeById(UEdGraph* Graph, const FString& NodeId)
	{
		return FBlueprintGraphEditor::FindNodeById(Graph, NodeId);
	}

	FORCEINLINE static FString GenerateNodeId(const FString& NodeType, const FString& Context, UEdGraph* Graph)
	{
		return FBlueprintGraphEditor::GenerateNodeId(NodeType, Context, Graph);
	}

	FORCEINLINE static void SetNodeId(UEdGraphNode* Node, const FString& NodeId)
	{
		FBlueprintGraphEditor::SetNodeId(Node, NodeId);
	}

	FORCEINLINE static FString GetNodeId(UEdGraphNode* Node)
	{
		return FBlueprintGraphEditor::GetNodeId(Node);
	}

	FORCEINLINE static UEdGraphNode* CreateNode(
		UEdGraph* Graph,
		const FString& NodeType,
		const TSharedPtr<FJsonObject>& NodeParams,
		int32 PosX,
		int32 PosY,
		FString& OutNodeId,
		FString& OutError)
	{
		return FBlueprintGraphEditor::CreateNode(Graph, NodeType, NodeParams, PosX, PosY, OutNodeId, OutError);
	}

	FORCEINLINE static bool DeleteNode(UEdGraph* Graph, const FString& NodeId, FString& OutError)
	{
		return FBlueprintGraphEditor::DeleteNode(Graph, NodeId, OutError);
	}

	FORCEINLINE static bool ConnectPins(
		UEdGraph* Graph,
		const FString& SourceNodeId,
		const FString& SourcePinName,
		const FString& TargetNodeId,
		const FString& TargetPinName,
		FString& OutError)
	{
		return FBlueprintGraphEditor::ConnectPins(Graph, SourceNodeId, SourcePinName, TargetNodeId, TargetPinName, OutError);
	}

	FORCEINLINE static bool DisconnectPins(
		UEdGraph* Graph,
		const FString& SourceNodeId,
		const FString& SourcePinName,
		const FString& TargetNodeId,
		const FString& TargetPinName,
		FString& OutError)
	{
		return FBlueprintGraphEditor::DisconnectPins(Graph, SourceNodeId, SourcePinName, TargetNodeId, TargetPinName, OutError);
	}

	FORCEINLINE static bool SetPinDefaultValue(
		UEdGraph* Graph,
		const FString& NodeId,
		const FString& PinName,
		const FString& Value,
		FString& OutError)
	{
		return FBlueprintGraphEditor::SetPinDefaultValue(Graph, NodeId, PinName, Value, OutError);
	}

	FORCEINLINE static UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction = EGPD_MAX)
	{
		return FBlueprintGraphEditor::FindPinByName(Node, PinName, Direction);
	}

	FORCEINLINE static UEdGraphPin* GetExecPin(UEdGraphNode* Node, bool bOutput)
	{
		return FBlueprintGraphEditor::GetExecPin(Node, bOutput);
	}

	FORCEINLINE static TSharedPtr<FJsonObject> SerializeNodeInfo(UEdGraphNode* Node)
	{
		return FBlueprintGraphEditor::SerializeNodeInfo(Node);
	}

	// ===== Introspection Functions (remain in this file for now) =====

	/**
	 * Get Blueprint type as string
	 */
	static FString GetBlueprintTypeString(EBlueprintType Type);

	/**
	 * Serialize Blueprint info to JSON
	 */
	static TSharedPtr<FJsonObject> SerializeBlueprintInfo(
		UBlueprint* Blueprint,
		bool bIncludeVariables = true,
		bool bIncludeFunctions = true,
		bool bIncludeGraphs = false
	);

	/**
	 * Get all variables in Blueprint with metadata
	 */
	static TArray<TSharedPtr<FJsonValue>> GetBlueprintVariables(UBlueprint* Blueprint);

	/**
	 * Get all functions in Blueprint with signatures
	 */
	static TArray<TSharedPtr<FJsonValue>> GetBlueprintFunctions(UBlueprint* Blueprint);

	/**
	 * Get graph information (node count, events)
	 */
	static TSharedPtr<FJsonObject> GetGraphInfo(UBlueprint* Blueprint);
};
