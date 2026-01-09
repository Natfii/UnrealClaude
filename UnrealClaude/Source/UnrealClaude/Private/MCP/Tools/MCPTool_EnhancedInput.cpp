// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_EnhancedInput.h"
#include "UnrealClaudeModule.h"
#include "MCP/MCPParamValidator.h"

// Enhanced Input includes
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "InputModifiers.h"
#include "EnhancedActionKeyMapping.h"

// Asset management
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"

FMCPToolResult FMCPTool_EnhancedInput::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Extract operation
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	// Route to appropriate handler
	if (Operation == TEXT("create_input_action"))
	{
		return ExecuteCreateInputAction(Params);
	}
	if (Operation == TEXT("create_mapping_context"))
	{
		return ExecuteCreateMappingContext(Params);
	}
	if (Operation == TEXT("add_mapping"))
	{
		return ExecuteAddMapping(Params);
	}
	if (Operation == TEXT("remove_mapping"))
	{
		return ExecuteRemoveMapping(Params);
	}
	if (Operation == TEXT("add_trigger"))
	{
		return ExecuteAddTrigger(Params);
	}
	if (Operation == TEXT("add_modifier"))
	{
		return ExecuteAddModifier(Params);
	}
	if (Operation == TEXT("query_context"))
	{
		return ExecuteQueryContext(Params);
	}
	if (Operation == TEXT("query_action"))
	{
		return ExecuteQueryAction(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid operations: create_input_action, create_mapping_context, "
			"add_mapping, remove_mapping, add_trigger, add_modifier, query_context, query_action"),
		*Operation));
}

// ============================================================================
// Asset Creation Operations
// ============================================================================

FMCPToolResult FMCPTool_EnhancedInput::ExecuteCreateInputAction(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString Name;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("action_name"), Name, Error))
	{
		return Error.GetValue();
	}

	FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"), TEXT("/Game/Input"));
	FString ValueTypeStr = ExtractOptionalString(Params, TEXT("value_type"), TEXT("Digital"));

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(PackagePath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Parse value type
	EInputActionValueType ValueType = EInputActionValueType::Boolean;
	if (ValueTypeStr == TEXT("Digital") || ValueTypeStr == TEXT("Boolean") || ValueTypeStr == TEXT("Bool"))
	{
		ValueType = EInputActionValueType::Boolean;
	}
	else if (ValueTypeStr == TEXT("Axis1D") || ValueTypeStr == TEXT("Float"))
	{
		ValueType = EInputActionValueType::Axis1D;
	}
	else if (ValueTypeStr == TEXT("Axis2D") || ValueTypeStr == TEXT("Vector2D"))
	{
		ValueType = EInputActionValueType::Axis2D;
	}
	else if (ValueTypeStr == TEXT("Axis3D") || ValueTypeStr == TEXT("Vector"))
	{
		ValueType = EInputActionValueType::Axis3D;
	}
	else
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Invalid value_type: %s. Valid types: Digital, Axis1D, Axis2D, Axis3D"), *ValueTypeStr));
	}

	// Create package
	FString FullPath = PackagePath / Name;
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *FullPath));
	}

	// Create InputAction
	UInputAction* NewAction = NewObject<UInputAction>(Package, FName(*Name), RF_Public | RF_Standalone);
	if (!NewAction)
	{
		return FMCPToolResult::Error(TEXT("Failed to create InputAction"));
	}

	// Set value type
	NewAction->ValueType = ValueType;

	// Mark package dirty and save
	Package->MarkPackageDirty();
	FString SaveError;
	if (!SaveAsset(NewAction, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(NewAction);

	UE_LOG(LogUnrealClaude, Log, TEXT("Created InputAction: %s (ValueType: %s)"), *FullPath, *ValueTypeStr);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), NewAction->GetPathName());
	ResultData->SetStringField(TEXT("name"), Name);
	ResultData->SetStringField(TEXT("value_type"), ValueTypeStr);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created InputAction '%s' at %s"), *Name, *FullPath),
		ResultData);
}

