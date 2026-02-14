// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"

class UEdGraph;
class UEdGraphNode;

/**
 * Configuration for pin search with fallbacks
 */
struct FPinSearchConfig
{
	/** Preferred pin names to try in order */
	TArray<FName> PreferredNames;

	/** Pin direction to match */
	EEdGraphPinDirection Direction = EGPD_MAX;

	/** Optional: Pin category to match as fallback (e.g., PC_Boolean, PC_Struct) */
	FName FallbackCategory = NAME_None;

	/** Optional: Substring to search in pin names as fallback */
	FString FallbackNameContains;

	/** If true, accept any pin matching direction when all else fails */
	bool bAcceptAnyAsLastResort = false;

	// Convenience constructors
	static FPinSearchConfig Output(std::initializer_list<FName> Names)
	{
		FPinSearchConfig Config;
		Config.PreferredNames = Names;
		Config.Direction = EGPD_Output;
		return Config;
	}

	static FPinSearchConfig Input(std::initializer_list<FName> Names)
	{
		FPinSearchConfig Config;
		Config.PreferredNames = Names;
		Config.Direction = EGPD_Input;
		return Config;
	}

	FPinSearchConfig& WithCategory(FName Category)
	{
		FallbackCategory = Category;
		return *this;
	}

	FPinSearchConfig& WithNameContains(const FString& Substring)
	{
		FallbackNameContains = Substring;
		return *this;
	}

	FPinSearchConfig& AcceptAny()
	{
		bAcceptAnyAsLastResort = true;
		return *this;
	}
};

/**
 * AnimNodePinUtils - Pin finding and connection utilities
 *
 * Responsibilities:
 * - Finding pins by name with fallbacks
 * - Managing node connections
 * - Pin validation and type checking
 */
class FAnimNodePinUtils
{
public:
	/**
	 * Find pin on node by name
	 * @param Node Node to search
	 * @param PinName Name of the pin
	 * @param Direction Pin direction filter (EGPD_MAX for any)
	 * @return Found pin or nullptr
	 */
	static UEdGraphPin* FindPinByName(
		UEdGraphNode* Node,
		const FString& PinName,
		EEdGraphPinDirection Direction = EGPD_MAX
	);

	/**
	 * Find pin using configuration with multiple fallback strategies
	 *
	 * Search order:
	 * 1. Try each preferred name in order
	 * 2. If FallbackCategory set, find first pin matching category + direction
	 * 3. If FallbackNameContains set, find first pin with name containing substring
	 * 4. If bAcceptAnyAsLastResort, find first pin matching direction
	 *
	 * @param Node Node to search
	 * @param Config Search configuration
	 * @param OutError Optional: If provided and no pin found, populated with available pins list
	 * @return Found pin or nullptr
	 */
	static UEdGraphPin* FindPinWithFallbacks(
		UEdGraphNode* Node,
		const FPinSearchConfig& Config,
		FString* OutError = nullptr
	);

	/**
	 * Build error message listing available pins on a node
	 * @param Node Node to inspect
	 * @param Direction Filter by direction
	 * @param Context Context string for error message
	 * @return Error message with available pins
	 */
	static FString BuildAvailablePinsError(
		UEdGraphNode* Node,
		EEdGraphPinDirection Direction,
		const FString& Context
	);

	/**
	 * Find result/output node in graph
	 * @param Graph Graph to search
	 * @return Result node or nullptr
	 */
	static UEdGraphNode* FindResultNode(UEdGraph* Graph);

	/**
	 * Validate if a value is compatible with a pin type
	 * @param Pin Pin to validate against
	 * @param Value Value to validate
	 * @param OutError Error message if invalid
	 * @return True if value is compatible
	 */
	static bool ValidatePinValueType(
		UEdGraphPin* Pin,
		const FString& Value,
		FString& OutError
	);

	/**
	 * Set pin default value with type validation
	 * @param Graph Graph containing the node
	 * @param NodeId Node identifier
	 * @param PinName Pin name
	 * @param Value Value to set
	 * @param OutError Error message if failed
	 * @return True if successful
	 */
	static bool SetPinDefaultValueWithValidation(
		UEdGraph* Graph,
		const FString& NodeId,
		const FString& PinName,
		const FString& Value,
		FString& OutError
	);
};
