// Copyright Natali Caggiano. All Rights Reserved.

#include "AnimNodePinUtils.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_Root.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"

// Forward declaration for node ID functions
class FAnimGraphEditor;

UEdGraphPin* FAnimNodePinUtils::FindPinByName(
	UEdGraphNode* Node,
	const FString& PinName,
	EEdGraphPinDirection Direction)
{
	if (!Node) return nullptr;

	for (UEdGraphPin* Pin : Node->Pins)
	{
		bool bDirectionMatch = (Direction == EGPD_MAX) || (Pin->Direction == Direction);
		if (bDirectionMatch && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			return Pin;
		}
	}

	return nullptr;
}

UEdGraphPin* FAnimNodePinUtils::FindPinWithFallbacks(
	UEdGraphNode* Node,
	const FPinSearchConfig& Config,
	FString* OutError)
{
	if (!Node)
	{
		if (OutError)
		{
			*OutError = TEXT("Invalid node");
		}
		return nullptr;
	}

	// Strategy 1: Try preferred names in order
	for (const FName& PinName : Config.PreferredNames)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			bool bDirectionMatch = (Config.Direction == EGPD_MAX) || (Pin->Direction == Config.Direction);
			if (bDirectionMatch && Pin->PinName == PinName)
			{
				return Pin;
			}
		}
	}

	// Strategy 2: Match by pin category (e.g., PC_Boolean, PC_Struct)
	if (!Config.FallbackCategory.IsNone())
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			bool bDirectionMatch = (Config.Direction == EGPD_MAX) || (Pin->Direction == Config.Direction);
			if (bDirectionMatch && Pin->PinType.PinCategory == Config.FallbackCategory)
			{
				return Pin;
			}
		}
	}

	// Strategy 3: Match by name substring
	if (!Config.FallbackNameContains.IsEmpty())
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			bool bDirectionMatch = (Config.Direction == EGPD_MAX) || (Pin->Direction == Config.Direction);
			if (bDirectionMatch && Pin->PinName.ToString().Contains(Config.FallbackNameContains, ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
	}

	// Strategy 4: Accept any pin matching direction
	if (Config.bAcceptAnyAsLastResort)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Config.Direction == EGPD_MAX || Pin->Direction == Config.Direction)
			{
				// Skip exec pins as they're typically not what we want
				if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					return Pin;
				}
			}
		}
		// If only exec pins exist, try them as last resort
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Config.Direction == EGPD_MAX || Pin->Direction == Config.Direction)
			{
				return Pin;
			}
		}
	}

	// No pin found - build error message if requested
	if (OutError)
	{
		*OutError = BuildAvailablePinsError(Node, Config.Direction, TEXT("pin"));
	}

	return nullptr;
}

FString FAnimNodePinUtils::BuildAvailablePinsError(
	UEdGraphNode* Node,
	EEdGraphPinDirection Direction,
	const FString& Context)
{
	if (!Node)
	{
		return FString::Printf(TEXT("Cannot find %s: invalid node"), *Context);
	}

	FString AvailablePins;
	FString DirectionStr = (Direction == EGPD_Input) ? TEXT("input") :
	                       (Direction == EGPD_Output) ? TEXT("output") : TEXT("");

	for (UEdGraphPin* Pin : Node->Pins)
	{
		bool bDirectionMatch = (Direction == EGPD_MAX) || (Pin->Direction == Direction);
		if (bDirectionMatch)
		{
			AvailablePins += FString::Printf(TEXT("[%s] "), *Pin->PinName.ToString());
		}
	}

	if (AvailablePins.IsEmpty())
	{
		return FString::Printf(TEXT("Cannot find %s %s. No %s pins available on node."),
			*DirectionStr, *Context, *DirectionStr);
	}

	return FString::Printf(TEXT("Cannot find %s %s. Available %s pins: %s"),
		*DirectionStr, *Context, *DirectionStr, *AvailablePins);
}

UEdGraphNode* FAnimNodePinUtils::FindResultNode(UEdGraph* Graph)
{
	if (!Graph) return nullptr;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		// Check for all types of result/root nodes
		if (Node->IsA<UAnimGraphNode_StateResult>() ||
			Node->IsA<UAnimGraphNode_TransitionResult>() ||
			Node->IsA<UAnimGraphNode_Root>())
		{
			return Node;
		}
	}

	return nullptr;
}

