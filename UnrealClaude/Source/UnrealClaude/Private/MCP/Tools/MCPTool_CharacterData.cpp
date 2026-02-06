// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_CharacterData.h"
#include "CharacterDataTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DataTable.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Engine/World.h"
#include "EngineUtils.h"

FMCPToolResult FMCPTool_CharacterData::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	// DataAsset operations
	if (Operation == TEXT("create_character_data"))
	{
		return ExecuteCreateCharacterData(Params);
	}
	else if (Operation == TEXT("query_character_data"))
	{
		return ExecuteQueryCharacterData(Params);
	}
	else if (Operation == TEXT("get_character_data"))
	{
		return ExecuteGetCharacterData(Params);
	}
	else if (Operation == TEXT("update_character_data"))
	{
		return ExecuteUpdateCharacterData(Params);
	}
	// DataTable operations
	else if (Operation == TEXT("create_stats_table"))
	{
		return ExecuteCreateStatsTable(Params);
	}
	else if (Operation == TEXT("query_stats_table"))
	{
		return ExecuteQueryStatsTable(Params);
	}
	else if (Operation == TEXT("add_stats_row"))
	{
		return ExecuteAddStatsRow(Params);
	}
	else if (Operation == TEXT("update_stats_row"))
	{
		return ExecuteUpdateStatsRow(Params);
	}
	else if (Operation == TEXT("remove_stats_row"))
	{
		return ExecuteRemoveStatsRow(Params);
	}
	// Application
	else if (Operation == TEXT("apply_character_data"))
	{
		return ExecuteApplyCharacterData(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid operations: create_character_data, query_character_data, get_character_data, ")
		TEXT("update_character_data, create_stats_table, query_stats_table, add_stats_row, update_stats_row, ")
		TEXT("remove_stats_row, apply_character_data"),
		*Operation));
}

FMCPToolResult FMCPTool_CharacterData::ExecuteCreateCharacterData(const TSharedRef<FJsonObject>& Params)
{
	FString AssetName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("asset_name"), AssetName, Error))
	{
		return Error.GetValue();
	}

	FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"), TEXT("/Game/Characters"));

	// Validate paths
	if (!ValidateBlueprintPathParam(PackagePath, Error))
	{
		return Error.GetValue();
	}

	// Build full package path
	FString FullPackagePath = PackagePath / AssetName;

	// Create package
	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *FullPackagePath));
	}

	// Create the DataAsset
	UCharacterConfigDataAsset* Config = NewObject<UCharacterConfigDataAsset>(
		Package, FName(*AssetName), RF_Public | RF_Standalone);

	if (!Config)
	{
		return FMCPToolResult::Error(TEXT("Failed to create UCharacterConfigDataAsset"));
	}

	// Populate from params
	PopulateConfigFromParams(Config, Params);

	// Mark dirty and save
	Package->MarkPackageDirty();
	FString SaveError;
	if (!SaveAsset(Config, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(Config);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), FullPackagePath);
	ResultData->SetStringField(TEXT("asset_name"), AssetName);
	ResultData->SetObjectField(TEXT("config"), ConfigToJson(Config));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created character config: %s"), *FullPackagePath),
		ResultData);
}

