// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCPToolRegistry.h"

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
};
