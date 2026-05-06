// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "MCP/MCPToolBase.h"

class UMaterial;
class UMaterialExpressionCustom;
class UMaterialExpression;

/**
 * MCP Tool for creating base materials with Custom HLSL nodes.
 * Handles material creation, expression nodes, wiring, and compilation.
 */
class FMCPTool_MaterialCreate : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override;
	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	// Operations
	FMCPToolResult ExecuteCreateMaterial(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddCustomNode(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddParameter(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteWireToCustom(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteConnectOutput(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetCustomCode(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteRecompileMaterial(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetupCoronaMaterial(const TSharedRef<FJsonObject>& Params);

	// Helpers
	UMaterial* LoadOrCreateMaterial(const FString& AssetPath, bool bCreateIfMissing, FString& OutError);
	UMaterialExpressionCustom* FindCustomNode(UMaterial* Material, const FString& NodeDesc);
	UMaterialExpression* FindParameterNode(UMaterial* Material, const FString& ParamName);
	bool SetMaterialBlendMode(UMaterial* Material, const FString& BlendMode, FString& OutError);
	bool SetMaterialShadingModel(UMaterial* Material, const FString& ShadingModel, FString& OutError);
};