FMCPToolResult FMCPTool_CharacterData::ExecuteQueryCharacterData(const TSharedRef<FJsonObject>& Params)
{
	FString SearchName = ExtractOptionalString(Params, TEXT("search_name"));
	int32 Limit = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("limit"), 25), 1, 1000);
	int32 Offset = FMath::Max(0, ExtractOptionalNumber<int32>(Params, TEXT("offset"), 0));

	// Get search tags if provided
	TArray<FString> SearchTags;
	const TArray<TSharedPtr<FJsonValue>>* TagsArray;
	if (Params->TryGetArrayField(TEXT("search_tags"), TagsArray))
	{
		for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
		{
			FString TagStr;
			if (TagValue->TryGetString(TagStr))
			{
				SearchTags.Add(TagStr);
			}
		}
	}

	// Query asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssetsByClass(UCharacterConfigDataAsset::StaticClass()->GetClassPathName(), AssetList);

	TArray<TSharedPtr<FJsonValue>> ResultArray;
	int32 TotalMatches = 0;
	int32 SkippedCount = 0;

	for (const FAssetData& AssetData : AssetList)
	{
		// Filter by name if specified
		if (!SearchName.IsEmpty())
		{
			FString AssetNameStr = AssetData.AssetName.ToString();
			if (!AssetNameStr.Contains(SearchName))
			{
				continue;
			}
		}

		// Filter by tags if specified (requires loading asset)
		if (SearchTags.Num() > 0)
		{
			UCharacterConfigDataAsset* Config = Cast<UCharacterConfigDataAsset>(AssetData.GetAsset());
			if (Config)
			{
				bool bHasAllTags = true;
				for (const FString& SearchTag : SearchTags)
				{
					bool bFound = false;
					for (const FName& Tag : Config->GameplayTags)
					{
						if (Tag.ToString().Contains(SearchTag))
						{
							bFound = true;
							break;
						}
					}
					if (!bFound)
					{
						bHasAllTags = false;
						break;
					}
				}
				if (!bHasAllTags)
				{
					continue;
				}
			}
		}

		TotalMatches++;

		// Apply pagination
		if (SkippedCount < Offset)
		{
			SkippedCount++;
			continue;
		}

		if (ResultArray.Num() >= Limit)
		{
			continue;
		}

		// Build result entry
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("asset_path"), AssetData.GetObjectPathString());
		Entry->SetStringField(TEXT("asset_name"), AssetData.AssetName.ToString());

		// Load to get details
		if (UCharacterConfigDataAsset* Config = Cast<UCharacterConfigDataAsset>(AssetData.GetAsset()))
		{
			Entry->SetStringField(TEXT("config_id"), Config->ConfigId.ToString());
			Entry->SetStringField(TEXT("display_name"), Config->DisplayName);
			Entry->SetBoolField(TEXT("is_player_character"), Config->bIsPlayerCharacter);
		}

		ResultArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("configs"), ResultArray);
	ResultData->SetNumberField(TEXT("count"), ResultArray.Num());
	ResultData->SetNumberField(TEXT("total"), TotalMatches);
	ResultData->SetNumberField(TEXT("offset"), Offset);
	ResultData->SetNumberField(TEXT("limit"), Limit);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d character configs (showing %d-%d of %d)"),
			TotalMatches, Offset + 1, Offset + ResultArray.Num(), TotalMatches),
		ResultData);
}

FMCPToolResult FMCPTool_CharacterData::ExecuteGetCharacterData(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UCharacterConfigDataAsset* Config = LoadCharacterConfig(AssetPath, LoadError);
	if (!Config)
	{
		return FMCPToolResult::Error(LoadError);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetObjectField(TEXT("config"), ConfigToJson(Config));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved character config: %s"), *Config->DisplayName),
		ResultData);
}

FMCPToolResult FMCPTool_CharacterData::ExecuteUpdateCharacterData(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UCharacterConfigDataAsset* Config = LoadCharacterConfig(AssetPath, LoadError);
	if (!Config)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Update from params
	PopulateConfigFromParams(Config, Params);

	// Save
	FString SaveError;
	if (!SaveAsset(Config, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetObjectField(TEXT("config"), ConfigToJson(Config));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Updated character config: %s"), *Config->DisplayName),
		ResultData);
}

FMCPToolResult FMCPTool_CharacterData::ExecuteCreateStatsTable(const TSharedRef<FJsonObject>& Params)
{
	FString AssetName;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("asset_name"), AssetName, Error))
	{
		return Error.GetValue();
	}

	FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"), TEXT("/Game/Characters"));

	if (!ValidateBlueprintPathParam(PackagePath, Error))
	{
		return Error.GetValue();
	}

	FString FullPackagePath = PackagePath / AssetName;

	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *FullPackagePath));
	}

	// Create DataTable with FCharacterStatsRow structure
	UDataTable* Table = NewObject<UDataTable>(
		Package, FName(*AssetName), RF_Public | RF_Standalone);

	if (!Table)
	{
		return FMCPToolResult::Error(TEXT("Failed to create UDataTable"));
	}

	// Set the row struct
	Table->RowStruct = FCharacterStatsRow::StaticStruct();

	Package->MarkPackageDirty();
	FString SaveError;
	if (!SaveAsset(Table, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	FAssetRegistryModule::AssetCreated(Table);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("table_path"), FullPackagePath);
	ResultData->SetStringField(TEXT("asset_name"), AssetName);
	ResultData->SetStringField(TEXT("row_struct"), TEXT("FCharacterStatsRow"));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created stats table: %s"), *FullPackagePath),
		ResultData);
}