FMCPToolResult FMCPTool_EnhancedInput::ExecuteCreateMappingContext(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString Name;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("context_name"), Name, Error))
	{
		return Error.GetValue();
	}

	FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"), TEXT("/Game/Input"));

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(PackagePath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Create package
	FString FullPath = PackagePath / Name;
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *FullPath));
	}

	// Create InputMappingContext
	UInputMappingContext* NewContext = NewObject<UInputMappingContext>(Package, FName(*Name), RF_Public | RF_Standalone);
	if (!NewContext)
	{
		return FMCPToolResult::Error(TEXT("Failed to create InputMappingContext"));
	}

	// Mark package dirty and save
	Package->MarkPackageDirty();
	FString SaveError;
	if (!SaveAsset(NewContext, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(NewContext);

	UE_LOG(LogUnrealClaude, Log, TEXT("Created InputMappingContext: %s"), *FullPath);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), NewContext->GetPathName());
	ResultData->SetStringField(TEXT("name"), Name);
	ResultData->SetNumberField(TEXT("mapping_count"), 0);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created InputMappingContext '%s' at %s"), *Name, *FullPath),
		ResultData);
}

// ============================================================================
// Mapping Operations
// ============================================================================

FMCPToolResult FMCPTool_EnhancedInput::ExecuteAddMapping(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString ContextPath, ActionPath, KeyName;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("context_path"), ContextPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("action_path"), ActionPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("key"), KeyName, Error))
	{
		return Error.GetValue();
	}

	// Load assets
	FString LoadError;
	UInputMappingContext* Context = LoadMappingContext(ContextPath, LoadError);
	if (!Context)
	{
		return FMCPToolResult::Error(LoadError);
	}

	UInputAction* Action = LoadInputAction(ActionPath, LoadError);
	if (!Action)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Parse key
	FString KeyError;
	FKey Key = ParseKey(KeyName, KeyError);
	if (!Key.IsValid())
	{
		return FMCPToolResult::Error(KeyError);
	}

	// Add mapping using MapKey
	FEnhancedActionKeyMapping& NewMapping = Context->MapKey(Action, Key);

	// Mark dirty and save
	Context->MarkPackageDirty();
	FString SaveError;
	if (!SaveAsset(Context, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Added mapping: %s -> %s in %s"),
		*KeyName, *Action->GetName(), *Context->GetName());

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("context_path"), Context->GetPathName());
	ResultData->SetStringField(TEXT("action_path"), Action->GetPathName());
	ResultData->SetStringField(TEXT("key"), KeyName);
	ResultData->SetNumberField(TEXT("mapping_index"), Context->GetMappings().Num() - 1);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added mapping: %s -> %s"), *KeyName, *Action->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_EnhancedInput::ExecuteRemoveMapping(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString ContextPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("context_path"), ContextPath, Error))
	{
		return Error.GetValue();
	}

	int32 MappingIndex = ExtractOptionalNumber<int32>(Params, TEXT("mapping_index"), -1);
	if (MappingIndex < 0)
	{
		return FMCPToolResult::Error(TEXT("Missing or invalid mapping_index parameter"));
	}

	// Load context
	FString LoadError;
	UInputMappingContext* Context = LoadMappingContext(ContextPath, LoadError);
	if (!Context)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Validate index
	TArray<FEnhancedActionKeyMapping>& Mappings = const_cast<TArray<FEnhancedActionKeyMapping>&>(Context->GetMappings());
	if (MappingIndex >= Mappings.Num())
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Invalid mapping_index %d. Context has %d mappings (0-%d)"),
			MappingIndex, Mappings.Num(), Mappings.Num() - 1));
	}

	// Store info before removal
	FString RemovedKey = Mappings[MappingIndex].Key.GetFName().ToString();
	FString RemovedAction = Mappings[MappingIndex].Action ? Mappings[MappingIndex].Action->GetName() : TEXT("None");

	// Remove mapping
	Context->UnmapKey(Mappings[MappingIndex].Action, Mappings[MappingIndex].Key);

	// Mark dirty and save
	Context->MarkPackageDirty();
	FString SaveError;
	if (!SaveAsset(Context, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Removed mapping at index %d: %s -> %s"),
		MappingIndex, *RemovedKey, *RemovedAction);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("context_path"), Context->GetPathName());
	ResultData->SetNumberField(TEXT("removed_index"), MappingIndex);
	ResultData->SetStringField(TEXT("removed_key"), RemovedKey);
	ResultData->SetStringField(TEXT("removed_action"), RemovedAction);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Removed mapping at index %d: %s -> %s"), MappingIndex, *RemovedKey, *RemovedAction),
		ResultData);
}

