// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Material.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "EditorAssetLibrary.h"
#include "Dom/JsonValue.h"

FMCPToolInfo FMCPTool_Material::GetInfo() const
{
	FMCPToolInfo Info;
	Info.Name = TEXT("material");
	Info.Description = TEXT("Material instance creation and assignment operations for meshes and actors");

	Info.Parameters.Add(FMCPToolParameter(TEXT("operation"), TEXT("string"),
		TEXT("Operation: create_material_instance, set_material_parameters, set_skeletal_mesh_material, set_actor_material, get_material_info"), true));

	Info.Parameters.Add(FMCPToolParameter(TEXT("asset_name"), TEXT("string"),
		TEXT("Name for the new material instance asset (for create_material_instance)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("parent_material"), TEXT("string"),
		TEXT("Asset path to parent material (for create_material_instance)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("package_path"), TEXT("string"),
		TEXT("Package path for new asset (default: /Game/Materials/)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("parameters"), TEXT("object"),
		TEXT("Material parameters to set: {scalars: {name: value}, vectors: {name: {r,g,b,a}}, textures: {name: path}}")));

	Info.Parameters.Add(FMCPToolParameter(TEXT("material_instance_path"), TEXT("string"),
		TEXT("Asset path to material instance (for set_material_parameters)")));

	Info.Parameters.Add(FMCPToolParameter(TEXT("skeletal_mesh_path"), TEXT("string"),
		TEXT("Asset path to skeletal mesh (for set_skeletal_mesh_material)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("material_slot"), TEXT("integer"),
		TEXT("Material slot index to set (for set_skeletal_mesh_material)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("material_path"), TEXT("string"),
		TEXT("Asset path to material to assign (for set_skeletal_mesh_material)")));

	Info.Parameters.Add(FMCPToolParameter(TEXT("actor_name"), TEXT("string"),
		TEXT("Name or label of the actor to assign material to (for set_actor_material)")));

	Info.Parameters.Add(FMCPToolParameter(TEXT("asset_path"), TEXT("string"),
		TEXT("Asset path to material (for get_material_info)")));

	Info.Annotations = FMCPToolAnnotations::Modifying();

	return Info;
}

FMCPToolResult FMCPTool_Material::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	if (Operation == TEXT("create_material_instance"))
	{
		return ExecuteCreateMaterialInstance(Params);
	}
	else if (Operation == TEXT("set_material_parameters"))
	{
		return ExecuteSetMaterialParameters(Params);
	}
	else if (Operation == TEXT("set_skeletal_mesh_material"))
	{
		return ExecuteSetSkeletalMeshMaterial(Params);
	}
	else if (Operation == TEXT("set_actor_material"))
	{
		return ExecuteSetActorMaterial(Params);
	}
	else if (Operation == TEXT("get_material_info"))
	{
		return ExecuteGetMaterialInfo(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: create_material_instance, set_material_parameters, set_skeletal_mesh_material, set_actor_material, get_material_info"),
		*Operation));
}

FMCPToolResult FMCPTool_Material::ExecuteCreateMaterialInstance(const TSharedRef<FJsonObject>& Params)
{
	FString AssetName;
	FString ParentMaterialPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("asset_name"), AssetName, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("parent_material"), ParentMaterialPath, Error))
	{
		return Error.GetValue();
	}

	if (!ValidateBlueprintPathParam(ParentMaterialPath, Error))
	{
		return Error.GetValue();
	}

	FString PackagePath = Params->HasField(TEXT("package_path"))
		? Params->GetStringField(TEXT("package_path"))
		: TEXT("/Game/Materials/");

	if (!ValidateBlueprintPathParam(PackagePath, Error))
	{
		return Error.GetValue();
	}

	if (!PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath += TEXT("/");
	}

	UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *ParentMaterialPath);
	if (!ParentMaterial)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to load parent material: %s"), *ParentMaterialPath));
	}

	FString FullPackagePath = PackagePath + AssetName;
	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *FullPackagePath));
	}

	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = ParentMaterial;

	UMaterialInstanceConstant* MatInst = Cast<UMaterialInstanceConstant>(
		Factory->FactoryCreateNew(
			UMaterialInstanceConstant::StaticClass(),
			Package,
			FName(*AssetName),
			RF_Public | RF_Standalone,
			nullptr,
			GWarn
		)
	);

	if (!MatInst)
	{
		return FMCPToolResult::Error(TEXT("Failed to create material instance"));
	}

	if (Params->HasField(TEXT("parameters")))
	{
		const TSharedPtr<FJsonObject>* ParamsObj;
		if (Params->TryGetObjectField(TEXT("parameters"), ParamsObj))
		{
			FString ParamError;
			if (!ApplyParametersFromJson(MatInst, *ParamsObj, ParamError))
			{
				// Asset was successfully created; downgrade parameter failures to a log warning so caller still gets the new instance
				UE_LOG(LogUnrealClaude, Warning, TEXT("Material instance created but some parameters failed: %s"), *ParamError);
			}
		}
	}

	FAssetRegistryModule::AssetCreated(MatInst);
	Package->MarkPackageDirty();

	FString PackageFileName = FPackageName::LongPackageNameToFilename(FullPackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	FSavePackageResultStruct SaveResult = UPackage::Save(Package, MatInst, *PackageFileName, SaveArgs);

	if (!SaveResult.IsSuccessful())
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Material instance created but failed to save: %s"), *FullPackagePath));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), FullPackagePath);
	ResultData->SetStringField(TEXT("asset_name"), AssetName);
	ResultData->SetStringField(TEXT("parent_material"), ParentMaterialPath);
	ResultData->SetBoolField(TEXT("saved"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created material instance: %s"), *FullPackagePath),
		ResultData
	);
}

FMCPToolResult FMCPTool_Material::ExecuteSetMaterialParameters(const TSharedRef<FJsonObject>& Params)
{
	// Canonical is 'material_path' across the material domain. 'material_instance_path'
	// is accepted for backward compat with a deprecation warning so callers converge.
	TArray<FString> Warnings;
	FString MaterialInstancePath = ExtractOptionalString(Params, TEXT("material_path"));
	FString LegacyName;
	const bool bAliasPresent = MaterialInstancePath.IsEmpty()
		&& Params->TryGetStringField(TEXT("material_instance_path"), LegacyName)
		&& !LegacyName.IsEmpty();
	if (bAliasPresent)
	{
		MaterialInstancePath = LegacyName;
		Warnings.Add(TEXT("Parameter 'material_instance_path' is deprecated — use 'material_path' (canonical across the material domain). Treating as alias for this call."));
	}

	auto WithWarnings = [&Warnings](FMCPToolResult R) -> FMCPToolResult
	{
		R.Warnings = Warnings;
		return R;
	};

	if (MaterialInstancePath.IsEmpty())
	{
		return WithWarnings(FMCPToolResult::Error(TEXT("Missing required parameter: material_path")));
	}

	TOptional<FMCPToolResult> Error;
	if (!ValidateBlueprintPathParam(MaterialInstancePath, Error))
	{
		return WithWarnings(Error.GetValue());
	}

	UMaterialInstanceConstant* MatInst = LoadObject<UMaterialInstanceConstant>(nullptr, *MaterialInstancePath);
	if (!MatInst)
	{
		return WithWarnings(FMCPToolResult::Error(FString::Printf(TEXT("Failed to load material instance: %s"), *MaterialInstancePath)));
	}

	const TSharedPtr<FJsonObject>* ParamsObj;
	if (!Params->TryGetObjectField(TEXT("parameters"), ParamsObj))
	{
		return WithWarnings(FMCPToolResult::Error(TEXT("Missing required parameter: parameters")));
	}

	FString ParamError;
	if (!ApplyParametersFromJson(MatInst, *ParamsObj, ParamError))
	{
		return WithWarnings(FMCPToolResult::Error(ParamError));
	}

	MatInst->PostEditChange();
	MatInst->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("material_path"), MaterialInstancePath);
	ResultData->SetBoolField(TEXT("modified"), true);

	return WithWarnings(FMCPToolResult::Success(
		FString::Printf(TEXT("Updated parameters on material instance: %s"), *MaterialInstancePath),
		ResultData
	));
}