FMCPToolResult FMCPTool_CharacterData::ExecuteQueryStatsTable(const TSharedRef<FJsonObject>& Params)
{
	FString TablePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("table_path"), TablePath, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UDataTable* Table = LoadStatsTable(TablePath, LoadError);
	if (!Table)
	{
		return FMCPToolResult::Error(LoadError);
	}

	FString RowNameFilter = ExtractOptionalString(Params, TEXT("row_name"));
	int32 Limit = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("limit"), 25), 1, 1000);
	int32 Offset = FMath::Max(0, ExtractOptionalNumber<int32>(Params, TEXT("offset"), 0));

	TArray<TSharedPtr<FJsonValue>> RowsArray;
	TArray<FName> RowNames = Table->GetRowNames();
	int32 TotalMatches = 0;
	int32 SkippedCount = 0;

	for (const FName& RowName : RowNames)
	{
		// Filter by row name if specified
		if (!RowNameFilter.IsEmpty() && !RowName.ToString().Contains(RowNameFilter))
		{
			continue;
		}

		FCharacterStatsRow* Row = Table->FindRow<FCharacterStatsRow>(RowName, TEXT(""));
		if (!Row)
		{
			continue;
		}

		TotalMatches++;

		if (SkippedCount < Offset)
		{
			SkippedCount++;
			continue;
		}

		if (RowsArray.Num() >= Limit)
		{
			continue;
		}

		RowsArray.Add(MakeShared<FJsonValueObject>(StatsRowToJson(*Row, RowName)));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("table_path"), TablePath);
	ResultData->SetArrayField(TEXT("rows"), RowsArray);
	ResultData->SetNumberField(TEXT("count"), RowsArray.Num());
	ResultData->SetNumberField(TEXT("total"), TotalMatches);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d rows in stats table"), TotalMatches),
		ResultData);
}

FMCPToolResult FMCPTool_CharacterData::ExecuteAddStatsRow(const TSharedRef<FJsonObject>& Params)
{
	FString TablePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("table_path"), TablePath, Error))
	{
		return Error.GetValue();
	}

	FString RowName;
	if (!ExtractRequiredString(Params, TEXT("row_name"), RowName, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UDataTable* Table = LoadStatsTable(TablePath, LoadError);
	if (!Table)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Check if row already exists
	if (Table->FindRow<FCharacterStatsRow>(FName(*RowName), TEXT("")))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Row '%s' already exists"), *RowName));
	}

	// Create new row
	FCharacterStatsRow NewRow;
	PopulateStatsRowFromParams(NewRow, Params);

	// Add to table
	Table->AddRow(FName(*RowName), NewRow);
	Table->MarkPackageDirty();

	FString SaveError;
	if (!SaveAsset(Table, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("table_path"), TablePath);
	ResultData->SetObjectField(TEXT("row"), StatsRowToJson(NewRow, FName(*RowName)));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added row '%s' to stats table"), *RowName),
		ResultData);
}

FMCPToolResult FMCPTool_CharacterData::ExecuteUpdateStatsRow(const TSharedRef<FJsonObject>& Params)
{
	FString TablePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("table_path"), TablePath, Error))
	{
		return Error.GetValue();
	}

	FString RowName;
	if (!ExtractRequiredString(Params, TEXT("row_name"), RowName, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UDataTable* Table = LoadStatsTable(TablePath, LoadError);
	if (!Table)
	{
		return FMCPToolResult::Error(LoadError);
	}

	FCharacterStatsRow* Row = Table->FindRow<FCharacterStatsRow>(FName(*RowName), TEXT(""));
	if (!Row)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Row '%s' not found"), *RowName));
	}

	// Update row from params
	PopulateStatsRowFromParams(*Row, Params);

	Table->MarkPackageDirty();
	FString SaveError;
	if (!SaveAsset(Table, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("table_path"), TablePath);
	ResultData->SetObjectField(TEXT("row"), StatsRowToJson(*Row, FName(*RowName)));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Updated row '%s' in stats table"), *RowName),
		ResultData);
}

