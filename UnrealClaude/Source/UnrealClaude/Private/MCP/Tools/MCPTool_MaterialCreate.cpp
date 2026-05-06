// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_MaterialCreate.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "EditorAssetLibrary.h"

FMCPToolInfo FMCPTool_MaterialCreate::GetInfo() const
{
	FMCPToolInfo Info;
	Info.Name = TEXT("material_create");
	Info.Description = TEXT("Create and edit base materials with Custom HLSL nodes, parameters, and wiring");

	Info.Parameters.Add(FMCPToolParameter(TEXT("operation"), TEXT("string"),
		TEXT("Operation: create_material, add_custom_node, add_parameter, wire_to_custom, connect_output, set_custom_code, recompile_material, setup_corona_material"), true));

	// Common
	Info.Parameters.Add(FMCPToolParameter(TEXT("material_path"), TEXT("string"),
		TEXT("Asset path to material (e.g., /Game/Materials/M_MyMaterial)")));

	// create_material
	Info.Parameters.Add(FMCPToolParameter(TEXT("blend_mode"), TEXT("string"),
		TEXT("Blend mode: opaque, masked, translucent, additive, modulate (for create_material)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("shading_model"), TEXT("string"),
		TEXT("Shading model: unlit, default_lit, subsurface, etc. (for create_material)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("two_sided"), TEXT("boolean"),
		TEXT("Enable two-sided rendering (for create_material)")));

	// add_custom_node
	Info.Parameters.Add(FMCPToolParameter(TEXT("code"), TEXT("string"),
		TEXT("HLSL code for Custom node (for add_custom_node, set_custom_code)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("description"), TEXT("string"),
		TEXT("Description/name for Custom node (for add_custom_node)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("inputs"), TEXT("array"),
		TEXT("Array of input names for Custom node (for add_custom_node)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("output_type"), TEXT("string"),
		TEXT("Output type: float, float2, float3, float4 (for add_custom_node)")));

	// add_parameter
	Info.Parameters.Add(FMCPToolParameter(TEXT("param_name"), TEXT("string"),
		TEXT("Parameter name (for add_parameter)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("param_type"), TEXT("string"),
		TEXT("Parameter type: scalar, vector (for add_parameter)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("default_value"), TEXT("number|object"),
		TEXT("Default value - number for scalar, {r,g,b,a} for vector (for add_parameter)")));

	// wire_to_custom
	Info.Parameters.Add(FMCPToolParameter(TEXT("custom_input"), TEXT("string"),
		TEXT("Custom node input name to wire to (for wire_to_custom)")));

	// connect_output
	Info.Parameters.Add(FMCPToolParameter(TEXT("output_pin"), TEXT("string"),
		TEXT("Material output: emissive, base_color, opacity, etc. (for connect_output)")));

	// setup_corona_material - convenience operation
	Info.Parameters.Add(FMCPToolParameter(TEXT("shader_file"), TEXT("string"),
		TEXT("Path to .hlsl file to read code from (for setup_corona_material)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("parameters"), TEXT("array"),
		TEXT("Array of {name, type, default} for parameters (for setup_corona_material)")));

	Info.Annotations = FMCPToolAnnotations::Modifying();

	return Info;
}

FMCPToolResult FMCPTool_MaterialCreate::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	if (Operation == TEXT("create_material"))
	{
		return ExecuteCreateMaterial(Params);
	}
	else if (Operation == TEXT("add_custom_node"))
	{
		return ExecuteAddCustomNode(Params);
	}
	else if (Operation == TEXT("add_parameter"))
	{
		return ExecuteAddParameter(Params);
	}
	else if (Operation == TEXT("wire_to_custom"))
	{
		return ExecuteWireToCustom(Params);
	}
	else if (Operation == TEXT("connect_output"))
	{
		return ExecuteConnectOutput(Params);
	}
	else if (Operation == TEXT("set_custom_code"))
	{
		return ExecuteSetCustomCode(Params);
	}
	else if (Operation == TEXT("recompile_material"))
	{
		return ExecuteRecompileMaterial(Params);
	}
	else if (Operation == TEXT("setup_corona_material"))
	{
		return ExecuteSetupCoronaMaterial(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s"), *Operation));
}

FMCPToolResult FMCPTool_MaterialCreate::ExecuteCreateMaterial(const TSharedRef<FJsonObject>& Params)
{
	FString MaterialPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(MaterialPath, Error))
	{
		return Error.GetValue();
	}

	// Delete existing if present
	if (UEditorAssetLibrary::DoesAssetExist(MaterialPath))
	{
		UEditorAssetLibrary::DeleteAsset(MaterialPath);
	}

	// Extract package path and asset name
	FString PackagePath, AssetName;
	MaterialPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	// Create package
	UPackage* Package = CreatePackage(*MaterialPath);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *MaterialPath));
	}

	// Create material
	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UMaterial* Material = Cast<UMaterial>(Factory->FactoryCreateNew(
		UMaterial::StaticClass(),
		Package,
		FName(*AssetName),
		RF_Public | RF_Standalone,
		nullptr,
		GWarn
	));

	if (!Material)
	{
		return FMCPToolResult::Error(TEXT("Failed to create material"));
	}

	// Set blend mode
	if (Params->HasField(TEXT("blend_mode")))
	{
		FString BlendMode = Params->GetStringField(TEXT("blend_mode")).ToLower();
		FString BlendError;
		if (!SetMaterialBlendMode(Material, BlendMode, BlendError))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Blend mode warning: %s"), *BlendError);
		}
	}

	// Set shading model
	if (Params->HasField(TEXT("shading_model")))
	{
		FString ShadingModel = Params->GetStringField(TEXT("shading_model")).ToLower();
		FString ShadingError;
		if (!SetMaterialShadingModel(Material, ShadingModel, ShadingError))
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Shading model warning: %s"), *ShadingError);
		}
	}

	// Set two-sided
	if (Params->HasField(TEXT("two_sided")))
	{
		Material->TwoSided = Params->GetBoolField(TEXT("two_sided"));
	}

	// Notify and save
	FAssetRegistryModule::AssetCreated(Material);
	Package->MarkPackageDirty();

	FString PackageFileName = FPackageName::LongPackageNameToFilename(MaterialPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::Save(Package, Material, *PackageFileName, SaveArgs);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("material_path"), MaterialPath);
	ResultData->SetBoolField(TEXT("created"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created material: %s"), *MaterialPath),
		ResultData
	);
}

FMCPToolResult FMCPTool_MaterialCreate::ExecuteAddCustomNode(const TSharedRef<FJsonObject>& Params)
{
	FString MaterialPath, Code;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("code"), Code, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UMaterial* Material = LoadOrCreateMaterial(MaterialPath, false, LoadError);
	if (!Material)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Create Custom node
	UMaterialExpressionCustom* CustomNode = Cast<UMaterialExpressionCustom>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionCustom::StaticClass(), -200, 0)
	);

	if (!CustomNode)
	{
		return FMCPToolResult::Error(TEXT("Failed to create Custom node"));
	}

	// Set code
	CustomNode->Code = Code;

	// Set description
	if (Params->HasField(TEXT("description")))
	{
		CustomNode->Description = Params->GetStringField(TEXT("description"));
	}

	// Set output type
	if (Params->HasField(TEXT("output_type")))
	{
		FString OutputType = Params->GetStringField(TEXT("output_type")).ToLower();
		if (OutputType == TEXT("float") || OutputType == TEXT("float1"))
		{
			CustomNode->OutputType = CMOT_Float1;
		}
		else if (OutputType == TEXT("float2"))
		{
			CustomNode->OutputType = CMOT_Float2;
		}
		else if (OutputType == TEXT("float3"))
		{
			CustomNode->OutputType = CMOT_Float3;
		}
		else if (OutputType == TEXT("float4"))
		{
			CustomNode->OutputType = CMOT_Float4;
		}
	}

	// Add inputs
	const TArray<TSharedPtr<FJsonValue>>* InputsArray;
	if (Params->TryGetArrayField(TEXT("inputs"), InputsArray))
	{
		CustomNode->Inputs.Empty();
		for (const TSharedPtr<FJsonValue>& InputVal : *InputsArray)
		{
			FString InputName;
			if (InputVal->TryGetString(InputName))
			{
				FCustomInput NewInput;
				NewInput.InputName = FName(*InputName);
				CustomNode->Inputs.Add(NewInput);
			}
		}
	}

	UMaterialEditingLibrary::RecompileMaterial(Material);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("material_path"), MaterialPath);
	ResultData->SetStringField(TEXT("node_description"), CustomNode->Description);
	ResultData->SetNumberField(TEXT("input_count"), CustomNode->Inputs.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added Custom node with %d inputs"), CustomNode->Inputs.Num()),
		ResultData
	);
}

FMCPToolResult FMCPTool_MaterialCreate::ExecuteAddParameter(const TSharedRef<FJsonObject>& Params)
{
	FString MaterialPath, ParamName, ParamType;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("param_name"), ParamName, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("param_type"), ParamType, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UMaterial* Material = LoadOrCreateMaterial(MaterialPath, false, LoadError);
	if (!Material)
	{
		return FMCPToolResult::Error(LoadError);
	}

	ParamType = ParamType.ToLower();
	int32 NodeY = Material->GetExpressions().Num() * 80; // Stack nodes vertically

	if (ParamType == TEXT("scalar"))
	{
		UMaterialExpressionScalarParameter* Param = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), -500, NodeY)
		);
		if (Param)
		{
			Param->ParameterName = FName(*ParamName);
			if (Params->HasField(TEXT("default_value")))
			{
				Param->DefaultValue = Params->GetNumberField(TEXT("default_value"));
			}
		}
	}
	else if (ParamType == TEXT("vector"))
	{
		UMaterialExpressionVectorParameter* Param = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), -500, NodeY)
		);
		if (Param)
		{
			Param->ParameterName = FName(*ParamName);
			const TSharedPtr<FJsonObject>* DefaultObj;
			if (Params->TryGetObjectField(TEXT("default_value"), DefaultObj))
			{
				FLinearColor Color(1, 1, 1, 1);
				(*DefaultObj)->TryGetNumberField(TEXT("r"), Color.R);
				(*DefaultObj)->TryGetNumberField(TEXT("g"), Color.G);
				(*DefaultObj)->TryGetNumberField(TEXT("b"), Color.B);
				(*DefaultObj)->TryGetNumberField(TEXT("a"), Color.A);
				Param->DefaultValue = Color;
			}
		}
	}
	else
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Unknown param_type: %s. Use 'scalar' or 'vector'"), *ParamType));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("material_path"), MaterialPath);
	ResultData->SetStringField(TEXT("param_name"), ParamName);
	ResultData->SetStringField(TEXT("param_type"), ParamType);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added %s parameter: %s"), *ParamType, *ParamName),
		ResultData
	);
}

