// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Asset.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "UObject/PropertyAccessUtil.h"
#include "Engine/SkeletalMesh.h"
#include "Dom/JsonValue.h"

FMCPToolInfo FMCPTool_Asset::GetInfo() const
{
	FMCPToolInfo Info;
	Info.Name = TEXT("asset");
	Info.Description = TEXT("Generic asset operations: set properties, save, and query assets in Content Browser");

	// Parameters
	Info.Parameters.Add(FMCPToolParameter(TEXT("operation"), TEXT("string"),
		TEXT("Operation: set_asset_property, save_asset, get_asset_info, list_assets"), true));

	// Common params
	Info.Parameters.Add(FMCPToolParameter(TEXT("asset_path"), TEXT("string"),
		TEXT("Asset path (e.g., /Game/Characters/MyMesh)"), false));

	// set_asset_property params
	Info.Parameters.Add(FMCPToolParameter(TEXT("property"), TEXT("string"),
		TEXT("Property path (e.g., Materials.0.MaterialInterface, bEnableGravity)"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("value"), TEXT("any"),
		TEXT("Value to set (type must match property type)"), false));

	// save_asset params
	Info.Parameters.Add(FMCPToolParameter(TEXT("save"), TEXT("boolean"),
		TEXT("Actually save to disk (default: true)"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("mark_dirty"), TEXT("boolean"),
		TEXT("Mark the asset as dirty (default: true if save is false)"), false));

	// get_asset_info params
	Info.Parameters.Add(FMCPToolParameter(TEXT("include_properties"), TEXT("boolean"),
		TEXT("Include editable property list (default: false)"), false));

	// list_assets params
	Info.Parameters.Add(FMCPToolParameter(TEXT("directory"), TEXT("string"),
		TEXT("Directory to list (e.g., /Game/Characters/)"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("class_filter"), TEXT("string"),
		TEXT("Filter by class name (e.g., SkeletalMesh, StaticMesh, Material)"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("recursive"), TEXT("boolean"),
		TEXT("Search recursively (default: false)"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("limit"), TEXT("integer"),
		TEXT("Maximum results (1-1000, default: 25)"), false));

	Info.Annotations = FMCPToolAnnotations::Modifying();

	return Info;
}

FMCPToolResult FMCPTool_Asset::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	if (Operation == TEXT("set_asset_property"))
	{
		return ExecuteSetAssetProperty(Params);
	}
	else if (Operation == TEXT("save_asset"))
	{
		return ExecuteSaveAsset(Params);
	}
	else if (Operation == TEXT("get_asset_info"))
	{
		return ExecuteGetAssetInfo(Params);
	}
	else if (Operation == TEXT("list_assets"))
	{
		return ExecuteListAssets(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: set_asset_property, save_asset, get_asset_info, list_assets"),
		*Operation));
}

FMCPToolResult FMCPTool_Asset::ExecuteSetAssetProperty(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath;
	FString PropertyPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("property"), PropertyPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(AssetPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidatePropertyPathParam(PropertyPath, Error))
	{
		return Error.GetValue();
	}

	if (!Params->HasField(TEXT("value")))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: value"));
	}
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));

	// Load the asset
	FString LoadError;
	UObject* Asset = LoadAssetByPath(AssetPath, LoadError);
	if (!Asset)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Set the property
	FString PropertyError;
	if (!SetPropertyFromJson(Asset, PropertyPath, Value, PropertyError))
	{
		return FMCPToolResult::Error(PropertyError);
	}

	// Mark dirty and notify
	Asset->PostEditChange();
	Asset->MarkPackageDirty();

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetStringField(TEXT("property"), PropertyPath);
	ResultData->SetBoolField(TEXT("modified"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set property '%s' on asset '%s'"), *PropertyPath, *Asset->GetName()),
		ResultData
	);
}

FMCPToolResult FMCPTool_Asset::ExecuteSaveAsset(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(AssetPath, Error))
	{
		return Error.GetValue();
	}

	// Get options
	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}

	bool bMarkDirty = !bSave; // Default to marking dirty if not saving
	if (Params->HasField(TEXT("mark_dirty")))
	{
		bMarkDirty = Params->GetBoolField(TEXT("mark_dirty"));
	}

	// Load the asset
	FString LoadError;
	UObject* Asset = LoadAssetByPath(AssetPath, LoadError);
	if (!Asset)
	{
		return FMCPToolResult::Error(LoadError);
	}

	bool bWasSaved = false;
	bool bWasMarkedDirty = false;

	// Mark dirty if requested
	if (bMarkDirty)
	{
		Asset->MarkPackageDirty();
		bWasMarkedDirty = true;
	}

	// Save if requested
	if (bSave)
	{
		UPackage* Package = Asset->GetOutermost();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension()
		);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		FSavePackageResultStruct SaveResult = UPackage::Save(Package, Asset, *PackageFileName, SaveArgs);

		bWasSaved = SaveResult.IsSuccessful();
		if (!bWasSaved)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Failed to save asset: %s"), *AssetPath));
		}
	}

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetBoolField(TEXT("saved"), bWasSaved);
	ResultData->SetBoolField(TEXT("marked_dirty"), bWasMarkedDirty);

	FString Message = bWasSaved
		? FString::Printf(TEXT("Saved asset: %s"), *AssetPath)
		: FString::Printf(TEXT("Marked asset dirty: %s"), *AssetPath);

	return FMCPToolResult::Success(Message, ResultData);
}

