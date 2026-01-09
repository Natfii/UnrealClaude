// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Utility class for validating MCP tool parameters
 * Provides common validation functions to prevent injection attacks and invalid input
 */
class UNREALCLAUDE_API FMCPParamValidator
{
public:
	/**
	 * Validate a string for use as an actor name or label
	 * Rejects shell metacharacters and control characters
	 * @param Name The actor name to validate
	 * @param OutError Error message if validation fails
	 * @return true if valid, false otherwise
	 */
	static bool ValidateActorName(const FString& Name, FString& OutError);

	/**
	 * Validate a property path (e.g., "Component.Property")
	 * Only allows alphanumeric characters, underscores, and dots
	 * @param PropertyPath The property path to validate
	 * @param OutError Error message if validation fails
	 * @return true if valid, false otherwise
	 */
	static bool ValidatePropertyPath(const FString& PropertyPath, FString& OutError);

	/**
	 * Validate a class path for loading UClass assets
	 * @param ClassPath The class path to validate
	 * @param OutError Error message if validation fails
	 * @return true if valid, false otherwise
	 */
	static bool ValidateClassPath(const FString& ClassPath, FString& OutError);

	/**
	 * Validate a console command for safety
	 * Checks against blocklist of dangerous commands
	 * @param Command The console command to validate
	 * @param OutError Error message if validation fails
	 * @return true if valid, false otherwise
	 */
	static bool ValidateConsoleCommand(const FString& Command, FString& OutError);

	/**
	 * Validate numeric value is within reasonable bounds
	 * Rejects NaN, Infinity, and extremely large values
	 * @param Value The numeric value to validate
	 * @param FieldName Name of the field for error message
	 * @param OutError Error message if validation fails
	 * @param MaxAbsValue Maximum absolute value allowed (default: 1e10)
	 * @return true if valid, false otherwise
	 */
	static bool ValidateNumericValue(double Value, const FString& FieldName, FString& OutError, double MaxAbsValue = 1e10);

	/**
	 * Validate string length is within limits
	 * @param Value The string to validate
	 * @param FieldName Name of the field for error message
	 * @param MaxLength Maximum allowed length
	 * @param OutError Error message if validation fails
	 * @return true if valid, false otherwise
	 */
	static bool ValidateStringLength(const FString& Value, const FString& FieldName, int32 MaxLength, FString& OutError);

	/**
	 * Sanitize a string by removing dangerous characters
	 * @param Input The string to sanitize
	 * @return Sanitized string
	 */
	static FString SanitizeString(const FString& Input);

	/**
	 * Validate a Blueprint asset path
	 * Rejects engine Blueprints and invalid paths
	 * @param BlueprintPath The Blueprint path to validate
	 * @param OutError Error message if validation fails
	 * @return true if valid, false otherwise
	 */
	static bool ValidateBlueprintPath(const FString& BlueprintPath, FString& OutError);

	/**
	 * Validate a Blueprint variable name
	 * Must start with letter/underscore, contain only alphanumeric/underscore
	 * @param VariableName The variable name to validate
	 * @param OutError Error message if validation fails
	 * @return true if valid, false otherwise
	 */
	static bool ValidateBlueprintVariableName(const FString& VariableName, FString& OutError);

	/**
	 * Validate a Blueprint function name
	 * Same rules as variable names
	 * @param FunctionName The function name to validate
	 * @param OutError Error message if validation fails
	 * @return true if valid, false otherwise
	 */
	static bool ValidateBlueprintFunctionName(const FString& FunctionName, FString& OutError);

private:
	/** Console commands that are blocked for safety */
	static const TArray<FString>& GetBlockedConsoleCommands();
};