FMCPToolResult FMCPTool_EnhancedInput::ExecuteAddTrigger(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString ContextPath, ActionPath, TriggerType;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("context_path"), ContextPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("action_path"), ActionPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("trigger_type"), TriggerType, Error))
	{
		return Error.GetValue();
	}

	// Load assets
	FString LoadError;
	UInputMappingContext* Context = LoadMappingContext(ContextPath, LoadError);
	if (!Context)
	{
		return FMCPToolResult::Error(LoadError);
	}

	UInputAction* Action = LoadInputAction(ActionPath, LoadError);
	if (!Action)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Find mapping for this action using helper
	FString MappingError;
	int32 MappingIndex = FindMappingIndex(Context, Action, Params, MappingError);
	if (MappingIndex < 0)
	{
		return FMCPToolResult::Error(MappingError);
	}

	TArray<FEnhancedActionKeyMapping>& Mappings = const_cast<TArray<FEnhancedActionKeyMapping>&>(Context->GetMappings());

	// Create trigger
	FString TriggerError;
	UInputTrigger* Trigger = CreateTrigger(TriggerType, Params, TriggerError);
	if (!Trigger)
	{
		return FMCPToolResult::Error(TriggerError);
	}

	// Add trigger to mapping
	Mappings[MappingIndex].Triggers.Add(Trigger);

	// Mark dirty and save
	Context->MarkPackageDirty();
	FString SaveError;
	if (!SaveAsset(Context, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Added %s trigger to %s in %s"),
		*TriggerType, *Action->GetName(), *Context->GetName());

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("context_path"), Context->GetPathName());
	ResultData->SetStringField(TEXT("action_path"), Action->GetPathName());
	ResultData->SetStringField(TEXT("trigger_type"), TriggerType);
	ResultData->SetNumberField(TEXT("trigger_count"), Mappings[MappingIndex].Triggers.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added %s trigger to %s"), *TriggerType, *Action->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_EnhancedInput::ExecuteAddModifier(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString ContextPath, ActionPath, ModifierType;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("context_path"), ContextPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("action_path"), ActionPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("modifier_type"), ModifierType, Error))
	{
		return Error.GetValue();
	}

	// Load assets
	FString LoadError;
	UInputMappingContext* Context = LoadMappingContext(ContextPath, LoadError);
	if (!Context)
	{
		return FMCPToolResult::Error(LoadError);
	}

	UInputAction* Action = LoadInputAction(ActionPath, LoadError);
	if (!Action)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Find mapping for this action using helper
	FString MappingError;
	int32 MappingIndex = FindMappingIndex(Context, Action, Params, MappingError);
	if (MappingIndex < 0)
	{
		return FMCPToolResult::Error(MappingError);
	}

	TArray<FEnhancedActionKeyMapping>& Mappings = const_cast<TArray<FEnhancedActionKeyMapping>&>(Context->GetMappings());

	// Create modifier
	FString ModifierError;
	UInputModifier* Modifier = CreateModifier(ModifierType, Params, ModifierError);
	if (!Modifier)
	{
		return FMCPToolResult::Error(ModifierError);
	}

	// Add modifier to mapping
	Mappings[MappingIndex].Modifiers.Add(Modifier);

	// Mark dirty and save
	Context->MarkPackageDirty();
	FString SaveError;
	if (!SaveAsset(Context, SaveError))
	{
		return FMCPToolResult::Error(SaveError);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Added %s modifier to %s in %s"),
		*ModifierType, *Action->GetName(), *Context->GetName());

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("context_path"), Context->GetPathName());
	ResultData->SetStringField(TEXT("action_path"), Action->GetPathName());
	ResultData->SetStringField(TEXT("modifier_type"), ModifierType);
	ResultData->SetNumberField(TEXT("modifier_count"), Mappings[MappingIndex].Modifiers.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added %s modifier to %s"), *ModifierType, *Action->GetName()),
		ResultData);
}

// ============================================================================
// Query Operations
// ============================================================================

FMCPToolResult FMCPTool_EnhancedInput::ExecuteQueryContext(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString ContextPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("context_path"), ContextPath, Error))
	{
		return Error.GetValue();
	}

	// Load context
	FString LoadError;
	UInputMappingContext* Context = LoadMappingContext(ContextPath, LoadError);
	if (!Context)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Build result
	TSharedPtr<FJsonObject> ResultData = MappingContextToJson(Context);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Queried context '%s' with %d mappings"),
			*Context->GetName(), Context->GetMappings().Num()),
		ResultData);
}

