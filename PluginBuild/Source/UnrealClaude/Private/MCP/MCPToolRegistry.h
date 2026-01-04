// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Parameter definition for an MCP tool
 */
struct FMCPToolParameter
{
	/** Parameter name */
	FString Name;

	/** Parameter type (string, number, boolean, array, object) */
	FString Type;

	/** Description of the parameter */
	FString Description;

	/** Whether this parameter is required */
	bool bRequired;

	/** Default value if not provided */
	FString DefaultValue;

	FMCPToolParameter()
		: bRequired(false)
	{}

	FMCPToolParameter(const FString& InName, const FString& InType, const FString& InDescription, bool bInRequired = false, const FString& InDefault = TEXT(""))
		: Name(InName)
		, Type(InType)
		, Description(InDescription)
		, bRequired(bInRequired)
		, DefaultValue(InDefault)
	{}
};

/**
 * Information about an MCP tool
 */
struct FMCPToolInfo
{
	/** Unique name of the tool */
	FString Name;

	/** Human-readable description */
	FString Description;

	/** Parameter definitions */
	TArray<FMCPToolParameter> Parameters;
};

/**
 * Result from executing an MCP tool
 */
struct FMCPToolResult
{
	/** Whether the operation succeeded */
	bool bSuccess;

	/** Human-readable message */
	FString Message;

	/** Optional structured data result */
	TSharedPtr<FJsonObject> Data;

	FMCPToolResult()
		: bSuccess(false)
	{}

	static FMCPToolResult Success(const FString& InMessage, TSharedPtr<FJsonObject> InData = nullptr)
	{
		FMCPToolResult Result;
		Result.bSuccess = true;
		Result.Message = InMessage;
		Result.Data = InData;
		return Result;
	}

	static FMCPToolResult Error(const FString& InMessage)
	{
		FMCPToolResult Result;
		Result.bSuccess = false;
		Result.Message = InMessage;
		return Result;
	}
};

/**
 * Base class for MCP tools
 */
class IMCPTool
{
public:
	virtual ~IMCPTool() = default;

	/** Get tool info (name, description, parameters) */
	virtual FMCPToolInfo GetInfo() const = 0;

	/** Execute the tool with given parameters */
	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) = 0;
};

/**
 * Registry for managing MCP tools
 */
class UNREALCLAUDE_API FMCPToolRegistry
{
public:
	FMCPToolRegistry();
	~FMCPToolRegistry();

	/** Register a tool */
	void RegisterTool(TSharedPtr<IMCPTool> Tool);

	/** Unregister a tool by name */
	void UnregisterTool(const FString& ToolName);

	/** Get all registered tools */
	TArray<FMCPToolInfo> GetAllTools() const;

	/** Execute a tool by name */
	FMCPToolResult ExecuteTool(const FString& ToolName, const TSharedRef<FJsonObject>& Params);

	/** Check if a tool exists */
	bool HasTool(const FString& ToolName) const;

private:
	/** Register all built-in tools */
	void RegisterBuiltinTools();

	/** Map of tool name to tool instance */
	TMap<FString, TSharedPtr<IMCPTool>> Tools;
};