FMCPToolResult FMCPTool_Material::ExecuteSetSkeletalMeshMaterial(const TSharedRef<FJsonObject>& Params)
{
	FString SkeletalMeshPath;
	FString MaterialPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("skeletal_mesh_path"), SkeletalMeshPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(SkeletalMeshPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(MaterialPath, Error))
	{
		return Error.GetValue();
	}

	int32 MaterialSlot = 0;
	if (Params->HasField(TEXT("material_slot")))
	{
		MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
		if (MaterialSlot < 0)
		{
			return FMCPToolResult::Error(TEXT("material_slot must be >= 0"));
		}
	}

	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
	if (!SkeletalMesh)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to load skeletal mesh: %s"), *SkeletalMeshPath));
	}

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Material)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
	}

	TArray<FSkeletalMaterial>& Materials = SkeletalMesh->GetMaterials();
	if (MaterialSlot >= Materials.Num())
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Material slot %d out of range. Skeletal mesh has %d material slots."),
			MaterialSlot, Materials.Num()));
	}

	FString OldMaterialName = Materials[MaterialSlot].MaterialInterface
		? Materials[MaterialSlot].MaterialInterface->GetName()
		: TEXT("None");

	Materials[MaterialSlot].MaterialInterface = Material;

	SkeletalMesh->PostEditChange();
	SkeletalMesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("skeletal_mesh"), SkeletalMeshPath);
	ResultData->SetNumberField(TEXT("slot"), MaterialSlot);
	ResultData->SetStringField(TEXT("old_material"), OldMaterialName);
	ResultData->SetStringField(TEXT("new_material"), Material->GetName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set material slot %d on %s to %s"), MaterialSlot, *SkeletalMesh->GetName(), *Material->GetName()),
		ResultData
	);
}