FMCPToolResult FMCPTool_EnhancedInput::ExecuteQueryAction(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString ActionPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("action_path"), ActionPath, Error))
	{
		return Error.GetValue();
	}

	// Load action
	FString LoadError;
	UInputAction* Action = LoadInputAction(ActionPath, LoadError);
	if (!Action)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Build result
	TSharedPtr<FJsonObject> ResultData = InputActionToJson(Action);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Queried action '%s'"), *Action->GetName()),
		ResultData);
}

// ============================================================================
// Helper Methods
// ============================================================================

UInputAction* FMCPTool_EnhancedInput::LoadInputAction(const FString& Path, FString& OutError)
{
	// Validate path
	if (!FMCPParamValidator::ValidateBlueprintPath(Path, OutError))
	{
		return nullptr;
	}

	// Try loading with various path formats
	FString AdjustedPath = Path;
	if (!AdjustedPath.EndsWith(TEXT(".")) && !AdjustedPath.Contains(TEXT(".")))
	{
		AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(Path);
	}

	UInputAction* Action = LoadObject<UInputAction>(nullptr, *AdjustedPath);
	if (!Action)
	{
		// Try without adjustment
		Action = LoadObject<UInputAction>(nullptr, *Path);
	}

	if (!Action)
	{
		OutError = FString::Printf(TEXT("Failed to load InputAction: %s"), *Path);
		return nullptr;
	}

	return Action;
}

UInputMappingContext* FMCPTool_EnhancedInput::LoadMappingContext(const FString& Path, FString& OutError)
{
	// Validate path
	if (!FMCPParamValidator::ValidateBlueprintPath(Path, OutError))
	{
		return nullptr;
	}

	// Try loading with various path formats
	FString AdjustedPath = Path;
	if (!AdjustedPath.EndsWith(TEXT(".")) && !AdjustedPath.Contains(TEXT(".")))
	{
		AdjustedPath += TEXT(".") + FPaths::GetBaseFilename(Path);
	}

	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, *AdjustedPath);
	if (!Context)
	{
		// Try without adjustment
		Context = LoadObject<UInputMappingContext>(nullptr, *Path);
	}

	if (!Context)
	{
		OutError = FString::Printf(TEXT("Failed to load InputMappingContext: %s"), *Path);
		return nullptr;
	}

	return Context;
}

bool FMCPTool_EnhancedInput::SaveAsset(UObject* Asset, FString& OutError)
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

	// Use EditorAssetLibrary for reliable saving
	FString PackagePath = Package->GetPathName();
	if (!UEditorAssetLibrary::SaveAsset(PackagePath, false))
	{
		OutError = FString::Printf(TEXT("Failed to save asset: %s"), *PackagePath);
		return false;
	}

	return true;
}

FKey FMCPTool_EnhancedInput::ParseKey(const FString& KeyName, FString& OutError)
{
	FKey ParsedKey = FKey(*KeyName);

	if (!ParsedKey.IsValid())
	{
		OutError = FString::Printf(
			TEXT("Invalid key name: %s. Use standard UE key names like 'SpaceBar', 'W', 'LeftMouseButton', 'Gamepad_FaceButton_Bottom'"),
			*KeyName);
		return FKey();  // Return invalid/empty key
	}

	return ParsedKey;
}

