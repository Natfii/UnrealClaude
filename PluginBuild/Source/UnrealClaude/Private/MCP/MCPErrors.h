// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCPToolRegistry.h"

/**
 * Standardized error codes for MCP operations
 *
 * Error codes are grouped by category:
 * - 1xx: Parameter/Input errors
 * - 2xx: Validation errors
 * - 3xx: Not found errors
 * - 4xx: Operation errors
 * - 5xx: Context/Environment errors
 */
enum class EMCPErrorCode : int32
{
	// Parameter errors (1xx)
	MissingParameter = 100,
	InvalidParameterType = 101,
	InvalidParameterValue = 102,

	// Validation errors (2xx)
	ValidationFailed = 200,
	PathTraversal = 201,
	ForbiddenCommand = 202,
	InvalidName = 203,
	StringTooLong = 204,

	// Not found errors (3xx)
	ActorNotFound = 300,
	BlueprintNotFound = 301,
	ClassNotFound = 302,
	PropertyNotFound = 303,
	FunctionNotFound = 304,
	GraphNotFound = 305,
	NodeNotFound = 306,
	ToolNotFound = 307,

	// Operation errors (4xx)
	OperationFailed = 400,
	CompilationFailed = 401,
	SpawnFailed = 402,
	ConnectionFailed = 403,
	CannotModify = 404,

	// Context errors (5xx)
	EditorNotAvailable = 500,
	NoActiveWorld = 501,
	ViewportNotAvailable = 502,
	Timeout = 503
};

/**
 * Standardized error message factory
 *
 * Provides consistent error message formatting:
 * - All messages start with uppercase
 * - No trailing periods (JSON convention)
 * - Consistent parameter substitution
 * - Categorized by error type
 */
class FMCPErrors
{
public:
	// ===== Parameter Errors =====

	static FMCPToolResult MissingParameter(const FString& ParamName)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Missing required parameter: %s"), *ParamName));
	}

	static FMCPToolResult InvalidParameterType(const FString& ParamName, const FString& ExpectedType)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid type for parameter '%s': expected %s"), *ParamName, *ExpectedType));
	}

	static FMCPToolResult InvalidParameterValue(const FString& ParamName, const FString& Reason)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid value for parameter '%s': %s"), *ParamName, *Reason));
	}

	// ===== Validation Errors =====

	static FMCPToolResult ValidationFailed(const FString& Message)
	{
		return FMCPToolResult::Error(Message);
	}

	static FMCPToolResult PathTraversal(const FString& Path)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Path traversal not allowed: %s"), *Path));
	}

	static FMCPToolResult ForbiddenCommand(const FString& Command)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Command not allowed: %s"), *Command));
	}

	static FMCPToolResult InvalidName(const FString& NameType, const FString& Name, const FString& Reason)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid %s '%s': %s"), *NameType, *Name, *Reason));
	}

	static FMCPToolResult StringTooLong(const FString& ParamName, int32 MaxLength)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Parameter '%s' exceeds maximum length of %d characters"), *ParamName, MaxLength));
	}

	// ===== Not Found Errors =====

	static FMCPToolResult ActorNotFound(const FString& ActorName)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	static FMCPToolResult ActorsNotFound(const TArray<FString>& ActorNames)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Actors not found: %s"), *FString::Join(ActorNames, TEXT(", "))));
	}

	static FMCPToolResult BlueprintNotFound(const FString& Path)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	static FMCPToolResult ClassNotFound(const FString& ClassName)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Class not found: %s"), *ClassName));
	}

	static FMCPToolResult PropertyNotFound(const FString& PropertyPath, const FString& ObjectName)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Property '%s' not found on %s"), *PropertyPath, *ObjectName));
	}

	static FMCPToolResult FunctionNotFound(const FString& FunctionName)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
	}

	static FMCPToolResult GraphNotFound(const FString& GraphName)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
	}

	static FMCPToolResult NodeNotFound(const FString& NodeId)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeId));
	}

	static FMCPToolResult ToolNotFound(const FString& ToolName)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Tool not found: %s"), *ToolName));
	}

	// ===== Operation Errors =====

	static FMCPToolResult OperationFailed(const FString& Operation, const FString& Reason)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to %s: %s"), *Operation, *Reason));
	}

	static FMCPToolResult OperationFailed(const FString& Message)
	{
		return FMCPToolResult::Error(Message);
	}

	static FMCPToolResult CompilationFailed(const FString& BlueprintName)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint compilation failed: %s"), *BlueprintName));
	}

	static FMCPToolResult SpawnFailed(const FString& ClassName)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to spawn actor of class: %s"), *ClassName));
	}

	static FMCPToolResult ConnectionFailed(const FString& SourcePin, const FString& TargetPin, const FString& Reason)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Cannot connect '%s' to '%s': %s"), *SourcePin, *TargetPin, *Reason));
	}

	static FMCPToolResult CannotModify(const FString& ObjectType, const FString& Reason)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Cannot modify %s: %s"), *ObjectType, *Reason));
	}

	// ===== Context Errors =====

	static FMCPToolResult EditorNotAvailable()
	{
		return FMCPToolResult::Error(TEXT("Editor not available"));
	}

	static FMCPToolResult NoActiveWorld()
	{
		return FMCPToolResult::Error(TEXT("No active world"));
	}

	static FMCPToolResult ViewportNotAvailable()
	{
		return FMCPToolResult::Error(TEXT("No viewport available"));
	}

	static FMCPToolResult Timeout(int32 TimeoutMs)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Operation timed out after %d ms"), TimeoutMs));
	}

	// ===== Helpers for OutError pattern =====
	// For functions using FString& OutError instead of FMCPToolResult

	static void SetMissingParameter(FString& OutError, const FString& ParamName)
	{
		OutError = FString::Printf(TEXT("Missing required parameter: %s"), *ParamName);
	}

	static void SetActorNotFound(FString& OutError, const FString& ActorName)
	{
		OutError = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
	}

	static void SetNotFound(FString& OutError, const FString& ObjectType, const FString& Name)
	{
		OutError = FString::Printf(TEXT("%s not found: %s"), *ObjectType, *Name);
	}

	static void SetOperationFailed(FString& OutError, const FString& Operation, const FString& Reason)
	{
		OutError = FString::Printf(TEXT("Failed to %s: %s"), *Operation, *Reason);
	}

	static void SetInvalidValue(FString& OutError, const FString& ParamName, const FString& Reason)
	{
		OutError = FString::Printf(TEXT("Invalid %s: %s"), *ParamName, *Reason);
	}

	static void SetNullObject(FString& OutError, const FString& ObjectType)
	{
		OutError = FString::Printf(TEXT("%s is null"), *ObjectType);
	}
};