FMCPToolResult FMCPTool_Material::ExecuteSetActorMaterial(const TSharedRef<FJsonObject>& Params)
{
	FString ActorName;
	FString MaterialPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(MaterialPath, Error))
	{
		return Error.GetValue();
	}

	int32 MaterialSlot = 0;
	if (Params->HasField(TEXT("material_slot")))
	{
		MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
		if (MaterialSlot < 0)
		{
			return FMCPToolResult::Error(TEXT("material_slot must be >= 0"));
		}
	}

	UWorld* World = nullptr;
	if (auto EditorError = ValidateEditorContext(World))
	{
		return EditorError.GetValue();
	}

	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor)
	{
		return ActorNotFoundError(ActorName);
	}

	UMeshComponent* MeshComp = Actor->FindComponentByClass<UMeshComponent>();
	if (!MeshComp)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Actor '%s' has no mesh component"), *ActorName));
	}

	int32 NumMaterials = MeshComp->GetNumMaterials();
	if (MaterialSlot >= NumMaterials)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Material slot %d out of range. Mesh component has %d material slots."),
			MaterialSlot, NumMaterials));
	}

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Material)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
	}

	UMaterialInterface* OldMaterial = MeshComp->GetMaterial(MaterialSlot);
	FString OldMaterialName = OldMaterial ? OldMaterial->GetName() : TEXT("None");

	// SetMaterial creates a per-instance runtime override; does not edit the asset
	MeshComp->SetMaterial(MaterialSlot, Material);

	MarkActorDirty(Actor);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor"), Actor->GetName());
	ResultData->SetStringField(TEXT("component"), MeshComp->GetName());
	ResultData->SetStringField(TEXT("component_type"), MeshComp->GetClass()->GetName());
	ResultData->SetNumberField(TEXT("slot"), MaterialSlot);
	ResultData->SetStringField(TEXT("old_material"), OldMaterialName);
	ResultData->SetStringField(TEXT("new_material"), Material->GetName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set material slot %d on actor '%s' (%s) to %s"),
			MaterialSlot, *Actor->GetName(), *MeshComp->GetClass()->GetName(), *Material->GetName()),
		ResultData
	);
}

FMCPToolResult FMCPTool_Material::ExecuteGetMaterialInfo(const TSharedRef<FJsonObject>& Params)
{
	// Canonical is 'material_path' across the material domain. 'asset_path' kept as
	// alias since this op originally followed the generic asset-tool naming.
	TArray<FString> Warnings;
	FString AssetPath = ExtractOptionalString(Params, TEXT("material_path"));
	FString LegacyName;
	const bool bAliasPresent = AssetPath.IsEmpty()
		&& Params->TryGetStringField(TEXT("asset_path"), LegacyName)
		&& !LegacyName.IsEmpty();
	if (bAliasPresent)
	{
		AssetPath = LegacyName;
		Warnings.Add(TEXT("Parameter 'asset_path' is deprecated for 'get_material_info' — use 'material_path' (canonical across the material domain). Treating as alias for this call."));
	}

	auto WithWarnings = [&Warnings](FMCPToolResult R) -> FMCPToolResult
	{
		R.Warnings = Warnings;
		return R;
	};

	if (AssetPath.IsEmpty())
	{
		return WithWarnings(FMCPToolResult::Error(TEXT("Missing required parameter: material_path")));
	}

	TOptional<FMCPToolResult> Error;
	if (!ValidateBlueprintPathParam(AssetPath, Error))
	{
		return WithWarnings(Error.GetValue());
	}

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *AssetPath);
	if (!Material)
	{
		return WithWarnings(FMCPToolResult::Error(FString::Printf(TEXT("Failed to load material: %s"), *AssetPath)));
	}

	TSharedPtr<FJsonObject> ResultData = BuildMaterialInfoJson(Material);

	return WithWarnings(FMCPToolResult::Success(
		FString::Printf(TEXT("Material info: %s"), *Material->GetName()),
		ResultData
	));
}

