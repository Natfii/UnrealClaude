// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCPToolRegistry.h"
#include "MCPParamValidator.h"
#include "UnrealClaudeUtils.h"

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

	// ===== Transform Extraction Helpers =====
	// These consolidate vector/rotator/scale extraction from JSON parameters
	// to eliminate duplicate code across MCP tools

	/**
	 * Extract a FVector from a nested JSON object parameter
	 * Expects JSON format: { "param_name": { "x": 0, "y": 0, "z": 0 } }
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name containing the vector object
	 * @param DefaultValue - Default value if not present or invalid
	 * @return Extracted vector or default
	 */
	FVector ExtractVectorParam(const TSharedRef<FJsonObject>& Params, const FString& ParamName,
		const FVector& DefaultValue = FVector::ZeroVector) const
	{
		const TSharedPtr<FJsonObject>* VectorObj;
		if (Params->TryGetObjectField(ParamName, VectorObj) && VectorObj && (*VectorObj).IsValid())
		{
			return UnrealClaudeJsonUtils::ExtractVector(*VectorObj, DefaultValue);
		}
		return DefaultValue;
	}

	/**
	 * Extract a FRotator from a nested JSON object parameter
	 * Expects JSON format: { "param_name": { "pitch": 0, "yaw": 0, "roll": 0 } }
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name containing the rotator object
	 * @param DefaultValue - Default value if not present or invalid
	 * @return Extracted rotator or default
	 */
	FRotator ExtractRotatorParam(const TSharedRef<FJsonObject>& Params, const FString& ParamName,
		const FRotator& DefaultValue = FRotator::ZeroRotator) const
	{
		const TSharedPtr<FJsonObject>* RotatorObj;
		if (Params->TryGetObjectField(ParamName, RotatorObj) && RotatorObj && (*RotatorObj).IsValid())
		{
			return UnrealClaudeJsonUtils::ExtractRotator(*RotatorObj, DefaultValue);
		}
		return DefaultValue;
	}

	/**
	 * Extract a scale FVector from a nested JSON object parameter
	 * Expects JSON format: { "param_name": { "x": 1, "y": 1, "z": 1 } }
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name containing the scale object
	 * @param DefaultValue - Default value if not present (typically 1,1,1)
	 * @return Extracted scale or default
	 */
	FVector ExtractScaleParam(const TSharedRef<FJsonObject>& Params, const FString& ParamName,
		const FVector& DefaultValue = FVector::OneVector) const
	{
		return ExtractVectorParam(Params, ParamName, DefaultValue);
	}

	/**
	 * Check if a vector parameter exists in the JSON
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name to check
	 * @return true if the parameter exists as an object
	 */
	bool HasVectorParam(const TSharedRef<FJsonObject>& Params, const FString& ParamName) const
	{
		const TSharedPtr<FJsonObject>* Obj;
		return Params->TryGetObjectField(ParamName, Obj) && Obj && (*Obj).IsValid();
	}

	/**
	 * Extract vector components individually with delta support
	 * Useful for move operations where only some components may be specified
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name containing the vector object
	 * @param OutVector - Output vector (modified in place)
	 * @param bAdditive - If true, add values to OutVector; if false, replace
	 * @return true if any component was extracted
	 */
	bool ExtractVectorComponents(const TSharedRef<FJsonObject>& Params, const FString& ParamName,
		FVector& OutVector, bool bAdditive = false) const
	{
		const TSharedPtr<FJsonObject>* VectorObj;
		if (!Params->TryGetObjectField(ParamName, VectorObj) || !VectorObj || !(*VectorObj).IsValid())
		{
			return false;
		}

		bool bAnyExtracted = false;
		double Val;

		if ((*VectorObj)->TryGetNumberField(TEXT("x"), Val))
		{
			OutVector.X = bAdditive ? OutVector.X + Val : Val;
			bAnyExtracted = true;
		}
		if ((*VectorObj)->TryGetNumberField(TEXT("y"), Val))
		{
			OutVector.Y = bAdditive ? OutVector.Y + Val : Val;
			bAnyExtracted = true;
		}
		if ((*VectorObj)->TryGetNumberField(TEXT("z"), Val))
		{
			OutVector.Z = bAdditive ? OutVector.Z + Val : Val;
			bAnyExtracted = true;
		}

		return bAnyExtracted;
	}

	/**
	 * Extract rotator components individually with delta support
	 * @param Params - The JSON parameters
	 * @param ParamName - The parameter name containing the rotator object
	 * @param OutRotator - Output rotator (modified in place)
	 * @param bAdditive - If true, add values to OutRotator; if false, replace
	 * @return true if any component was extracted
	 */
	bool ExtractRotatorComponents(const TSharedRef<FJsonObject>& Params, const FString& ParamName,
		FRotator& OutRotator, bool bAdditive = false) const
	{
		const TSharedPtr<FJsonObject>* RotatorObj;
		if (!Params->TryGetObjectField(ParamName, RotatorObj) || !RotatorObj || !(*RotatorObj).IsValid())
		{
			return false;
		}

		bool bAnyExtracted = false;
		double Val;

		if ((*RotatorObj)->TryGetNumberField(TEXT("pitch"), Val))
		{
			OutRotator.Pitch = bAdditive ? OutRotator.Pitch + Val : Val;
			bAnyExtracted = true;
		}
		if ((*RotatorObj)->TryGetNumberField(TEXT("yaw"), Val))
		{
			OutRotator.Yaw = bAdditive ? OutRotator.Yaw + Val : Val;
			bAnyExtracted = true;
		}
		if ((*RotatorObj)->TryGetNumberField(TEXT("roll"), Val))
		{
			OutRotator.Roll = bAdditive ? OutRotator.Roll + Val : Val;
			bAnyExtracted = true;
		}

		return bAnyExtracted;
	}

	// ===== Validation Helpers =====
	// Consolidate common validation patterns to reduce boilerplate

	/**
	 * Validate a string parameter and return error if invalid
	 * @param Value - The value to validate
	 * @param ValidatorFunc - Validation function that returns bool and sets error string
	 * @param OutError - Optional error result if validation fails
	 * @return true if validation passed
	 */
	template<typename ValidatorT>
	bool ValidateParam(const FString& Value, ValidatorT ValidatorFunc, TOptional<FMCPToolResult>& OutError) const
	{
		FString ValidationError;
		if (!ValidatorFunc(Value, ValidationError))
		{
			OutError = FMCPToolResult::Error(ValidationError);
			return false;
		}
		return true;
	}

	/**
	 * Validate actor name parameter using centralized validator
	 * @param ActorName - Actor name to validate
	 * @param OutError - Optional error result if validation fails
	 * @return true if valid
	 */
	bool ValidateActorNameParam(const FString& ActorName, TOptional<FMCPToolResult>& OutError) const
	{
		return ValidateParam(ActorName, FMCPParamValidator::ValidateActorName, OutError);
	}

	/**
	 * Validate console command parameter using centralized validator
	 * @param Command - Command to validate
	 * @param OutError - Optional error result if validation fails
	 * @return true if valid
	 */
	bool ValidateConsoleCommandParam(const FString& Command, TOptional<FMCPToolResult>& OutError) const
	{
		return ValidateParam(Command, FMCPParamValidator::ValidateConsoleCommand, OutError);
	}

	/**
	 * Validate property path parameter using centralized validator
	 * @param PropertyPath - Property path to validate
	 * @param OutError - Optional error result if validation fails
	 * @return true if valid
	 */
	bool ValidatePropertyPathParam(const FString& PropertyPath, TOptional<FMCPToolResult>& OutError) const
	{
		return ValidateParam(PropertyPath, FMCPParamValidator::ValidatePropertyPath, OutError);
	}

	/**
	 * Validate Blueprint path parameter using centralized validator
	 * @param BlueprintPath - Path to validate
	 * @param OutError - Optional error result if validation fails
	 * @return true if valid
	 */
	bool ValidateBlueprintPathParam(const FString& BlueprintPath, TOptional<FMCPToolResult>& OutError) const
	{
		return ValidateParam(BlueprintPath, FMCPParamValidator::ValidateBlueprintPath, OutError);
	}

	// ===== Class Loading Helpers =====

	/**
	 * Load an actor class by path with fallback prefixes
	 * Tries: full path, /Script/Engine., /Script/CoreUObject., FindObject
	 * @param ClassPath - Class path or short name
	 * @param OutError - Optional error result if class not found
	 * @return Actor class or nullptr
	 */
	UClass* LoadActorClass(const FString& ClassPath, TOptional<FMCPToolResult>& OutError) const
	{
		UClass* ActorClass = LoadClass<AActor>(nullptr, *ClassPath);

		if (!ActorClass)
		{
			ActorClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassPath));
		}

		if (!ActorClass)
		{
			ActorClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassPath));
		}

		if (!ActorClass)
		{
			ActorClass = FindObject<UClass>(nullptr, *ClassPath);
		}

		if (!ActorClass)
		{
			OutError = FMCPToolResult::Error(FString::Printf(TEXT("Could not find actor class: %s"), *ClassPath));
		}

		return ActorClass;
	}

	// ===== Actor Result Helpers =====

	/**
	 * Create error result for actor not found
	 * @param ActorName - Name of actor that wasn't found
	 * @return Error result
	 */
	FMCPToolResult ActorNotFoundError(const FString& ActorName) const
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	/**
	 * Build JSON object with actor basic info
	 * @param Actor - Actor to serialize
	 * @return JSON object with name, label, class fields
	 */
	TSharedPtr<FJsonObject> BuildActorInfoJson(AActor* Actor) const
	{
		TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
		if (Actor)
		{
			ActorJson->SetStringField(TEXT("name"), Actor->GetName());
			ActorJson->SetStringField(TEXT("label"), Actor->GetActorLabel());
			ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		}
		return ActorJson;
	}

	/**
	 * Build JSON object with actor info including transform
	 * @param Actor - Actor to serialize
	 * @return JSON object with name, label, class, location, rotation, scale
	 */
	TSharedPtr<FJsonObject> BuildActorInfoWithTransformJson(AActor* Actor) const
	{
		TSharedPtr<FJsonObject> ActorJson = BuildActorInfoJson(Actor);
		if (Actor)
		{
			ActorJson->SetObjectField(TEXT("location"), UnrealClaudeJsonUtils::VectorToJson(Actor->GetActorLocation()));
			ActorJson->SetObjectField(TEXT("rotation"), UnrealClaudeJsonUtils::RotatorToJson(Actor->GetActorRotation()));
			ActorJson->SetObjectField(TEXT("scale"), UnrealClaudeJsonUtils::VectorToJson(Actor->GetActorScale3D()));
		}
		return ActorJson;
	}

	/**
	 * Build JSON array from string array
	 * @param Strings - Array of strings to convert
	 * @return JSON array of string values
	 */
	TArray<TSharedPtr<FJsonValue>> StringArrayToJsonArray(const TArray<FString>& Strings) const
	{
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		for (const FString& Str : Strings)
		{
			JsonArray.Add(MakeShared<FJsonValueString>(Str));
		}
		return JsonArray;
	}
};