FMCPToolResult FMCPTool_MaterialCreate::ExecuteWireToCustom(const TSharedRef<FJsonObject>& Params)
{
	FString MaterialPath, ParamName, CustomInput;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("param_name"), ParamName, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("custom_input"), CustomInput, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UMaterial* Material = LoadOrCreateMaterial(MaterialPath, false, LoadError);
	if (!Material)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Find parameter node
	UMaterialExpression* ParamNode = FindParameterNode(Material, ParamName);
	if (!ParamNode)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Parameter '%s' not found in material"), *ParamName));
	}

	// Find Custom node (first one, or by description if specified)
	FString CustomDesc = Params->HasField(TEXT("description")) ? Params->GetStringField(TEXT("description")) : TEXT("");
	UMaterialExpressionCustom* CustomNode = FindCustomNode(Material, CustomDesc);
	if (!CustomNode)
	{
		return FMCPToolResult::Error(TEXT("No Custom node found in material"));
	}

	// Wire connection
	bool bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(ParamNode, TEXT(""), CustomNode, CustomInput);
	if (!bConnected)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to connect %s to Custom input %s"), *ParamName, *CustomInput));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("from"), ParamName);
	ResultData->SetStringField(TEXT("to"), CustomInput);
	ResultData->SetBoolField(TEXT("connected"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Wired %s -> Custom.%s"), *ParamName, *CustomInput),
		ResultData
	);
}