bool FAnimNodePinUtils::ValidatePinValueType(
	UEdGraphPin* Pin,
	const FString& Value,
	FString& OutError)
{
	if (!Pin)
	{
		OutError = TEXT("Invalid pin");
		return false;
	}

	// Validate based on pin type
	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		FString LowerValue = Value.ToLower();
		if (LowerValue != TEXT("true") && LowerValue != TEXT("false") &&
			LowerValue != TEXT("1") && LowerValue != TEXT("0"))
		{
			OutError = FString::Printf(TEXT("Pin '%s' expects bool value (true/false), got: %s"),
				*Pin->PinName.ToString(), *Value);
			return false;
		}
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int ||
			 Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		if (!Value.IsNumeric())
		{
			OutError = FString::Printf(TEXT("Pin '%s' expects integer value, got: %s"),
				*Pin->PinName.ToString(), *Value);
			return false;
		}
		// Check for decimal point (integers shouldn't have them)
		if (Value.Contains(TEXT(".")))
		{
			OutError = FString::Printf(TEXT("Pin '%s' expects integer value (no decimals), got: %s"),
				*Pin->PinName.ToString(), *Value);
			return false;
		}
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (!FCString::Atod(*Value) && Value != TEXT("0") && Value != TEXT("0.0"))
		{
			// Additional validation for numeric strings
			bool bIsValid = true;
			bool bFoundDot = false;
			for (TCHAR C : Value)
			{
				if (C == '.' && !bFoundDot)
				{
					bFoundDot = true;
				}
				else if (C == '-' && Value[0] == '-')
				{
					continue;
				}
				else if (!FChar::IsDigit(C))
				{
					bIsValid = false;
					break;
				}
			}
			if (!bIsValid)
			{
				OutError = FString::Printf(TEXT("Pin '%s' expects float/double value, got: %s"),
					*Pin->PinName.ToString(), *Value);
				return false;
			}
		}
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		OutError = FString::Printf(TEXT("Pin '%s' is an exec pin, cannot set default value"),
			*Pin->PinName.ToString());
		return false;
	}
	// For strings, names, text - any value is valid
	// For structs and objects - we allow JSON-style values but don't deeply validate

	return true;
}

bool FAnimNodePinUtils::SetPinDefaultValueWithValidation(
	UEdGraph* Graph,
	const FString& NodeId,
	const FString& PinName,
	const FString& Value,
	FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("Invalid graph");
		return false;
	}

	// Find node - need to use FAnimGraphEditor::FindNodeById
	// This is implemented via delegate to avoid circular dependency
	UEdGraphNode* Node = nullptr;
	for (UEdGraphNode* GraphNode : Graph->Nodes)
	{
		if (GraphNode && GraphNode->NodeComment.Contains(NodeId))
		{
			Node = GraphNode;
			break;
		}
	}

	if (!Node)
	{
		OutError = FString::Printf(TEXT("Node not found: %s"), *NodeId);
		return false;
	}

	// Find pin
	UEdGraphPin* Pin = FindPinByName(Node, PinName, EGPD_Input);
	if (!Pin)
	{
		// List available input pins
		FString AvailablePins;
		for (UEdGraphPin* P : Node->Pins)
		{
			if (P->Direction == EGPD_Input)
			{
				AvailablePins += FString::Printf(TEXT("[%s] "), *P->PinName.ToString());
			}
		}
		OutError = FString::Printf(TEXT("Input pin '%s' not found on node %s. Available: %s"),
			*PinName, *NodeId, AvailablePins.IsEmpty() ? TEXT("none") : *AvailablePins);
		return false;
	}

	// Validate value type
	if (!ValidatePinValueType(Pin, Value, OutError))
	{
		return false;
	}

	// Set the default value
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema)
	{
		Schema->TrySetDefaultValue(*Pin, Value);
	}
	else
	{
		Pin->DefaultValue = Value;
	}

	Graph->Modify();

	return true;
}
