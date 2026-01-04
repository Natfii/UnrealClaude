// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

/**
 * Shared utility classes and functions for UnrealClaude plugin
 * Centralizes common patterns to reduce code duplication
 */

/**
 * Custom output device to capture console command output
 * Used by MCP tools and script execution to capture command results
 * Note: Named differently to avoid collision with engine's FStringOutputDevice
 */
class FUnrealClaudeOutputDevice : public FOutputDevice
{
public:
	FString Output;

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		Output += V;
		Output += TEXT("\n");
	}

	/** Clear captured output */
	void Clear()
	{
		Output.Empty();
	}

	/** Get trimmed output (removes trailing whitespace) */
	FString GetTrimmedOutput() const
	{
		return Output.TrimEnd();
	}
};

/**
 * JSON parsing utilities for MCP tools
 * Provides safe extraction of transform data from JSON objects
 */
namespace UnrealClaudeJsonUtils
{
	/**
	 * Safely extract a FVector from a JSON object
	 * @param JsonObj - The JSON object containing x, y, z fields
	 * @param DefaultValue - Value to use if extraction fails
	 * @return Extracted vector or default
	 */
	inline FVector ExtractVector(const TSharedPtr<FJsonObject>& JsonObj, const FVector& DefaultValue = FVector::ZeroVector)
	{
		if (!JsonObj.IsValid())
		{
			return DefaultValue;
		}

		FVector Result = DefaultValue;
		JsonObj->TryGetNumberField(TEXT("x"), Result.X);
		JsonObj->TryGetNumberField(TEXT("y"), Result.Y);
		JsonObj->TryGetNumberField(TEXT("z"), Result.Z);
		return Result;
	}

	/**
	 * Safely extract a FRotator from a JSON object
	 * @param JsonObj - The JSON object containing pitch, yaw, roll fields
	 * @param DefaultValue - Value to use if extraction fails
	 * @return Extracted rotator or default
	 */
	inline FRotator ExtractRotator(const TSharedPtr<FJsonObject>& JsonObj, const FRotator& DefaultValue = FRotator::ZeroRotator)
	{
		if (!JsonObj.IsValid())
		{
			return DefaultValue;
		}

		FRotator Result = DefaultValue;
		JsonObj->TryGetNumberField(TEXT("pitch"), Result.Pitch);
		JsonObj->TryGetNumberField(TEXT("yaw"), Result.Yaw);
		JsonObj->TryGetNumberField(TEXT("roll"), Result.Roll);
		return Result;
	}

	/**
	 * Safely extract a scale FVector from a JSON object
	 * @param JsonObj - The JSON object containing x, y, z fields
	 * @param DefaultValue - Value to use if extraction fails (typically 1,1,1)
	 * @return Extracted scale or default
	 */
	inline FVector ExtractScale(const TSharedPtr<FJsonObject>& JsonObj, const FVector& DefaultValue = FVector::OneVector)
	{
		return ExtractVector(JsonObj, DefaultValue);
	}

	/**
	 * Create a JSON object from a FVector
	 */
	inline TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector)
	{
		TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
		JsonObj->SetNumberField(TEXT("x"), Vector.X);
		JsonObj->SetNumberField(TEXT("y"), Vector.Y);
		JsonObj->SetNumberField(TEXT("z"), Vector.Z);
		return JsonObj;
	}

	/**
	 * Create a JSON object from a FRotator
	 */
	inline TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator)
	{
		TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
		JsonObj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
		JsonObj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
		JsonObj->SetNumberField(TEXT("roll"), Rotator.Roll);
		return JsonObj;
	}
}