FMCPToolResult FMCPTool_MaterialCreate::ExecuteConnectOutput(const TSharedRef<FJsonObject>& Params)
{
	FString MaterialPath, OutputPin;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("output_pin"), OutputPin, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UMaterial* Material = LoadOrCreateMaterial(MaterialPath, false, LoadError);
	if (!Material)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Find Custom node
	FString CustomDesc = Params->HasField(TEXT("description")) ? Params->GetStringField(TEXT("description")) : TEXT("");
	UMaterialExpressionCustom* CustomNode = FindCustomNode(Material, CustomDesc);
	if (!CustomNode)
	{
		return FMCPToolResult::Error(TEXT("No Custom node found in material"));
	}

	// Map output pin name to enum
	OutputPin = OutputPin.ToLower();
	EMaterialProperty MatProp = MP_EmissiveColor; // Default

	if (OutputPin == TEXT("emissive") || OutputPin == TEXT("emissive_color"))
	{
		MatProp = MP_EmissiveColor;
	}
	else if (OutputPin == TEXT("base_color") || OutputPin == TEXT("basecolor"))
	{
		MatProp = MP_BaseColor;
	}
	else if (OutputPin == TEXT("opacity"))
	{
		MatProp = MP_Opacity;
	}
	else if (OutputPin == TEXT("opacity_mask"))
	{
		MatProp = MP_OpacityMask;
	}
	else if (OutputPin == TEXT("metallic"))
	{
		MatProp = MP_Metallic;
	}
	else if (OutputPin == TEXT("roughness"))
	{
		MatProp = MP_Roughness;
	}
	else if (OutputPin == TEXT("normal"))
	{
		MatProp = MP_Normal;
	}

	bool bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(CustomNode, TEXT(""), MatProp);
	if (!bConnected)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to connect Custom to %s"), *OutputPin));
	}

	UMaterialEditingLibrary::RecompileMaterial(Material);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("output_pin"), OutputPin);
	ResultData->SetBoolField(TEXT("connected"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Connected Custom node to %s"), *OutputPin),
		ResultData
	);
}