FMCPToolResult FMCPTool_CharacterData::ExecuteRemoveStatsRow(const TSharedRef<FJsonObject>& Params)
{
	FString TablePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("table_path"), TablePath, Error))
	{
		return Error.GetValue();
	}

	FString RowName;
	if (!ExtractRequiredString(Params, TEXT("row_name"), RowName, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UDataTable* Table = LoadStatsTable(TablePath, LoadError);
	if (!Table)
	{
		return FMCPToolResult::Error(LoadError);
	}

	if (!Table->FindRow<FCharacterStatsRow>(FName(*RowName), TEXT("")))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Row '%s' not found"), *RowName));
	}

	Table->RemoveRow(FName(*RowName));
	Table->MarkPackageDirty();

	FString SaveError;
	if (!SaveAsset(Table, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("table_path"), TablePath);
	ResultData->SetStringField(TEXT("removed_row"), RowName);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Removed row '%s' from stats table"), *RowName),
		ResultData);
}

FMCPToolResult FMCPTool_CharacterData::ExecuteApplyCharacterData(const TSharedRef<FJsonObject>& Params)
{
	UWorld* World;
	TOptional<FMCPToolResult> Error;
	if (ValidateEditorContext(World).IsSet())
	{
		return ValidateEditorContext(World).GetValue();
	}

	FString AssetPath;
	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}

	FString CharacterName;
	if (!ExtractActorName(Params, TEXT("character_name"), CharacterName, Error))
	{
		return Error.GetValue();
	}

	FString LoadError;
	UCharacterConfigDataAsset* Config = LoadCharacterConfig(AssetPath, LoadError);
	if (!Config)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Find character
	ACharacter* Character = nullptr;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if ((*It)->GetName() == CharacterName || (*It)->GetActorLabel() == CharacterName)
		{
			Character = *It;
			break;
		}
	}

	if (!Character)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Character not found: %s"), *CharacterName));
	}

	bool bApplyMovement = ExtractOptionalBool(Params, TEXT("apply_movement"), true);
	bool bApplyMesh = ExtractOptionalBool(Params, TEXT("apply_mesh"), false);
	bool bApplyAnim = ExtractOptionalBool(Params, TEXT("apply_anim"), false);

	TArray<FString> AppliedSettings;

	// Apply movement settings
	if (bApplyMovement && Character->GetCharacterMovement())
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		Movement->MaxWalkSpeed = Config->BaseWalkSpeed;
		Movement->MaxAcceleration = Config->BaseAcceleration;
		Movement->JumpZVelocity = Config->BaseJumpVelocity;
		Movement->GroundFriction = Config->BaseGroundFriction;
		Movement->AirControl = Config->BaseAirControl;
		Movement->GravityScale = Config->BaseGravityScale;
		AppliedSettings.Add(TEXT("movement"));
	}

	// Apply capsule settings
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCapsuleRadius(Config->CapsuleRadius);
		Capsule->SetCapsuleHalfHeight(Config->CapsuleHalfHeight);
		AppliedSettings.Add(TEXT("capsule"));
	}

	// Apply mesh (if requested and available)
	if (bApplyMesh && !Config->SkeletalMesh.IsNull())
	{
		if (USkeletalMesh* Mesh = Config->SkeletalMesh.LoadSynchronous())
		{
			if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
			{
				MeshComp->SetSkeletalMesh(Mesh);
				AppliedSettings.Add(TEXT("skeletal_mesh"));
			}
		}
	}

	// Apply animation blueprint (if requested and available)
	if (bApplyAnim && !Config->AnimBlueprintClass.IsNull())
	{
		if (UClass* AnimClass = Config->AnimBlueprintClass.LoadSynchronous())
		{
			if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
			{
				MeshComp->SetAnimInstanceClass(AnimClass);
				AppliedSettings.Add(TEXT("anim_blueprint"));
			}
		}
	}

	MarkActorDirty(Character);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("character_name"), CharacterName);
	ResultData->SetStringField(TEXT("config_applied"), AssetPath);
	ResultData->SetArrayField(TEXT("applied_settings"), StringArrayToJsonArray(AppliedSettings));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Applied config '%s' to character '%s' (%d settings)"),
			*Config->DisplayName, *CharacterName, AppliedSettings.Num()),
		ResultData);
}

UCharacterConfigDataAsset* FMCPTool_CharacterData::LoadCharacterConfig(const FString& Path, FString& OutError)
{
	UCharacterConfigDataAsset* Config = LoadObject<UCharacterConfigDataAsset>(nullptr, *Path);
	if (!Config)
	{
		OutError = FString::Printf(TEXT("Failed to load character config: %s"), *Path);
	}
	return Config;
}

