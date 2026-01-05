// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCPToolRegistry.h"
#include "MCPParamValidator.h"
#include "BlueprintUtils.h"
#include "Engine/Blueprint.h"

/**
 * Context helper for Blueprint load-validate-modify operations
 * Eliminates ~220 lines of duplicate boilerplate across MCP tools
 *
 * Usage:
 *   FMCPBlueprintLoadContext Context;
 *   if (auto Error = Context.LoadAndValidate(Params))
 *       return Error.GetValue();
 *
 *   // Use Context.Blueprint for operations
 *
 *   if (auto Error = Context.CompileAndFinalize())
 *       return Error.GetValue();
 */
class FMCPBlueprintLoadContext
{
public:
	/** The loaded Blueprint (valid after successful LoadAndValidate) */
	UBlueprint* Blueprint = nullptr;

	/** The Blueprint path that was loaded */
	FString BlueprintPath;

	/** Last error message (for custom error handling) */
	FString LastError;

	/**
	 * Load and validate a Blueprint from JSON parameters
	 * Handles: path extraction, path validation, loading, and editability check
	 *
	 * @param Params - JSON parameters containing "blueprint_path"
	 * @param PathParamName - Name of the path parameter (default: "blueprint_path")
	 * @return Error result if any step fails, empty optional on success
	 */
	TOptional<FMCPToolResult> LoadAndValidate(
		const TSharedRef<FJsonObject>& Params,
		const FString& PathParamName = TEXT("blueprint_path"))
	{
		// Extract blueprint path
		if (!Params->TryGetStringField(PathParamName, BlueprintPath) || BlueprintPath.IsEmpty())
		{
			LastError = FString::Printf(TEXT("Missing required parameter: %s"), *PathParamName);
			return FMCPToolResult::Error(LastError);
		}

		// Validate path (security check)
		if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, LastError))
		{
			return FMCPToolResult::Error(LastError);
		}

		// Load Blueprint
		Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LastError);
		if (!Blueprint)
		{
			return FMCPToolResult::Error(LastError);
		}

		// Check if editable
		if (!FBlueprintUtils::IsBlueprintEditable(Blueprint, LastError))
		{
			return FMCPToolResult::Error(LastError);
		}

		return TOptional<FMCPToolResult>();
	}

	/**
	 * Load a Blueprint without editability check (for query operations)
	 *
	 * @param Params - JSON parameters containing the path
	 * @param PathParamName - Name of the path parameter
	 * @return Error result if loading fails, empty optional on success
	 */
	TOptional<FMCPToolResult> LoadForQuery(
		const TSharedRef<FJsonObject>& Params,
		const FString& PathParamName = TEXT("blueprint_path"))
	{
		// Extract blueprint path
		if (!Params->TryGetStringField(PathParamName, BlueprintPath) || BlueprintPath.IsEmpty())
		{
			LastError = FString::Printf(TEXT("Missing required parameter: %s"), *PathParamName);
			return FMCPToolResult::Error(LastError);
		}

		// Validate path (security check)
		if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, LastError))
		{
			return FMCPToolResult::Error(LastError);
		}

		// Load Blueprint
		Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LastError);
		if (!Blueprint)
		{
			return FMCPToolResult::Error(LastError);
		}

		return TOptional<FMCPToolResult>();
	}

	/**
	 * Compile the Blueprint and mark it dirty
	 * Call this after making modifications
	 *
	 * @param OperationName - Name of the operation for error messages
	 * @return Error result if compilation fails, empty optional on success
	 */
	TOptional<FMCPToolResult> CompileAndFinalize(const FString& OperationName = TEXT("Operation"))
	{
		if (!Blueprint)
		{
			LastError = TEXT("No Blueprint loaded");
			return FMCPToolResult::Error(LastError);
		}

		// Compile the Blueprint
		if (!FBlueprintUtils::CompileBlueprint(Blueprint, LastError))
		{
			return FMCPToolResult::Error(FString::Printf(
				TEXT("%s succeeded but compilation failed: %s"), *OperationName, *LastError));
		}

		// Mark dirty
		FBlueprintUtils::MarkBlueprintDirty(Blueprint);

		return TOptional<FMCPToolResult>();
	}

	/**
	 * Build standard result JSON with blueprint info
	 * @return JSON object with blueprint_path and compiled fields
	 */
	TSharedPtr<FJsonObject> BuildResultJson() const
	{
		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		if (Blueprint)
		{
			ResultData->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
			ResultData->SetBoolField(TEXT("compiled"), true);
		}
		return ResultData;
	}

	/**
	 * Check if Blueprint is valid
	 */
	bool IsValid() const { return Blueprint != nullptr; }

	/**
	 * Get the Blueprint (convenience accessor)
	 */
	UBlueprint* Get() const { return Blueprint; }
};
