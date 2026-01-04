// Copyright Your Name. All Rights Reserved.

#include "BlueprintEditor.h"
#include "UnrealClaudeModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_FunctionEntry.h"
#include "EdGraph/EdGraph.h"

// ===== Variable Management =====

bool FBlueprintEditor::AddVariable(
	UBlueprint* Blueprint,
	const FString& VariableName,
	const FEdGraphPinType& PinType,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	if (!ValidateVariableName(VariableName, OutError))
	{
		return false;
	}

	// Check for existing variable
	FName VarName(*VariableName);
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == VarName)
		{
			OutError = FString::Printf(TEXT("Variable '%s' already exists"), *VariableName);
			return false;
		}
	}

	// Add the variable
	if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, PinType))
	{
		OutError = TEXT("Failed to add variable");
		return false;
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Added variable '%s' to Blueprint '%s'"),
		*VariableName, *Blueprint->GetName());
	return true;
}

bool FBlueprintEditor::RemoveVariable(
	UBlueprint* Blueprint,
	const FString& VariableName,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	FName VarName(*VariableName);

	// Verify variable exists
	bool bFound = false;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == VarName)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		OutError = FString::Printf(TEXT("Variable '%s' not found"), *VariableName);
		return false;
	}

	FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VarName);

	UE_LOG(LogUnrealClaude, Log, TEXT("Removed variable '%s' from Blueprint '%s'"),
		*VariableName, *Blueprint->GetName());
	return true;
}

// ===== Function Management =====

bool FBlueprintEditor::AddFunction(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	if (!ValidateFunctionName(FunctionName, OutError))
	{
		return false;
	}

	// Check for existing function
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			OutError = FString::Printf(TEXT("Function '%s' already exists"), *FunctionName);
			return false;
		}
	}

	// Create function graph
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		OutError = TEXT("Failed to create function graph");
		return false;
	}

	// Initialize and add to Blueprint
	// nullptr cast for UE 5.7 template deduction
	FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewGraph, false, static_cast<UFunction*>(nullptr));

	// Ensure function entry node exists
	bool bHasEntry = false;
	for (UEdGraphNode* Node : NewGraph->Nodes)
	{
		if (Cast<UK2Node_FunctionEntry>(Node))
		{
			bHasEntry = true;
			break;
		}
	}

	if (!bHasEntry)
	{
		// Create function entry node
		UK2Node_FunctionEntry* EntryNode = NewObject<UK2Node_FunctionEntry>(NewGraph);
		EntryNode->CreateNewGuid();
		EntryNode->PostPlacedNewNode();
		EntryNode->AllocateDefaultPins();
		NewGraph->AddNode(EntryNode);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Added function '%s' to Blueprint '%s'"),
		*FunctionName, *Blueprint->GetName());
	return true;
}

bool FBlueprintEditor::RemoveFunction(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	// Find function graph
	UEdGraph* GraphToRemove = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			GraphToRemove = Graph;
			break;
		}
	}

	if (!GraphToRemove)
	{
		OutError = FString::Printf(TEXT("Function '%s' not found"), *FunctionName);
		return false;
	}

	FBlueprintEditorUtils::RemoveGraph(Blueprint, GraphToRemove);

	UE_LOG(LogUnrealClaude, Log, TEXT("Removed function '%s' from Blueprint '%s'"),
		*FunctionName, *Blueprint->GetName());
	return true;
}

// ===== Name Validation =====

bool FBlueprintEditor::ValidateVariableName(const FString& VariableName, FString& OutError)
{
	if (VariableName.IsEmpty())
	{
		OutError = TEXT("Variable name cannot be empty");
		return false;
	}

	if (VariableName.Len() > MaxNameLength)
	{
		OutError = FString::Printf(TEXT("Variable name exceeds maximum length of %d characters"), MaxNameLength);
		return false;
	}

	// Must start with letter or underscore
	if (!FChar::IsAlpha(VariableName[0]) && VariableName[0] != TEXT('_'))
	{
		OutError = TEXT("Variable name must start with a letter or underscore");
		return false;
	}

	// Only alphanumeric and underscore
	for (TCHAR C : VariableName)
	{
		if (!FChar::IsAlnum(C) && C != TEXT('_'))
		{
			OutError = FString::Printf(TEXT("Variable name contains invalid character: '%c'"), C);
			return false;
		}
	}

	return true;
}

bool FBlueprintEditor::ValidateFunctionName(const FString& FunctionName, FString& OutError)
{
	// Same validation rules as variables
	if (FunctionName.IsEmpty())
	{
		OutError = TEXT("Function name cannot be empty");
		return false;
	}

	if (FunctionName.Len() > MaxNameLength)
	{
		OutError = FString::Printf(TEXT("Function name exceeds maximum length of %d characters"), MaxNameLength);
		return false;
	}

	if (!FChar::IsAlpha(FunctionName[0]) && FunctionName[0] != TEXT('_'))
	{
		OutError = TEXT("Function name must start with a letter or underscore");
		return false;
	}

	for (TCHAR C : FunctionName)
	{
		if (!FChar::IsAlnum(C) && C != TEXT('_'))
		{
			OutError = FString::Printf(TEXT("Function name contains invalid character: '%c'"), C);
			return false;
		}
	}

	return true;
}

// ===== Type Conversion =====