FMCPToolResult FMCPTool_MaterialCreate::ExecuteSetCustomCode(const TSharedRef<FJsonObject>& Params)
{
	FString MaterialPath, Code;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("code"), Code, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UMaterial* Material = LoadOrCreateMaterial(MaterialPath, false, LoadError);
	if (!Material)
	{
		return FMCPToolResult::Error(LoadError);
	}

	FString CustomDesc = Params->HasField(TEXT("description")) ? Params->GetStringField(TEXT("description")) : TEXT("");
	UMaterialExpressionCustom* CustomNode = FindCustomNode(Material, CustomDesc);
	if (!CustomNode)
	{
		return FMCPToolResult::Error(TEXT("No Custom node found in material"));
	}

	CustomNode->Code = Code;
	UMaterialEditingLibrary::RecompileMaterial(Material);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("material_path"), MaterialPath);
	ResultData->SetNumberField(TEXT("code_length"), Code.Len());

	return FMCPToolResult::Success(TEXT("Updated Custom node code"), ResultData);
}

FMCPToolResult FMCPTool_MaterialCreate::ExecuteRecompileMaterial(const TSharedRef<FJsonObject>& Params)
{
	FString MaterialPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UMaterial* Material = LoadOrCreateMaterial(MaterialPath, false, LoadError);
	if (!Material)
	{
		return FMCPToolResult::Error(LoadError);
	}

	UMaterialEditingLibrary::RecompileMaterial(Material);
	UEditorAssetLibrary::SaveAsset(MaterialPath);

	return FMCPToolResult::Success(FString::Printf(TEXT("Recompiled material: %s"), *MaterialPath));
}

