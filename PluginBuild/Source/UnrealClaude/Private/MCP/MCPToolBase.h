// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCPToolRegistry.h"
#include "MCPParamValidator.h"

// Forward declarations
class UWorld;

/**
 * Base class for MCP tools that provides common functionality
 * Reduces code duplication across tool implementations
 */
class FMCPToolBase : public IMCPTool
{
public:
	virtual ~FMCPToolBase() = default;

protected:
	/**
	 * Validate that the editor context is available
	 * @param OutWorld - Output world pointer if validation succeeds
	 * @return Error result if validation fails, or empty optional if success
	 */
	TOptional<FMCPToolResult> ValidateEditorContext(UWorld*& OutWorld) const;

	/**
	 * Find an actor by name or label in the given world
	 * @param World - The world to search in
	 * @param NameOrLabel - The actor name or label to search for
	 * @return The found actor, or nullptr if not found
	 */
	AActor* FindActorByNameOrLabel(UWorld* World, const FString& NameOrLabel) const;

	/**
	 * Mark the world as dirty after modifications
	 * @param World - The world to mark dirty
	 */
	void MarkWorldDirty(UWorld* World) const;

	/**
	 * Mark an actor and its world as dirty after modifications
	 * @param Actor - The actor to mark dirty
	 */
	void MarkActorDirty(AActor* Actor) const;

	// ===== Parameter Extraction Helpers =====

	/**
	 * Extract and validate a required string parameter
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name to extract
	 * @param OutValue - Output value
	 * @param OutError - Error result if extraction/validation fails
	 * @return true if extraction succeeded
	 */
	bool ExtractRequiredString(const TSharedRef<FJsonObject>& Params, const FString& ParamName,
		FString& OutValue, TOptional<FMCPToolResult>& OutError) const
	{
		if (!Params->TryGetStringField(ParamName, OutValue) || OutValue.IsEmpty())
		{
			OutError = FMCPToolResult::Error(FString::Printf(TEXT("Missing required parameter: %s"), *ParamName));
			return false;
		}
		return true;
	}

	/**
	 * Extract and validate a required actor name parameter
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name to extract
	 * @param OutValue - Output value
	 * @param OutError - Error result if extraction/validation fails
	 * @return true if extraction and validation succeeded
	 */
	bool ExtractActorName(const TSharedRef<FJsonObject>& Params, const FString& ParamName,
		FString& OutValue, TOptional<FMCPToolResult>& OutError) const
	{
		if (!ExtractRequiredString(Params, ParamName, OutValue, OutError))
		{
			return false;
		}
		FString ValidationError;
		if (!FMCPParamValidator::ValidateActorName(OutValue, ValidationError))
		{
			OutError = FMCPToolResult::Error(ValidationError);
			return false;
		}
		return true;
	}

	/**
	 * Extract an optional string parameter with default value
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name to extract
	 * @param DefaultValue - Default value if not present
	 * @return Extracted value or default
	 */
	FString ExtractOptionalString(const TSharedRef<FJsonObject>& Params, const FString& ParamName,
		const FString& DefaultValue = FString()) const
	{
		FString Value;
		if (Params->TryGetStringField(ParamName, Value))
		{
			return Value;
		}
		return DefaultValue;
	}

	/**
	 * Extract an optional number parameter with default value
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name to extract
	 * @param DefaultValue - Default value if not present
	 * @return Extracted value or default
	 */
	template<typename T>
	T ExtractOptionalNumber(const TSharedRef<FJsonObject>& Params, const FString& ParamName, T DefaultValue) const
	{
		double Value;
		if (Params->TryGetNumberField(ParamName, Value))
		{
			return static_cast<T>(Value);
		}
		return DefaultValue;
	}

	/**
	 * Extract an optional boolean parameter with default value
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name to extract
	 * @param DefaultValue - Default value if not present
	 * @return Extracted value or default
	 */
	bool ExtractOptionalBool(const TSharedRef<FJsonObject>& Params, const FString& ParamName, bool DefaultValue = false) const
	{
		bool Value;
		if (Params->TryGetBoolField(ParamName, Value))
		{
			return Value;
		}
		return DefaultValue;
	}
};