UDataTable* FMCPTool_CharacterData::LoadStatsTable(const FString& Path, FString& OutError)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, *Path);
	if (!Table)
	{
		OutError = FString::Printf(TEXT("Failed to load DataTable: %s"), *Path);
		return nullptr;
	}

	// Verify row struct
	if (Table->RowStruct != FCharacterStatsRow::StaticStruct())
	{
		OutError = FString::Printf(TEXT("DataTable '%s' does not use FCharacterStatsRow struct"), *Path);
		return nullptr;
	}

	return Table;
}

bool FMCPTool_CharacterData::SaveAsset(UObject* Asset, FString& OutError)
{
	if (!Asset)
	{
		OutError = TEXT("Cannot save null asset");
		return false;
	}

	UPackage* Package = Asset->GetOutermost();
	if (!Package)
	{
		OutError = TEXT("Asset has no package");
		return false;
	}

	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

	FSavePackageResultStruct Result = UPackage::Save(Package, Asset, *PackageFileName, SaveArgs);

	if (Result.Result != ESavePackageResult::Success)
	{
		OutError = FString::Printf(TEXT("Failed to save asset: %s"), *PackageFileName);
		return false;
	}

	return true;
}

TSharedPtr<FJsonObject> FMCPTool_CharacterData::ConfigToJson(UCharacterConfigDataAsset* Config)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Config)
	{
		return Json;
	}

	// Identity
	Json->SetStringField(TEXT("config_id"), Config->ConfigId.ToString());
	Json->SetStringField(TEXT("display_name"), Config->DisplayName);
	Json->SetStringField(TEXT("description"), Config->Description);
	Json->SetBoolField(TEXT("is_player_character"), Config->bIsPlayerCharacter);

	// Visuals
	if (!Config->SkeletalMesh.IsNull())
	{
		Json->SetStringField(TEXT("skeletal_mesh"), Config->SkeletalMesh.ToString());
	}
	if (!Config->AnimBlueprintClass.IsNull())
	{
		Json->SetStringField(TEXT("anim_blueprint"), Config->AnimBlueprintClass.ToString());
	}

	// Movement
	TSharedPtr<FJsonObject> Movement = MakeShared<FJsonObject>();
	Movement->SetNumberField(TEXT("base_walk_speed"), Config->BaseWalkSpeed);
	Movement->SetNumberField(TEXT("base_run_speed"), Config->BaseRunSpeed);
	Movement->SetNumberField(TEXT("base_jump_velocity"), Config->BaseJumpVelocity);
	Movement->SetNumberField(TEXT("base_acceleration"), Config->BaseAcceleration);
	Movement->SetNumberField(TEXT("base_ground_friction"), Config->BaseGroundFriction);
	Movement->SetNumberField(TEXT("base_air_control"), Config->BaseAirControl);
	Movement->SetNumberField(TEXT("base_gravity_scale"), Config->BaseGravityScale);
	Json->SetObjectField(TEXT("movement"), Movement);

	// Combat
	TSharedPtr<FJsonObject> Combat = MakeShared<FJsonObject>();
	Combat->SetNumberField(TEXT("base_health"), Config->BaseHealth);
	Combat->SetNumberField(TEXT("base_stamina"), Config->BaseStamina);
	Combat->SetNumberField(TEXT("base_damage"), Config->BaseDamage);
	Combat->SetNumberField(TEXT("base_defense"), Config->BaseDefense);
	Json->SetObjectField(TEXT("combat"), Combat);

	// Collision
	TSharedPtr<FJsonObject> Collision = MakeShared<FJsonObject>();
	Collision->SetNumberField(TEXT("capsule_radius"), Config->CapsuleRadius);
	Collision->SetNumberField(TEXT("capsule_half_height"), Config->CapsuleHalfHeight);
	Json->SetObjectField(TEXT("collision"), Collision);

	// Tags
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FName& Tag : Config->GameplayTags)
	{
		TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	Json->SetArrayField(TEXT("gameplay_tags"), TagsArray);

	// Stats table reference
	if (!Config->StatsTable.IsNull())
	{
		Json->SetStringField(TEXT("stats_table"), Config->StatsTable.ToString());
		Json->SetStringField(TEXT("default_stats_row"), Config->DefaultStatsRowName.ToString());
	}

	return Json;
}