FMCPToolResult FMCPTool_Asset::ExecuteGetAssetInfo(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(AssetPath, Error))
	{
		return Error.GetValue();
	}

	bool bIncludeProperties = false;
	if (Params->HasField(TEXT("include_properties")))
	{
		bIncludeProperties = Params->GetBoolField(TEXT("include_properties"));
	}

	// Load the asset
	FString LoadError;
	UObject* Asset = LoadAssetByPath(AssetPath, LoadError);
	if (!Asset)
	{
		return FMCPToolResult::Error(LoadError);
	}

	TSharedPtr<FJsonObject> ResultData = BuildAssetInfoJson(Asset);

	// Add properties if requested
	if (bIncludeProperties)
	{
		ResultData->SetArrayField(TEXT("properties"), GetAssetProperties(Asset, true));
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Asset info: %s"), *Asset->GetName()),
		ResultData
	);
}

FMCPToolResult FMCPTool_Asset::ExecuteListAssets(const TSharedRef<FJsonObject>& Params)
{
	FString Directory = TEXT("/Game/");
	if (Params->HasField(TEXT("directory")))
	{
		Directory = Params->GetStringField(TEXT("directory"));
	}

	TOptional<FMCPToolResult> Error;
	if (!ValidateBlueprintPathParam(Directory, Error))
	{
		return Error.GetValue();
	}

	FString ClassFilter;
	if (Params->HasField(TEXT("class_filter")))
	{
		ClassFilter = Params->GetStringField(TEXT("class_filter"));
	}

	bool bRecursive = false;
	if (Params->HasField(TEXT("recursive")))
	{
		bRecursive = Params->GetBoolField(TEXT("recursive"));
	}

	int32 Limit = 25;
	if (Params->HasField(TEXT("limit")))
	{
		Limit = FMath::Clamp(Params->GetIntegerField(TEXT("limit")), 1, 1000);
	}

	// Query asset registry
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(FName(*Directory), Assets, bRecursive);

	// Filter and build results
	TArray<TSharedPtr<FJsonValue>> ResultArray;
	int32 Count = 0;

	for (const FAssetData& AssetData : Assets)
	{
		if (Count >= Limit)
		{
			break;
		}

		// Apply class filter if specified
		if (!ClassFilter.IsEmpty())
		{
			FString AssetClassName = AssetData.AssetClassPath.GetAssetName().ToString();
			if (!AssetClassName.Contains(ClassFilter))
			{
				continue;
			}
		}

		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
		AssetObj->SetStringField(TEXT("package"), AssetData.PackageName.ToString());

		ResultArray.Add(MakeShared<FJsonValueObject>(AssetObj));
		Count++;
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("directory"), Directory);
	ResultData->SetNumberField(TEXT("count"), Count);
	ResultData->SetNumberField(TEXT("total_found"), Assets.Num());
	ResultData->SetArrayField(TEXT("assets"), ResultArray);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d assets in %s"), Count, *Directory),
		ResultData
	);
}

