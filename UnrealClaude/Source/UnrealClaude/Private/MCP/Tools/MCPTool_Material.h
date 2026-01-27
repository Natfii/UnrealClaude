// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * MCP Tool for material operations.
 *
 * Provides operations for:
 * - Creating Material Instances (Constant or Dynamic)
 * - Setting material parameters (scalar, vector, texture)
 * - Assigning materials to Skeletal Mesh asset slots
 *
 * Operations:
 * - create_material_instance: Create a new UMaterialInstanceConstant asset
 * - set_material_parameters: Set parameters on an existing material instance
 * - set_skeletal_mesh_material: Set a material slot on a USkeletalMesh asset
 * - set_actor_material: Assign a material to an actor's mesh component at runtime
 * - get_material_info: Get information about a material or material instance
 */
class FMCPTool_Material : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override;
	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	// Operation handlers
	FMCPToolResult ExecuteCreateMaterialInstance(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetMaterialParameters(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetSkeletalMeshMaterial(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetActorMaterial(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetMaterialInfo(const TSharedRef<FJsonObject>& Params);

	// Helper methods
	bool SetScalarParameter(class UMaterialInstanceConstant* MatInst, const FString& ParamName, float Value, FString& OutError);
	bool SetVectorParameter(class UMaterialInstanceConstant* MatInst, const FString& ParamName, const FLinearColor& Value, FString& OutError);
	bool SetTextureParameter(class UMaterialInstanceConstant* MatInst, const FString& ParamName, const FString& TexturePath, FString& OutError);
	bool ApplyParametersFromJson(class UMaterialInstanceConstant* MatInst, const TSharedPtr<FJsonObject>& ParamsObj, FString& OutError);

	// Utility
	TSharedPtr<FJsonObject> BuildMaterialInfoJson(class UMaterialInterface* Material);
	TArray<TSharedPtr<FJsonValue>> GetMaterialParameters(class UMaterialInterface* Material);
};