int32 FMCPTool_EnhancedInput::FindMappingIndex(
	UInputMappingContext* Context,
	UInputAction* Action,
	const TSharedRef<FJsonObject>& Params,
	FString& OutError)
{
	if (!Context || !Action)
	{
		OutError = TEXT("Invalid context or action");
		return -1;
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();
	int32 MappingIndex = ExtractOptionalNumber<int32>(Params, TEXT("mapping_index"), -1);

	if (MappingIndex >= 0)
	{
		// Use specified index - validate bounds
		if (MappingIndex >= Mappings.Num())
		{
			OutError = FString::Printf(
				TEXT("Invalid mapping_index %d. Context has %d mappings."),
				MappingIndex, Mappings.Num());
			return -1;
		}
		// Verify the action matches if both are specified
		if (Mappings[MappingIndex].Action != Action)
		{
			OutError = FString::Printf(
				TEXT("Mapping at index %d is for action '%s', not '%s'"),
				MappingIndex,
				Mappings[MappingIndex].Action ? *Mappings[MappingIndex].Action->GetName() : TEXT("None"),
				*Action->GetName());
			return -1;
		}
		return MappingIndex;
	}

	// Search for first mapping with this action
	for (int32 i = 0; i < Mappings.Num(); ++i)
	{
		if (Mappings[i].Action == Action)
		{
			return i;
		}
	}

	OutError = FString::Printf(
		TEXT("No mapping found for action '%s' in context '%s'. Use query_context to see available mappings."),
		*Action->GetName(), *Context->GetName());
	return -1;
}

UInputTrigger* FMCPTool_EnhancedInput::CreateTrigger(const FString& TriggerType, const TSharedRef<FJsonObject>& Params, FString& OutError)
{
	UInputTrigger* Trigger = nullptr;

	if (TriggerType == TEXT("Pressed"))
	{
		Trigger = NewObject<UInputTriggerPressed>();
	}
	else if (TriggerType == TEXT("Released"))
	{
		Trigger = NewObject<UInputTriggerReleased>();
	}
	else if (TriggerType == TEXT("Down"))
	{
		Trigger = NewObject<UInputTriggerDown>();
	}
	else if (TriggerType == TEXT("Hold"))
	{
		UInputTriggerHold* HoldTrigger = NewObject<UInputTriggerHold>();
		HoldTrigger->HoldTimeThreshold = ExtractOptionalNumber<float>(Params, TEXT("hold_time"), 0.5f);
		if (HoldTrigger->HoldTimeThreshold <= 0.0f || HoldTrigger->HoldTimeThreshold > 60.0f)
		{
			OutError = TEXT("hold_time must be between 0 and 60 seconds");
			return nullptr;
		}
		Trigger = HoldTrigger;
	}
	else if (TriggerType == TEXT("HoldAndRelease"))
	{
		UInputTriggerHoldAndRelease* HoldReleaseTrigger = NewObject<UInputTriggerHoldAndRelease>();
		HoldReleaseTrigger->HoldTimeThreshold = ExtractOptionalNumber<float>(Params, TEXT("hold_time"), 0.5f);
		if (HoldReleaseTrigger->HoldTimeThreshold <= 0.0f || HoldReleaseTrigger->HoldTimeThreshold > 60.0f)
		{
			OutError = TEXT("hold_time must be between 0 and 60 seconds");
			return nullptr;
		}
		Trigger = HoldReleaseTrigger;
	}
	else if (TriggerType == TEXT("Tap"))
	{
		UInputTriggerTap* TapTrigger = NewObject<UInputTriggerTap>();
		TapTrigger->TapReleaseTimeThreshold = ExtractOptionalNumber<float>(Params, TEXT("tap_release_time"), 0.2f);
		if (TapTrigger->TapReleaseTimeThreshold <= 0.0f || TapTrigger->TapReleaseTimeThreshold > 5.0f)
		{
			OutError = TEXT("tap_release_time must be between 0 and 5 seconds");
			return nullptr;
		}
		Trigger = TapTrigger;
	}
	else if (TriggerType == TEXT("Pulse"))
	{
		UInputTriggerPulse* PulseTrigger = NewObject<UInputTriggerPulse>();
		PulseTrigger->Interval = ExtractOptionalNumber<float>(Params, TEXT("pulse_interval"), 0.1f);
		if (PulseTrigger->Interval <= 0.01f || PulseTrigger->Interval > 10.0f)
		{
			OutError = TEXT("pulse_interval must be between 0.01 and 10 seconds");
			return nullptr;
		}
		Trigger = PulseTrigger;
	}
	else if (TriggerType == TEXT("ChordAction"))
	{
		FString ChordActionPath = ExtractOptionalString(Params, TEXT("chord_action_path"), TEXT(""));
		if (ChordActionPath.IsEmpty())
		{
			OutError = TEXT("ChordAction trigger requires 'chord_action_path' parameter");
			return nullptr;
		}

		FString LoadError;
		UInputAction* ChordAction = LoadInputAction(ChordActionPath, LoadError);
		if (!ChordAction)
		{
			OutError = LoadError;
			return nullptr;
		}

		UInputTriggerChordAction* ChordTrigger = NewObject<UInputTriggerChordAction>();
		ChordTrigger->ChordAction = ChordAction;
		Trigger = ChordTrigger;
	}
	else
	{
		OutError = FString::Printf(
			TEXT("Invalid trigger_type: %s. Valid types: Pressed, Released, Down, Hold, HoldAndRelease, Tap, Pulse, ChordAction"),
			*TriggerType);
		return nullptr;
	}

	return Trigger;
}

UInputModifier* FMCPTool_EnhancedInput::CreateModifier(const FString& ModifierType, const TSharedRef<FJsonObject>& Params, FString& OutError)
{
	UInputModifier* Modifier = nullptr;

	if (ModifierType == TEXT("Negate"))
	{
		Modifier = NewObject<UInputModifierNegate>();
	}
	else if (ModifierType == TEXT("Swizzle"))
	{
		UInputModifierSwizzleAxis* SwizzleModifier = NewObject<UInputModifierSwizzleAxis>();
		FString SwizzleOrder = ExtractOptionalString(Params, TEXT("swizzle_order"), TEXT("YXZ"));

		if (SwizzleOrder == TEXT("YXZ"))
		{
			SwizzleModifier->Order = EInputAxisSwizzle::YXZ;
		}
		else if (SwizzleOrder == TEXT("ZYX"))
		{
			SwizzleModifier->Order = EInputAxisSwizzle::ZYX;
		}
		else if (SwizzleOrder == TEXT("XZY"))
		{
			SwizzleModifier->Order = EInputAxisSwizzle::XZY;
		}
		else if (SwizzleOrder == TEXT("YZX"))
		{
			SwizzleModifier->Order = EInputAxisSwizzle::YZX;
		}
		else if (SwizzleOrder == TEXT("ZXY"))
		{
			SwizzleModifier->Order = EInputAxisSwizzle::ZXY;
		}
		else
		{
			OutError = FString::Printf(
				TEXT("Invalid swizzle_order: %s. Valid orders: YXZ, ZYX, XZY, YZX, ZXY"),
				*SwizzleOrder);
			return nullptr;
		}

		Modifier = SwizzleModifier;
	}
	else if (ModifierType == TEXT("Scalar"))
	{
		UInputModifierScalar* ScalarModifier = NewObject<UInputModifierScalar>();
		ScalarModifier->Scalar = ExtractVectorParam(Params, TEXT("scalar"), FVector(1.0f, 1.0f, 1.0f));
		Modifier = ScalarModifier;
	}
	else if (ModifierType == TEXT("DeadZone"))
	{
		UInputModifierDeadZone* DeadZoneModifier = NewObject<UInputModifierDeadZone>();
		DeadZoneModifier->LowerThreshold = ExtractOptionalNumber<float>(Params, TEXT("dead_zone_lower"), 0.2f);
		DeadZoneModifier->UpperThreshold = ExtractOptionalNumber<float>(Params, TEXT("dead_zone_upper"), 1.0f);

		// Validate dead zone thresholds
		if (DeadZoneModifier->LowerThreshold < 0.0f || DeadZoneModifier->LowerThreshold > 1.0f)
		{
			OutError = TEXT("dead_zone_lower must be between 0.0 and 1.0");
			return nullptr;
		}
		if (DeadZoneModifier->UpperThreshold < 0.0f || DeadZoneModifier->UpperThreshold > 1.0f)
		{
			OutError = TEXT("dead_zone_upper must be between 0.0 and 1.0");
			return nullptr;
		}
		if (DeadZoneModifier->LowerThreshold >= DeadZoneModifier->UpperThreshold)
		{
			OutError = TEXT("dead_zone_lower must be less than dead_zone_upper");
			return nullptr;
		}

		FString DeadZoneType = ExtractOptionalString(Params, TEXT("dead_zone_type"), TEXT("Axial"));
		if (DeadZoneType == TEXT("Radial"))
		{
			DeadZoneModifier->Type = EDeadZoneType::Radial;
		}
		else
		{
			DeadZoneModifier->Type = EDeadZoneType::Axial;
		}

		Modifier = DeadZoneModifier;
	}
	else
	{
		OutError = FString::Printf(
			TEXT("Invalid modifier_type: %s. Valid types: Negate, Swizzle, Scalar, DeadZone"),
			*ModifierType);
		return nullptr;
	}

	return Modifier;
}

// ============================================================================
// JSON Conversion Helpers
// ============================================================================

TSharedPtr<FJsonObject> FMCPTool_EnhancedInput::InputActionToJson(UInputAction* Action)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!Action)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), Action->GetName());
	Json->SetStringField(TEXT("path"), Action->GetPathName());

	// Value type
	FString ValueTypeStr;
	switch (Action->ValueType)
	{
	case EInputActionValueType::Boolean:
		ValueTypeStr = TEXT("Digital");
		break;
	case EInputActionValueType::Axis1D:
		ValueTypeStr = TEXT("Axis1D");
		break;
	case EInputActionValueType::Axis2D:
		ValueTypeStr = TEXT("Axis2D");
		break;
	case EInputActionValueType::Axis3D:
		ValueTypeStr = TEXT("Axis3D");
		break;
	default:
		ValueTypeStr = TEXT("Unknown");
		break;
	}
	Json->SetStringField(TEXT("value_type"), ValueTypeStr);

	// Action-level triggers
	TArray<TSharedPtr<FJsonValue>> TriggersArray;
	for (UInputTrigger* Trigger : Action->Triggers)
	{
		if (Trigger)
		{
			TSharedPtr<FJsonObject> TriggerJson = MakeShared<FJsonObject>();
			TriggerJson->SetStringField(TEXT("type"), Trigger->GetClass()->GetName());
			TriggersArray.Add(MakeShared<FJsonValueObject>(TriggerJson));
		}
	}
	Json->SetArrayField(TEXT("triggers"), TriggersArray);

	// Action-level modifiers
	TArray<TSharedPtr<FJsonValue>> ModifiersArray;
	for (UInputModifier* Modifier : Action->Modifiers)
	{
		if (Modifier)
		{
			TSharedPtr<FJsonObject> ModifierJson = MakeShared<FJsonObject>();
			ModifierJson->SetStringField(TEXT("type"), Modifier->GetClass()->GetName());
			ModifiersArray.Add(MakeShared<FJsonValueObject>(ModifierJson));
		}
	}
	Json->SetArrayField(TEXT("modifiers"), ModifiersArray);

	return Json;
}