UObject* FMCPTool_Asset::LoadAssetByPath(const FString& AssetPath, FString& OutError)
{
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		// Try with _C suffix for Blueprint classes
		if (!AssetPath.EndsWith(TEXT("_C")))
		{
			Asset = LoadObject<UObject>(nullptr, *(AssetPath + TEXT("_C")));
		}
	}

	if (!Asset)
	{
		OutError = FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath);
	}

	return Asset;
}

bool FMCPTool_Asset::NavigateToProperty(
	UObject* StartObject,
	const TArray<FString>& PathParts,
	UObject*& OutObject,
	FProperty*& OutProperty,
	FString& OutError)
{
	OutObject = StartObject;
	OutProperty = nullptr;

	for (int32 i = 0; i < PathParts.Num(); ++i)
	{
		const FString& PartName = PathParts[i];
		const bool bIsLastPart = (i == PathParts.Num() - 1);

		// Check for array index notation (e.g., "Materials.0")
		int32 ArrayIndex = INDEX_NONE;
		FString PropertyName = PartName;

		// Check if this part is a numeric index
		if (PartName.IsNumeric())
		{
			ArrayIndex = FCString::Atoi(*PartName);
			// The previous property should be an array
			if (!OutProperty)
			{
				OutError = FString::Printf(TEXT("Cannot index without preceding array property"));
				return false;
			}

			FArrayProperty* ArrayProp = CastField<FArrayProperty>(OutProperty);
			if (!ArrayProp)
			{
				OutError = FString::Printf(TEXT("Property is not an array, cannot use index"));
				return false;
			}

			// Navigate into array element
			FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(OutObject));
			if (ArrayIndex < 0 || ArrayIndex >= ArrayHelper.Num())
			{
				OutError = FString::Printf(TEXT("Array index %d out of bounds (size: %d)"), ArrayIndex, ArrayHelper.Num());
				return false;
			}

			// Get the inner property
			FProperty* InnerProp = ArrayProp->Inner;
			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(InnerProp))
			{
				void* ElementPtr = ArrayHelper.GetRawPtr(ArrayIndex);
				UObject* ElementObj = ObjProp->GetObjectPropertyValue(ElementPtr);
				if (ElementObj && !bIsLastPart)
				{
					OutObject = ElementObj;
					OutProperty = nullptr;
					continue;
				}
			}

			if (bIsLastPart)
			{
				// Return the inner property for setting
				OutProperty = InnerProp;
				return true;
			}
			continue;
		}

		// Find the property
		OutProperty = OutObject->GetClass()->FindPropertyByName(FName(*PropertyName));

		if (!OutProperty)
		{
			OutError = FString::Printf(TEXT("Property not found: %s on %s"), *PropertyName, *OutObject->GetClass()->GetName());
			return false;
		}

		// If not the last part, navigate into nested object
		if (!bIsLastPart)
		{
			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(OutProperty))
			{
				// Keep the array property for next iteration (index access)
				continue;
			}

			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(OutProperty))
			{
				UObject* NestedObj = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(OutObject));
				if (!NestedObj)
				{
					OutError = FString::Printf(TEXT("Nested object is null: %s"), *PropertyName);
					return false;
				}
				OutObject = NestedObj;
				OutProperty = nullptr;
			}
			else if (FStructProperty* StructProp = CastField<FStructProperty>(OutProperty))
			{
				// For structs, we need to handle differently - keep the struct property
				// This is a limitation: we can only set entire structs, not nested struct members
				OutError = FString::Printf(TEXT("Cannot navigate into struct property: %s. Set the entire struct instead."), *PropertyName);
				return false;
			}
			else
			{
				OutError = FString::Printf(TEXT("Cannot navigate into property type: %s"), *PropertyName);
				return false;
			}
		}
	}

	return OutProperty != nullptr;
}