FMCPToolResult FMCPTool_MaterialCreate::ExecuteSetupCoronaMaterial(const TSharedRef<FJsonObject>& Params)
{
	FString MaterialPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}

	// Read shader file if specified
	FString Code = TEXT("return float3(1,0,1);"); // Default pink for error
	if (Params->HasField(TEXT("shader_file")))
	{
		FString ShaderFile = Params->GetStringField(TEXT("shader_file"));
		if (FFileHelper::LoadFileToString(Code, *ShaderFile))
		{
			UE_LOG(LogUnrealClaude, Log, TEXT("Loaded shader code from: %s"), *ShaderFile);
		}
		else
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to load shader file: %s"), *ShaderFile);
		}
	}
	else if (Params->HasField(TEXT("code")))
	{
		Code = Params->GetStringField(TEXT("code"));
	}

	// Delete and recreate material
	if (UEditorAssetLibrary::DoesAssetExist(MaterialPath))
	{
		UEditorAssetLibrary::DeleteAsset(MaterialPath);
	}

	FString PackagePath, AssetName;
	MaterialPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	UPackage* Package = CreatePackage(*MaterialPath);
	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UMaterial* Material = Cast<UMaterial>(Factory->FactoryCreateNew(
		UMaterial::StaticClass(), Package, FName(*AssetName),
		RF_Public | RF_Standalone, nullptr, GWarn
	));

	if (!Material)
	{
		return FMCPToolResult::Error(TEXT("Failed to create material"));
	}

	// Set material properties
	Material->BlendMode = BLEND_Additive;
	Material->SetShadingModel(MSM_Unlit);
	Material->TwoSided = true;

	// Create parameters and Custom node
	TArray<FString> ParamNames;
	const TArray<TSharedPtr<FJsonValue>>* ParametersArray;
	if (Params->TryGetArrayField(TEXT("parameters"), ParametersArray))
	{
		int32 YPos = 0;
		for (const TSharedPtr<FJsonValue>& ParamVal : *ParametersArray)
		{
			const TSharedPtr<FJsonObject>* ParamObj;
			if (ParamVal->TryGetObject(ParamObj))
			{
				FString PName = (*ParamObj)->GetStringField(TEXT("name"));
				FString PType = (*ParamObj)->GetStringField(TEXT("type")).ToLower();
				ParamNames.Add(PName);

				if (PType == TEXT("scalar"))
				{
					UMaterialExpressionScalarParameter* Param = Cast<UMaterialExpressionScalarParameter>(
						UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), -600, YPos)
					);
					if (Param)
					{
						Param->ParameterName = FName(*PName);
						if ((*ParamObj)->HasField(TEXT("default")))
						{
							Param->DefaultValue = (*ParamObj)->GetNumberField(TEXT("default"));
						}
					}
				}
				else if (PType == TEXT("vector"))
				{
					UMaterialExpressionVectorParameter* Param = Cast<UMaterialExpressionVectorParameter>(
						UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), -600, YPos)
					);
					if (Param)
					{
						Param->ParameterName = FName(*PName);
						const TSharedPtr<FJsonObject>* DefObj;
						if ((*ParamObj)->TryGetObjectField(TEXT("default"), DefObj))
						{
							FLinearColor Color(1, 1, 1, 1);
							(*DefObj)->TryGetNumberField(TEXT("r"), Color.R);
							(*DefObj)->TryGetNumberField(TEXT("g"), Color.G);
							(*DefObj)->TryGetNumberField(TEXT("b"), Color.B);
							(*DefObj)->TryGetNumberField(TEXT("a"), Color.A);
							Param->DefaultValue = Color;
						}
					}
				}
				YPos += 80;
			}
		}
	}

	// Create Custom node
	UMaterialExpressionCustom* CustomNode = Cast<UMaterialExpressionCustom>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionCustom::StaticClass(), -200, 100)
	);

	if (CustomNode)
	{
		CustomNode->Code = Code;
		CustomNode->Description = TEXT("Corona Shader");
		CustomNode->OutputType = CMOT_Float3;

		// Add inputs matching parameters
		CustomNode->Inputs.Empty();
		for (const FString& PName : ParamNames)
		{
			FCustomInput NewInput;
			NewInput.InputName = FName(*PName);
			CustomNode->Inputs.Add(NewInput);
		}

		// Wire parameters to Custom inputs
		for (const FString& PName : ParamNames)
		{
			UMaterialExpression* ParamNode = FindParameterNode(Material, PName);
			if (ParamNode)
			{
				UMaterialEditingLibrary::ConnectMaterialExpressions(ParamNode, TEXT(""), CustomNode, PName);
			}
		}

		// Connect to Emissive
		UMaterialEditingLibrary::ConnectMaterialProperty(CustomNode, TEXT(""), MP_EmissiveColor);
	}

	// Recompile and save
	UMaterialEditingLibrary::RecompileMaterial(Material);
	FAssetRegistryModule::AssetCreated(Material);
	Package->MarkPackageDirty();

	FString PackageFileName = FPackageName::LongPackageNameToFilename(MaterialPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::Save(Package, Material, *PackageFileName, SaveArgs);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("material_path"), MaterialPath);
	ResultData->SetNumberField(TEXT("parameter_count"), ParamNames.Num());
	ResultData->SetBoolField(TEXT("custom_node_created"), CustomNode != nullptr);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created corona material with %d parameters"), ParamNames.Num()),
		ResultData
	);
}

