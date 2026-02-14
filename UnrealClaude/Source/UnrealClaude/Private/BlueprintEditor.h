// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"

/**
 * Blueprint variable and function management
 *
 * Responsibilities:
 * - Adding/removing member variables
 * - Adding/removing functions
 * - Type parsing and conversion
 * - Name validation
 *
 * Supported Types:
 * - Primitives: bool, int32, int64, float, double, byte, FString, FName, FText
 * - Structs: FVector, FRotator, FTransform, FLinearColor, FVector2D
 * - Containers: TArray<T>, TSet<T>
 * - Object references: AActor*, UTexture2D*, etc.
 */
class FBlueprintEditor
{
public:
	// ===== Variable Management =====

	/**
	 * Add member variable to Blueprint
	 * @param Blueprint - Blueprint to modify
	 * @param VariableName - Name of variable (must be valid identifier)
	 * @param PinType - Variable type
	 * @param OutError - Error message if failed
	 * @return true if successful
	 */
	static bool AddVariable(
		UBlueprint* Blueprint,
		const FString& VariableName,
		const FEdGraphPinType& PinType,
		FString& OutError
	);

	/**
	 * Remove variable from Blueprint
	 * @param Blueprint - Blueprint to modify
	 * @param VariableName - Name of variable to remove
	 * @param OutError - Error message if failed
	 * @return true if successful
	 */
	static bool RemoveVariable(
		UBlueprint* Blueprint,
		const FString& VariableName,
		FString& OutError
	);

	// ===== Function Management =====

	/**
	 * Add empty function to Blueprint
	 * @param Blueprint - Blueprint to modify
	 * @param FunctionName - Name of function (must be valid identifier)
	 * @param OutError - Error message if failed
	 * @return true if successful
	 */
	static bool AddFunction(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		FString& OutError
	);

	/**
	 * Remove function from Blueprint
	 * @param Blueprint - Blueprint to modify
	 * @param FunctionName - Name of function to remove
	 * @param OutError - Error message if failed
	 * @return true if successful
	 */
	static bool RemoveFunction(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		FString& OutError
	);

	// ===== Type Conversion =====

	/**
	 * Parse type string to FEdGraphPinType
	 *
	 * Supported formats:
	 * - Primitives: "bool", "int32", "float", "FString"
	 * - Structs: "FVector", "FRotator", "FTransform"
	 * - Arrays: "TArray<int32>", "TArray<FVector>"
	 * - Objects: "AActor*", "UTexture2D*"
	 *
	 * @param TypeString - Type name string
	 * @param OutPinType - Output pin type
	 * @param OutError - Error message if parsing fails
	 * @return true if successful
	 */
	static bool ParsePinType(
		const FString& TypeString,
		FEdGraphPinType& OutPinType,
		FString& OutError
	);

	/**
	 * Convert FEdGraphPinType to string representation
	 * @param PinType - Pin type to convert
	 * @return Type string
	 */
	static FString PinTypeToString(const FEdGraphPinType& PinType);

	// ===== Name Validation =====

	/**
	 * Validate variable name follows Blueprint naming conventions
	 * Rules: max 128 chars, starts with letter/underscore, alphanumeric + underscore only
	 * @param VariableName - Name to validate
	 * @param OutError - Error message if invalid
	 * @return true if valid
	 */
	static bool ValidateVariableName(const FString& VariableName, FString& OutError);

	/**
	 * Validate function name follows Blueprint naming conventions
	 * (Same rules as variable names)
	 * @param FunctionName - Name to validate
	 * @param OutError - Error message if invalid
	 * @return true if valid
	 */
	static bool ValidateFunctionName(const FString& FunctionName, FString& OutError);

private:
	// Constants
	static constexpr int32 MaxNameLength = 128;

	// Helper for parsing container types
	static bool ParseContainerType(
		const FString& TypeString,
		FEdGraphPinType& OutPinType,
		FString& OutError
	);

	// Helper for parsing struct types
	static bool ParseStructType(
		const FString& TypeName,
		FEdGraphPinType& OutPinType,
		FString& OutError
	);
};