bool FMCPTool_Material::SetScalarParameter(UMaterialInstanceConstant* MatInst, const FString& ParamName, float Value, FString& OutError)
{
	if (!MatInst)
	{
		OutError = TEXT("Invalid material instance");
		return false;
	}

	MatInst->SetScalarParameterValueEditorOnly(FName(*ParamName), Value);
	return true;
}

bool FMCPTool_Material::SetVectorParameter(UMaterialInstanceConstant* MatInst, const FString& ParamName, const FLinearColor& Value, FString& OutError)
{
	if (!MatInst)
	{
		OutError = TEXT("Invalid material instance");
		return false;
	}

	MatInst->SetVectorParameterValueEditorOnly(FName(*ParamName), Value);
	return true;
}

bool FMCPTool_Material::SetTextureParameter(UMaterialInstanceConstant* MatInst, const FString& ParamName, const FString& TexturePath, FString& OutError)
{
	if (!MatInst)
	{
		OutError = TEXT("Invalid material instance");
		return false;
	}

	UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
	if (!Texture)
	{
		OutError = FString::Printf(TEXT("Failed to load texture: %s"), *TexturePath);
		return false;
	}

	MatInst->SetTextureParameterValueEditorOnly(FName(*ParamName), Texture);
	return true;
}

bool FMCPTool_Material::ApplyParametersFromJson(UMaterialInstanceConstant* MatInst, const TSharedPtr<FJsonObject>& ParamsObj, FString& OutError)
{
	if (!MatInst || !ParamsObj.IsValid())
	{
		OutError = TEXT("Invalid material instance or parameters");
		return false;
	}

	bool bAllSuccess = true;
	TArray<FString> Errors;

	const TSharedPtr<FJsonObject>* ScalarsObj;
	if (ParamsObj->TryGetObjectField(TEXT("scalars"), ScalarsObj))
	{
		for (const auto& Pair : (*ScalarsObj)->Values)
		{
			double Value = 0.0;
			if (Pair.Value->TryGetNumber(Value))
			{
				FString Error;
				if (!SetScalarParameter(MatInst, FString(*Pair.Key), static_cast<float>(Value), Error))
				{
					Errors.Add(Error);
					bAllSuccess = false;
				}
			}
		}
	}

	const TSharedPtr<FJsonObject>* VectorsObj;
	if (ParamsObj->TryGetObjectField(TEXT("vectors"), VectorsObj))
	{
		for (const auto& Pair : (*VectorsObj)->Values)
		{
			const TSharedPtr<FJsonObject>* ColorObj;
			if (Pair.Value->TryGetObject(ColorObj))
			{
				FLinearColor Color;
				(*ColorObj)->TryGetNumberField(TEXT("r"), Color.R);
				(*ColorObj)->TryGetNumberField(TEXT("g"), Color.G);
				(*ColorObj)->TryGetNumberField(TEXT("b"), Color.B);
				Color.A = 1.0f;
				(*ColorObj)->TryGetNumberField(TEXT("a"), Color.A);

				FString Error;
				if (!SetVectorParameter(MatInst, FString(*Pair.Key), Color, Error))
				{
					Errors.Add(Error);
					bAllSuccess = false;
				}
			}
		}
	}

	const TSharedPtr<FJsonObject>* TexturesObj;
	if (ParamsObj->TryGetObjectField(TEXT("textures"), TexturesObj))
	{
		for (const auto& Pair : (*TexturesObj)->Values)
		{
			FString TexturePath;
			if (Pair.Value->TryGetString(TexturePath))
			{
				FString Error;
				if (!SetTextureParameter(MatInst, FString(*Pair.Key), TexturePath, Error))
				{
					Errors.Add(Error);
					bAllSuccess = false;
				}
			}
		}
	}

	if (!bAllSuccess)
	{
		OutError = FString::Join(Errors, TEXT("; "));
	}

	return bAllSuccess;
}