bool FBlueprintEditor::ParsePinType(
	const FString& TypeString,
	FEdGraphPinType& OutPinType,
	FString& OutError)
{
	OutPinType.ResetToDefaults();
	FString CleanType = TypeString.TrimStartAndEnd();

	// Handle container types
	if (ParseContainerType(CleanType, OutPinType, OutError))
	{
		return OutError.IsEmpty();
	}

	// Primitive types
	if (CleanType == TEXT("bool") || CleanType == TEXT("Boolean"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		return true;
	}
	if (CleanType == TEXT("int") || CleanType == TEXT("int32") || CleanType == TEXT("Integer"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		return true;
	}
	if (CleanType == TEXT("int64"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		return true;
	}
	if (CleanType == TEXT("float") || CleanType == TEXT("Float"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		return true;
	}
	if (CleanType == TEXT("double") || CleanType == TEXT("Double"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		return true;
	}
	if (CleanType == TEXT("byte") || CleanType == TEXT("uint8") || CleanType == TEXT("Byte"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		return true;
	}
	if (CleanType == TEXT("FString") || CleanType == TEXT("String"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
		return true;
	}
	if (CleanType == TEXT("FName") || CleanType == TEXT("Name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		return true;
	}
	if (CleanType == TEXT("FText") || CleanType == TEXT("Text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		return true;
	}

	// Struct and object types
	if (ParseStructType(CleanType, OutPinType, OutError))
	{
		return true;
	}

	// Object references (with * suffix)
	if (CleanType.EndsWith(TEXT("*")))
	{
		FString ClassName = CleanType.LeftChop(1).TrimEnd();
		UClass* Class = FindObject<UClass>(nullptr, *ClassName);

		if (!Class)
		{
			Class = LoadClass<UObject>(nullptr,
				*FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
		}
		if (!Class)
		{
			Class = LoadClass<UObject>(nullptr,
				*FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassName));
		}

		if (Class)
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			OutPinType.PinSubCategoryObject = Class;
			return true;
		}

		OutError = FString::Printf(TEXT("Unknown class: %s"), *ClassName);
		return false;
	}

	OutError = FString::Printf(TEXT("Unknown type: %s"), *TypeString);
	return false;
}

bool FBlueprintEditor::ParseContainerType(
	const FString& TypeString,
	FEdGraphPinType& OutPinType,
	FString& OutError)
{
	// TArray<T>
	if (TypeString.StartsWith(TEXT("TArray<")) && TypeString.EndsWith(TEXT(">")))
	{
		FString InnerType = TypeString.Mid(7, TypeString.Len() - 8);
		FEdGraphPinType InnerPinType;
		if (!ParsePinType(InnerType, InnerPinType, OutError))
		{
			return true; // Error set
		}
		OutPinType = InnerPinType;
		OutPinType.ContainerType = EPinContainerType::Array;
		return true;
	}

	// TSet<T>
	if (TypeString.StartsWith(TEXT("TSet<")) && TypeString.EndsWith(TEXT(">")))
	{
		FString InnerType = TypeString.Mid(5, TypeString.Len() - 6);
		FEdGraphPinType InnerPinType;
		if (!ParsePinType(InnerType, InnerPinType, OutError))
		{
			return true; // Error set
		}
		OutPinType = InnerPinType;
		OutPinType.ContainerType = EPinContainerType::Set;
		return true;
	}

	return false; // Not a container type
}

bool FBlueprintEditor::ParseStructType(
	const FString& TypeName,
	FEdGraphPinType& OutPinType,
	FString& OutError)
{
	// Common struct types with TBaseStructure
	if (TypeName == TEXT("FVector") || TypeName == TEXT("Vector"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
		return true;
	}
	if (TypeName == TEXT("FRotator") || TypeName == TEXT("Rotator"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
		return true;
	}
	if (TypeName == TEXT("FTransform") || TypeName == TEXT("Transform"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
		return true;
	}
	if (TypeName == TEXT("FLinearColor") || TypeName == TEXT("LinearColor"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
		return true;
	}
	if (TypeName == TEXT("FColor") || TypeName == TEXT("Color"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FColor>::Get();
		return true;
	}
	if (TypeName == TEXT("FVector2D") || TypeName == TEXT("Vector2D"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FVector2D>::Get();
		return true;
	}

	// Try finding by name
	UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *TypeName);
	if (Struct)
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = Struct;
		return true;
	}

	return false;
}

FString FBlueprintEditor::PinTypeToString(const FEdGraphPinType& PinType)
{
	// Container prefix/suffix
	FString Prefix, Suffix;
	if (PinType.ContainerType == EPinContainerType::Array)
	{
		Prefix = TEXT("TArray<");
		Suffix = TEXT(">");
	}
	else if (PinType.ContainerType == EPinContainerType::Set)
	{
		Prefix = TEXT("TSet<");
		Suffix = TEXT(">");
	}

	// Base type name
	FString TypeName;

	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		TypeName = TEXT("bool");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		TypeName = TEXT("int32");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		TypeName = TEXT("int64");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		TypeName = (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
			? TEXT("double") : TEXT("float");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		TypeName = TEXT("byte");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		TypeName = TEXT("FString");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		TypeName = TEXT("FName");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		TypeName = TEXT("FText");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (UScriptStruct* Struct = Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get()))
		{
			TypeName = Struct->GetName();
		}
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object ||
	         PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
	{
		if (UClass* Class = Cast<UClass>(PinType.PinSubCategoryObject.Get()))
		{
			TypeName = Class->GetName() + TEXT("*");
		}
	}
	else
	{
		TypeName = PinType.PinCategory.ToString();
	}

	return Prefix + TypeName + Suffix;
}