bool FMCPTool_Asset::SetPropertyFromJson(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	if (!Object || !Value.IsValid())
	{
		OutError = TEXT("Invalid object or value");
		return false;
	}

	// Parse property path
	TArray<FString> PathParts;
	PropertyPath.ParseIntoArray(PathParts, TEXT("."), true);

	UObject* TargetObject = nullptr;
	FProperty* Property = nullptr;

	if (!NavigateToProperty(Object, PathParts, TargetObject, Property, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Property not found: %s"), *PropertyPath);
		}
		return false;
	}

	// Get property address
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);

	// Handle object property (for setting references like materials)
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		return SetObjectPropertyValue(ObjProp, ValuePtr, Value, OutError);
	}

	// Try numeric property
	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (SetNumericPropertyValue(NumProp, ValuePtr, Value))
		{
			return true;
		}
	}
	// Try bool property
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolVal = false;
		if (Value->TryGetBool(BoolVal))
		{
			BoolProp->SetPropertyValue(ValuePtr, BoolVal);
			return true;
		}
	}
	// Try string property
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			StrProp->SetPropertyValue(ValuePtr, StrVal);
			return true;
		}
	}
	// Try name property
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*StrVal));
			return true;
		}
	}
	// Try struct property
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (SetStructPropertyValue(StructProp, ValuePtr, Value))
		{
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Unsupported property type for: %s"), *PropertyPath);
	return false;
}

bool FMCPTool_Asset::SetNumericPropertyValue(FNumericProperty* NumProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value)
{
	if (NumProp->IsFloatingPoint())
	{
		double DoubleVal = 0.0;
		if (Value->TryGetNumber(DoubleVal))
		{
			NumProp->SetFloatingPointPropertyValue(ValuePtr, DoubleVal);
			return true;
		}
	}
	else if (NumProp->IsInteger())
	{
		int64 IntVal = 0;
		if (Value->TryGetNumber(IntVal))
		{
			NumProp->SetIntPropertyValue(ValuePtr, IntVal);
			return true;
		}
	}
	return false;
}

bool FMCPTool_Asset::SetStructPropertyValue(FStructProperty* StructProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value)
{
	const TSharedPtr<FJsonObject>* ObjVal;
	if (!Value->TryGetObject(ObjVal))
	{
		return false;
	}

	if (StructProp->Struct == TBaseStructure<FVector>::Get())
	{
		FVector Vec;
		(*ObjVal)->TryGetNumberField(TEXT("x"), Vec.X);
		(*ObjVal)->TryGetNumberField(TEXT("y"), Vec.Y);
		(*ObjVal)->TryGetNumberField(TEXT("z"), Vec.Z);
		*reinterpret_cast<FVector*>(ValuePtr) = Vec;
		return true;
	}

	if (StructProp->Struct == TBaseStructure<FRotator>::Get())
	{
		FRotator Rot;
		(*ObjVal)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
		(*ObjVal)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
		(*ObjVal)->TryGetNumberField(TEXT("roll"), Rot.Roll);
		*reinterpret_cast<FRotator*>(ValuePtr) = Rot;
		return true;
	}

	if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
	{
		FLinearColor Color;
		(*ObjVal)->TryGetNumberField(TEXT("r"), Color.R);
		(*ObjVal)->TryGetNumberField(TEXT("g"), Color.G);
		(*ObjVal)->TryGetNumberField(TEXT("b"), Color.B);
		(*ObjVal)->TryGetNumberField(TEXT("a"), Color.A);
		*reinterpret_cast<FLinearColor*>(ValuePtr) = Color;
		return true;
	}

	return false;
}