TSharedPtr<FJsonObject> FMCPTool_EnhancedInput::MappingContextToJson(UInputMappingContext* Context)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!Context)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), Context->GetName());
	Json->SetStringField(TEXT("path"), Context->GetPathName());

	// Mappings
	TArray<TSharedPtr<FJsonValue>> MappingsArray;
	const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();
	for (int32 i = 0; i < Mappings.Num(); ++i)
	{
		MappingsArray.Add(MakeShared<FJsonValueObject>(MappingToJson(Mappings[i], i)));
	}
	Json->SetArrayField(TEXT("mappings"), MappingsArray);
	Json->SetNumberField(TEXT("mapping_count"), Mappings.Num());

	return Json;
}

TSharedPtr<FJsonObject> FMCPTool_EnhancedInput::MappingToJson(const FEnhancedActionKeyMapping& Mapping, int32 Index)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	Json->SetNumberField(TEXT("index"), Index);
	Json->SetStringField(TEXT("key"), Mapping.Key.GetFName().ToString());

	if (Mapping.Action)
	{
		Json->SetStringField(TEXT("action"), Mapping.Action->GetName());
		Json->SetStringField(TEXT("action_path"), Mapping.Action->GetPathName());
	}
	else
	{
		Json->SetStringField(TEXT("action"), TEXT("None"));
	}

	// Triggers
	TArray<TSharedPtr<FJsonValue>> TriggersArray;
	for (UInputTrigger* Trigger : Mapping.Triggers)
	{
		if (Trigger)
		{
			TSharedPtr<FJsonObject> TriggerJson = MakeShared<FJsonObject>();
			TriggerJson->SetStringField(TEXT("type"), Trigger->GetClass()->GetName());
			TriggersArray.Add(MakeShared<FJsonValueObject>(TriggerJson));
		}
	}
	Json->SetArrayField(TEXT("triggers"), TriggersArray);

	// Modifiers
	TArray<TSharedPtr<FJsonValue>> ModifiersArray;
	for (UInputModifier* Modifier : Mapping.Modifiers)
	{
		if (Modifier)
		{
			TSharedPtr<FJsonObject> ModifierJson = MakeShared<FJsonObject>();
			ModifierJson->SetStringField(TEXT("type"), Modifier->GetClass()->GetName());
			ModifiersArray.Add(MakeShared<FJsonValueObject>(ModifierJson));
		}
	}
	Json->SetArrayField(TEXT("modifiers"), ModifiersArray);

	return Json;
}
