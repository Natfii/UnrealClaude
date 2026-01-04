// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"

/**
 * Blueprint loading and validation utilities
 *
 * Responsibilities:
 * - Loading Blueprints from asset paths
 * - Validating Blueprint paths for security
 * - Checking Blueprint editability
 * - Compiling and saving Blueprints
 *
 * Security:
 * - Blocks access to /Engine/ and /Script/ paths
 * - Prevents path traversal attacks
 * - Validates character safety
 */
class UNREALCLAUDE_API FBlueprintLoader
{
public:
	/**
	 * Load a Blueprint asset from path
	 * @param BlueprintPath - Asset path (e.g., "/Game/Blueprints/BP_Actor")
	 * @param OutError - Error message if loading fails
	 * @return Loaded Blueprint or nullptr
	 */
	static UBlueprint* LoadBlueprint(const FString& BlueprintPath, FString& OutError);

	/**
	 * Validate Blueprint path is safe and accessible
	 * Checks: not empty, not engine path, no traversal, safe characters
	 * @param BlueprintPath - Path to validate
	 * @param OutError - Error message if validation fails
	 * @return true if valid
	 */
	static bool ValidateBlueprintPath(const FString& BlueprintPath, FString& OutError);

	/**
	 * Check if Blueprint is editable (not engine, not read-only)
	 * @param Blueprint - Blueprint to check
	 * @param OutError - Error message if not editable
	 * @return true if editable
	 */
	static bool IsBlueprintEditable(UBlueprint* Blueprint, FString& OutError);

	/**
	 * Compile Blueprint and capture any errors
	 * @param Blueprint - Blueprint to compile
	 * @param OutError - Compilation errors if any
	 * @return true if compilation succeeded
	 */
	static bool CompileBlueprint(UBlueprint* Blueprint, FString& OutError);

	/**
	 * Mark Blueprint as modified (dirty)
	 * @param Blueprint - Blueprint to mark
	 */
	static void MarkBlueprintDirty(UBlueprint* Blueprint);

	/**
	 * Create new Blueprint of specified type and parent class
	 * @param PackagePath - Package path (e.g., "/Game/Blueprints")
	 * @param BlueprintName - Name of Blueprint to create
	 * @param ParentClass - Parent class for the Blueprint
	 * @param BlueprintType - Type of Blueprint (Normal, Interface, etc.)
	 * @param OutError - Error message if creation fails
	 * @return Created Blueprint or nullptr
	 */
	static UBlueprint* CreateBlueprint(
		const FString& PackagePath,
		const FString& BlueprintName,
		UClass* ParentClass,
		EBlueprintType BlueprintType,
		FString& OutError
	);

	/**
	 * Find parent class from string
	 * Supports: short names ("Actor"), full paths ("/Script/Engine.Pawn")
	 * @param ParentClassName - Class name
	 * @param OutError - Error message if not found
	 * @return UClass or nullptr
	 */
	static UClass* FindParentClass(const FString& ParentClassName, FString& OutError);
};