// Helper implementations

UMaterial* FMCPTool_MaterialCreate::LoadOrCreateMaterial(const FString& AssetPath, bool bCreateIfMissing, FString& OutError)
{
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *AssetPath);
	if (!Material)
	{
		OutError = FString::Printf(TEXT("Failed to load material: %s"), *AssetPath);
	}
	return Material;
}

UMaterialExpressionCustom* FMCPTool_MaterialCreate::FindCustomNode(UMaterial* Material, const FString& NodeDesc)
{
	if (!Material) return nullptr;

	for (UMaterialExpression* Expr : Material->GetExpressions())
	{
		if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
		{
			if (NodeDesc.IsEmpty() || Custom->Description.Contains(NodeDesc))
			{
				return Custom;
			}
		}
	}
	return nullptr;
}

UMaterialExpression* FMCPTool_MaterialCreate::FindParameterNode(UMaterial* Material, const FString& ParamName)
{
	if (!Material) return nullptr;

	for (UMaterialExpression* Expr : Material->GetExpressions())
	{
		if (UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expr))
		{
			if (Scalar->ParameterName.ToString() == ParamName)
			{
				return Scalar;
			}
		}
		else if (UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expr))
		{
			if (Vector->ParameterName.ToString() == ParamName)
			{
				return Vector;
			}
		}
	}
	return nullptr;
}

bool FMCPTool_MaterialCreate::SetMaterialBlendMode(UMaterial* Material, const FString& BlendMode, FString& OutError)
{
	if (BlendMode == TEXT("opaque"))
	{
		Material->BlendMode = BLEND_Opaque;
	}
	else if (BlendMode == TEXT("masked"))
	{
		Material->BlendMode = BLEND_Masked;
	}
	else if (BlendMode == TEXT("translucent"))
	{
		Material->BlendMode = BLEND_Translucent;
	}
	else if (BlendMode == TEXT("additive"))
	{
		Material->BlendMode = BLEND_Additive;
	}
	else if (BlendMode == TEXT("modulate"))
	{
		Material->BlendMode = BLEND_Modulate;
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown blend mode: %s"), *BlendMode);
		return false;
	}
	return true;
}

bool FMCPTool_MaterialCreate::SetMaterialShadingModel(UMaterial* Material, const FString& ShadingModel, FString& OutError)
{
	if (ShadingModel == TEXT("unlit"))
	{
		Material->SetShadingModel(MSM_Unlit);
	}
	else if (ShadingModel == TEXT("default_lit") || ShadingModel == TEXT("lit"))
	{
		Material->SetShadingModel(MSM_DefaultLit);
	}
	else if (ShadingModel == TEXT("subsurface"))
	{
		Material->SetShadingModel(MSM_Subsurface);
	}
	else if (ShadingModel == TEXT("preintegrated_skin"))
	{
		Material->SetShadingModel(MSM_PreintegratedSkin);
	}
	else if (ShadingModel == TEXT("clear_coat"))
	{
		Material->SetShadingModel(MSM_ClearCoat);
	}
	else if (ShadingModel == TEXT("subsurface_profile"))
	{
		Material->SetShadingModel(MSM_SubsurfaceProfile);
	}
	else if (ShadingModel == TEXT("two_sided_foliage"))
	{
		Material->SetShadingModel(MSM_TwoSidedFoliage);
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown shading model: %s"), *ShadingModel);
		return false;
	}
	return true;
}