TSharedPtr<FJsonObject> FMCPTool_Material::BuildMaterialInfoJson(UMaterialInterface* Material)
{
	TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();

	Info->SetStringField(TEXT("name"), Material->GetName());
	Info->SetStringField(TEXT("path"), Material->GetPathName());
	Info->SetStringField(TEXT("class"), Material->GetClass()->GetName());

	if (UMaterialInstance* MatInst = Cast<UMaterialInstance>(Material))
	{
		Info->SetBoolField(TEXT("is_instance"), true);
		if (MatInst->Parent)
		{
			Info->SetStringField(TEXT("parent"), MatInst->Parent->GetPathName());
		}

		TSharedPtr<FJsonObject> ScalarsObj = MakeShared<FJsonObject>();
		for (const FScalarParameterValue& Param : MatInst->ScalarParameterValues)
		{
			ScalarsObj->SetNumberField(Param.ParameterInfo.Name.ToString(), Param.ParameterValue);
		}
		Info->SetObjectField(TEXT("scalar_parameters"), ScalarsObj);

		TSharedPtr<FJsonObject> VectorsObj = MakeShared<FJsonObject>();
		for (const FVectorParameterValue& Param : MatInst->VectorParameterValues)
		{
			TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
			ColorObj->SetNumberField(TEXT("r"), Param.ParameterValue.R);
			ColorObj->SetNumberField(TEXT("g"), Param.ParameterValue.G);
			ColorObj->SetNumberField(TEXT("b"), Param.ParameterValue.B);
			ColorObj->SetNumberField(TEXT("a"), Param.ParameterValue.A);
			VectorsObj->SetObjectField(Param.ParameterInfo.Name.ToString(), ColorObj);
		}
		Info->SetObjectField(TEXT("vector_parameters"), VectorsObj);

		TSharedPtr<FJsonObject> TexturesObj = MakeShared<FJsonObject>();
		for (const FTextureParameterValue& Param : MatInst->TextureParameterValues)
		{
			FString TexturePath = Param.ParameterValue ? Param.ParameterValue->GetPathName() : TEXT("None");
			TexturesObj->SetStringField(Param.ParameterInfo.Name.ToString(), TexturePath);
		}
		Info->SetObjectField(TEXT("texture_parameters"), TexturesObj);
	}
	else
	{
		Info->SetBoolField(TEXT("is_instance"), false);
	}

	return Info;
}

TArray<TSharedPtr<FJsonValue>> FMCPTool_Material::GetMaterialParameters(UMaterialInterface* Material)
{
	TArray<TSharedPtr<FJsonValue>> Params;

	if (!Material)
	{
		return Params;
	}

	TArray<FMaterialParameterInfo> ScalarParams;
	TArray<FGuid> ScalarGuids;
	Material->GetAllScalarParameterInfo(ScalarParams, ScalarGuids);

	for (const FMaterialParameterInfo& ParamInfo : ScalarParams)
	{
		TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
		ParamObj->SetStringField(TEXT("name"), ParamInfo.Name.ToString());
		ParamObj->SetStringField(TEXT("type"), TEXT("scalar"));
		Params.Add(MakeShared<FJsonValueObject>(ParamObj));
	}

	TArray<FMaterialParameterInfo> VectorParams;
	TArray<FGuid> VectorGuids;
	Material->GetAllVectorParameterInfo(VectorParams, VectorGuids);

	for (const FMaterialParameterInfo& ParamInfo : VectorParams)
	{
		TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
		ParamObj->SetStringField(TEXT("name"), ParamInfo.Name.ToString());
		ParamObj->SetStringField(TEXT("type"), TEXT("vector"));
		Params.Add(MakeShared<FJsonValueObject>(ParamObj));
	}

	TArray<FMaterialParameterInfo> TextureParams;
	TArray<FGuid> TextureGuids;
	Material->GetAllTextureParameterInfo(TextureParams, TextureGuids);

	for (const FMaterialParameterInfo& ParamInfo : TextureParams)
	{
		TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
		ParamObj->SetStringField(TEXT("name"), ParamInfo.Name.ToString());
		ParamObj->SetStringField(TEXT("type"), TEXT("texture"));
		Params.Add(MakeShared<FJsonValueObject>(ParamObj));
	}

	return Params;
}