TSharedPtr<FJsonObject> FMCPTool_CharacterData::StatsRowToJson(const FCharacterStatsRow& Row, const FName& RowName)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	Json->SetStringField(TEXT("row_name"), RowName.ToString());
	Json->SetStringField(TEXT("stats_id"), Row.StatsId.ToString());
	Json->SetStringField(TEXT("display_name"), Row.DisplayName);

	// Vitals
	Json->SetNumberField(TEXT("base_health"), Row.BaseHealth);
	Json->SetNumberField(TEXT("max_health"), Row.MaxHealth);
	Json->SetNumberField(TEXT("base_stamina"), Row.BaseStamina);
	Json->SetNumberField(TEXT("max_stamina"), Row.MaxStamina);

	// Movement
	Json->SetNumberField(TEXT("walk_speed"), Row.WalkSpeed);
	Json->SetNumberField(TEXT("run_speed"), Row.RunSpeed);
	Json->SetNumberField(TEXT("jump_velocity"), Row.JumpVelocity);

	// Combat/Progression
	Json->SetNumberField(TEXT("damage_multiplier"), Row.DamageMultiplier);
	Json->SetNumberField(TEXT("defense_multiplier"), Row.DefenseMultiplier);
	Json->SetNumberField(TEXT("xp_multiplier"), Row.XPMultiplier);
	Json->SetNumberField(TEXT("level"), Row.Level);

	// Tags
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FName& Tag : Row.Tags)
	{
		TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	Json->SetArrayField(TEXT("tags"), TagsArray);

	return Json;
}

void FMCPTool_CharacterData::PopulateConfigFromParams(UCharacterConfigDataAsset* Config, const TSharedRef<FJsonObject>& Params)
{
	if (!Config)
	{
		return;
	}

	FString StrVal;
	double NumVal;
	bool BoolVal;

	// Identity
	if (Params->TryGetStringField(TEXT("config_id"), StrVal))
	{
		Config->ConfigId = FName(*StrVal);
	}
	if (Params->TryGetStringField(TEXT("display_name"), StrVal))
	{
		Config->DisplayName = StrVal;
	}
	if (Params->TryGetStringField(TEXT("description"), StrVal))
	{
		Config->Description = StrVal;
	}
	if (Params->TryGetBoolField(TEXT("is_player_character"), BoolVal))
	{
		Config->bIsPlayerCharacter = BoolVal;
	}

	// Visuals
	if (Params->TryGetStringField(TEXT("skeletal_mesh"), StrVal))
	{
		Config->SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(StrVal));
	}
	if (Params->TryGetStringField(TEXT("anim_blueprint"), StrVal))
	{
		Config->AnimBlueprintClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(StrVal));
	}

	// Movement
	if (Params->TryGetNumberField(TEXT("base_walk_speed"), NumVal))
	{
		Config->BaseWalkSpeed = static_cast<float>(FMath::Clamp(NumVal, 0.0, 10000.0));
	}
	if (Params->TryGetNumberField(TEXT("base_run_speed"), NumVal))
	{
		Config->BaseRunSpeed = static_cast<float>(FMath::Clamp(NumVal, 0.0, 10000.0));
	}
	if (Params->TryGetNumberField(TEXT("base_jump_velocity"), NumVal))
	{
		Config->BaseJumpVelocity = static_cast<float>(FMath::Clamp(NumVal, 0.0, 5000.0));
	}
	if (Params->TryGetNumberField(TEXT("base_acceleration"), NumVal))
	{
		Config->BaseAcceleration = static_cast<float>(FMath::Clamp(NumVal, 0.0, 100000.0));
	}
	if (Params->TryGetNumberField(TEXT("base_ground_friction"), NumVal))
	{
		Config->BaseGroundFriction = static_cast<float>(FMath::Clamp(NumVal, 0.0, 100.0));
	}
	if (Params->TryGetNumberField(TEXT("base_air_control"), NumVal))
	{
		Config->BaseAirControl = static_cast<float>(FMath::Clamp(NumVal, 0.0, 1.0));
	}
	if (Params->TryGetNumberField(TEXT("base_gravity_scale"), NumVal))
	{
		Config->BaseGravityScale = static_cast<float>(FMath::Clamp(NumVal, -10.0, 10.0));
	}

	// Combat
	if (Params->TryGetNumberField(TEXT("base_health"), NumVal))
	{
		Config->BaseHealth = static_cast<float>(FMath::Max(NumVal, 0.0));
	}
	if (Params->TryGetNumberField(TEXT("base_stamina"), NumVal))
	{
		Config->BaseStamina = static_cast<float>(FMath::Max(NumVal, 0.0));
	}
	if (Params->TryGetNumberField(TEXT("base_damage"), NumVal))
	{
		Config->BaseDamage = static_cast<float>(FMath::Max(NumVal, 0.0));
	}
	if (Params->TryGetNumberField(TEXT("base_defense"), NumVal))
	{
		Config->BaseDefense = static_cast<float>(FMath::Max(NumVal, 0.0));
	}

	// Collision
	if (Params->TryGetNumberField(TEXT("capsule_radius"), NumVal))
	{
		Config->CapsuleRadius = static_cast<float>(FMath::Clamp(NumVal, 1.0, 500.0));
	}
	if (Params->TryGetNumberField(TEXT("capsule_half_height"), NumVal))
	{
		Config->CapsuleHalfHeight = static_cast<float>(FMath::Clamp(NumVal, 1.0, 500.0));
	}

	// Tags
	const TArray<TSharedPtr<FJsonValue>>* TagsArray;
	if (Params->TryGetArrayField(TEXT("gameplay_tags"), TagsArray))
	{
		Config->GameplayTags.Empty();
		for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
		{
			if (TagValue->TryGetString(StrVal))
			{
				Config->GameplayTags.Add(FName(*StrVal));
			}
		}
	}
}