bool FMCPTool_Asset::SetObjectPropertyValue(FObjectProperty* ObjProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	// Value should be a string path to the object
	FString ObjectPath;
	if (!Value->TryGetString(ObjectPath))
	{
		OutError = TEXT("Object property value must be a string path");
		return false;
	}

	// Handle "None" or empty as null
	if (ObjectPath.IsEmpty() || ObjectPath.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
		return true;
	}

	// Load the referenced object
	UObject* ReferencedObject = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!ReferencedObject)
	{
		OutError = FString::Printf(TEXT("Failed to load object: %s"), *ObjectPath);
		return false;
	}

	// Verify type compatibility
	if (!ReferencedObject->IsA(ObjProp->PropertyClass))
	{
		OutError = FString::Printf(TEXT("Object type mismatch. Expected %s, got %s"),
			*ObjProp->PropertyClass->GetName(), *ReferencedObject->GetClass()->GetName());
		return false;
	}

	ObjProp->SetObjectPropertyValue(ValuePtr, ReferencedObject);
	return true;
}

TSharedPtr<FJsonObject> FMCPTool_Asset::BuildAssetInfoJson(UObject* Asset)
{
	TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();

	Info->SetStringField(TEXT("name"), Asset->GetName());
	Info->SetStringField(TEXT("path"), Asset->GetPathName());
	Info->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
	Info->SetStringField(TEXT("package"), Asset->GetOutermost()->GetName());

	// Check dirty state
	UPackage* Package = Asset->GetOutermost();
	Info->SetBoolField(TEXT("is_dirty"), Package->IsDirty());

	// Add class-specific info
	if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Asset))
	{
		TArray<TSharedPtr<FJsonValue>> MaterialsArr;
		const TArray<FSkeletalMaterial>& Materials = SkelMesh->GetMaterials();
		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			TSharedPtr<FJsonObject> MatObj = MakeShared<FJsonObject>();
			MatObj->SetNumberField(TEXT("slot"), i);
			MatObj->SetStringField(TEXT("slot_name"), Materials[i].MaterialSlotName.ToString());
			MatObj->SetStringField(TEXT("material"),
				Materials[i].MaterialInterface ? Materials[i].MaterialInterface->GetPathName() : TEXT("None"));
			MaterialsArr.Add(MakeShared<FJsonValueObject>(MatObj));
		}
		Info->SetArrayField(TEXT("materials"), MaterialsArr);
	}

	return Info;
}

TArray<TSharedPtr<FJsonValue>> FMCPTool_Asset::GetAssetProperties(UObject* Asset, bool bEditableOnly)
{
	TArray<TSharedPtr<FJsonValue>> PropsArray;

	for (TFieldIterator<FProperty> PropIt(Asset->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Skip non-editable if requested
		if (bEditableOnly && !Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());

		// Determine type string
		FString TypeStr;
		if (CastField<FNumericProperty>(Property))
		{
			TypeStr = Property->IsA<FFloatProperty>() || Property->IsA<FDoubleProperty>()
				? TEXT("float") : TEXT("integer");
		}
		else if (CastField<FBoolProperty>(Property))
		{
			TypeStr = TEXT("bool");
		}
		else if (CastField<FStrProperty>(Property))
		{
			TypeStr = TEXT("string");
		}
		else if (CastField<FNameProperty>(Property))
		{
			TypeStr = TEXT("name");
		}
		else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			TypeStr = FString::Printf(TEXT("struct:%s"), *StructProp->Struct->GetName());
		}
		else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
		{
			TypeStr = FString::Printf(TEXT("object:%s"), *ObjProp->PropertyClass->GetName());
		}
		else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			TypeStr = TEXT("array");
		}
		else
		{
			TypeStr = TEXT("other");
		}

		PropObj->SetStringField(TEXT("type"), TypeStr);
		PropObj->SetBoolField(TEXT("editable"), Property->HasAnyPropertyFlags(CPF_Edit));

		PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}

	return PropsArray;
}