void FMCPTool_CharacterData::PopulateStatsRowFromParams(FCharacterStatsRow& Row, const TSharedRef<FJsonObject>& Params)
{
	FString StrVal;
	double NumVal;

	if (Params->TryGetStringField(TEXT("stats_id"), StrVal))
	{
		Row.StatsId = FName(*StrVal);
	}
	if (Params->TryGetStringField(TEXT("display_name"), StrVal))
	{
		Row.DisplayName = StrVal;
	}

	// Vitals
	if (Params->TryGetNumberField(TEXT("base_health"), NumVal))
	{
		Row.BaseHealth = static_cast<float>(FMath::Max(NumVal, 0.0));
	}
	if (Params->TryGetNumberField(TEXT("max_health"), NumVal))
	{
		Row.MaxHealth = static_cast<float>(FMath::Max(NumVal, 0.0));
	}
	if (Params->TryGetNumberField(TEXT("base_stamina"), NumVal))
	{
		Row.BaseStamina = static_cast<float>(FMath::Max(NumVal, 0.0));
	}
	if (Params->TryGetNumberField(TEXT("max_stamina"), NumVal))
	{
		Row.MaxStamina = static_cast<float>(FMath::Max(NumVal, 0.0));
	}

	// Movement
	if (Params->TryGetNumberField(TEXT("walk_speed"), NumVal))
	{
		Row.WalkSpeed = static_cast<float>(FMath::Clamp(NumVal, 0.0, 10000.0));
	}
	if (Params->TryGetNumberField(TEXT("run_speed"), NumVal))
	{
		Row.RunSpeed = static_cast<float>(FMath::Clamp(NumVal, 0.0, 10000.0));
	}
	if (Params->TryGetNumberField(TEXT("jump_velocity"), NumVal))
	{
		Row.JumpVelocity = static_cast<float>(FMath::Clamp(NumVal, 0.0, 5000.0));
	}

	// Combat/Progression
	if (Params->TryGetNumberField(TEXT("damage_multiplier"), NumVal))
	{
		Row.DamageMultiplier = static_cast<float>(FMath::Clamp(NumVal, 0.0, 10.0));
	}
	if (Params->TryGetNumberField(TEXT("defense_multiplier"), NumVal))
	{
		Row.DefenseMultiplier = static_cast<float>(FMath::Clamp(NumVal, 0.0, 10.0));
	}
	if (Params->TryGetNumberField(TEXT("xp_multiplier"), NumVal))
	{
		Row.XPMultiplier = static_cast<float>(FMath::Clamp(NumVal, 0.0, 10.0));
	}
	if (Params->TryGetNumberField(TEXT("level"), NumVal))
	{
		Row.Level = FMath::Max(1, static_cast<int32>(NumVal));
	}

	// Tags
	const TArray<TSharedPtr<FJsonValue>>* TagsArray;
	if (Params->TryGetArrayField(TEXT("tags"), TagsArray))
	{
		Row.Tags.Empty();
		for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
		{
			if (TagValue->TryGetString(StrVal))
			{
				Row.Tags.Add(FName(*StrVal));
			}
		}
	}
}
