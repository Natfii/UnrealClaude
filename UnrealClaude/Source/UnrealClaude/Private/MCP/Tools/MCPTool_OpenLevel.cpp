// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_OpenLevel.h"
#include "UnrealClaudeModule.h"
#include "FileHelpers.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Editor.h"
#include "Misc/PackageName.h"

FMCPToolResult FMCPTool_OpenLevel::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Extract required action parameter
	FString Action;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("action"), Action, Error))
	{
		return Error.GetValue();
	}

	Action = Action.ToLower().TrimStartAndEnd();

	if (Action == TEXT("open"))
	{
		return ExecuteOpen(Params);
	}
	else if (Action == TEXT("new"))
	{
		return ExecuteNew(Params);
	}
	else if (Action == TEXT("save_as"))
	{
		return ExecuteSaveAs(Params);
	}
	else if (Action == TEXT("list_templates"))
	{
		return ExecuteListTemplates();
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown action: '%s'. Use 'open', 'new', 'save_as', or 'list_templates'."), *Action));
}

FMCPToolResult FMCPTool_OpenLevel::ExecuteOpen(const TSharedRef<FJsonObject>& Params)
{
	// Extract and validate level path
	FString LevelPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("level_path"), LevelPath, Error))
	{
		return Error.GetValue();
	}

	FString ValidationError;
	if (!ValidateLevelPath(LevelPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Verify the asset exists before attempting to load
	FString PackagePath = LevelPath;
	if (!PackagePath.EndsWith(TEXT(".umap")))
	{
		// Try to find the package
		if (!FPackageName::DoesPackageExist(PackagePath))
		{
			return FMCPToolResult::Error(FString::Printf(
				TEXT("Level not found: '%s'. Use asset_search to find available maps."), *LevelPath));
		}
	}

	// Resolve to filename for LoadMap
	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, Filename, FPackageName::GetMapPackageExtension()))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Could not resolve level path: '%s'"), *LevelPath));
	}

	// Load the map (engine handles save prompt internally)
	UWorld* LoadedWorld = UEditorLoadingAndSavingUtils::LoadMap(Filename);

	if (!LoadedWorld)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Failed to load level: '%s'"), *LevelPath));
	}

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("action"), TEXT("open"));
	ResultData->SetStringField(TEXT("levelPath"), LevelPath);
	ResultData->SetStringField(TEXT("mapName"), LoadedWorld->GetMapName());
	ResultData->SetStringField(TEXT("worldName"), LoadedWorld->GetName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Opened level: %s"), *LoadedWorld->GetMapName()), ResultData);
}

FMCPToolResult FMCPTool_OpenLevel::ExecuteNew(const TSharedRef<FJsonObject>& Params)
{
	FString TemplateName = ExtractOptionalString(Params, TEXT("template"));
	bool bSaveCurrent = ExtractOptionalBool(Params, TEXT("save_current"), true);

	if (TemplateName.IsEmpty())
	{
		// Create a new blank map
		UWorld* NewWorld = UEditorLoadingAndSavingUtils::NewBlankMap(bSaveCurrent);

		if (!NewWorld)
		{
			return FMCPToolResult::Error(TEXT("Failed to create new blank map."));
		}

		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetStringField(TEXT("action"), TEXT("new"));
		ResultData->SetStringField(TEXT("template"), TEXT("blank"));
		ResultData->SetStringField(TEXT("mapName"), NewWorld->GetMapName());

		return FMCPToolResult::Success(
			FString::Printf(TEXT("Created new blank map: %s"), *NewWorld->GetMapName()), ResultData);
	}

	// Create from template - find matching template
	if (!GUnrealEd)
	{
		return FMCPToolResult::Error(TEXT("Editor engine not available."));
	}

	const TArray<FTemplateMapInfo>& Templates = GUnrealEd->GetTemplateMapInfos();
	FString TemplateNameLower = TemplateName.ToLower();

	const FTemplateMapInfo* FoundTemplate = nullptr;
	for (const FTemplateMapInfo& Template : Templates)
	{
		// Match by display name or map package path
		FString DisplayName = FPaths::GetBaseFilename(Template.Map.ToString());
		if (DisplayName.ToLower() == TemplateNameLower ||
			Template.Map.ToString().ToLower().Contains(TemplateNameLower))
		{
			FoundTemplate = &Template;
			break;
		}
	}

	if (!FoundTemplate)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Template '%s' not found. Use action 'list_templates' to see available templates."),
			*TemplateName));
	}

	// Load the template map
	FString TemplateFilename;
	FString TemplatePackageName = FoundTemplate->Map.ToString();
	if (!FPackageName::TryConvertLongPackageNameToFilename(TemplatePackageName, TemplateFilename, FPackageName::GetMapPackageExtension()))
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Could not resolve template path: '%s'"), *TemplatePackageName));
	}

	UWorld* NewWorld = UEditorLoadingAndSavingUtils::LoadMap(TemplateFilename);

	if (!NewWorld)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Failed to create map from template: '%s'"), *TemplateName));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("action"), TEXT("new"));
	ResultData->SetStringField(TEXT("template"), TemplateName);
	ResultData->SetStringField(TEXT("mapName"), NewWorld->GetMapName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created new map from template '%s': %s"), *TemplateName, *NewWorld->GetMapName()),
		ResultData);
}

FMCPToolResult FMCPTool_OpenLevel::ExecuteSaveAs(const TSharedRef<FJsonObject>& Params)
{
	FString SavePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("save_path"), SavePath, Error))
	{
		return Error.GetValue();
	}

	FString ValidationError;
	if (!ValidateLevelPath(SavePath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Get current world
	if (!GEditor)
	{
		return FMCPToolResult::Error(TEXT("Editor not available"));
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FMCPToolResult::Error(TEXT("No world currently loaded"));
	}

	// Convert package path to filename
	FString Filename = FPackageName::LongPackageNameToFilename(SavePath, FPackageName::GetMapPackageExtension());

	// Save the map
	bool bSaved = FEditorFileUtils::SaveMap(World, Filename);
	if (!bSaved)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Failed to save level to: '%s'"), *SavePath));
	}

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("action"), TEXT("save_as"));
	ResultData->SetStringField(TEXT("save_path"), SavePath);
	ResultData->SetStringField(TEXT("filename"), Filename);
	ResultData->SetStringField(TEXT("mapName"), World->GetMapName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Saved level to: %s"), *SavePath), ResultData);
}

FMCPToolResult FMCPTool_OpenLevel::ExecuteListTemplates()
{
	if (!GUnrealEd)
	{
		return FMCPToolResult::Error(TEXT("Editor engine not available."));
	}

	const TArray<FTemplateMapInfo>& Templates = GUnrealEd->GetTemplateMapInfos();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> TemplateArray;

	for (const FTemplateMapInfo& Template : Templates)
	{
		TSharedPtr<FJsonObject> TemplateObj = MakeShared<FJsonObject>();

		FString DisplayName = FPaths::GetBaseFilename(Template.Map.ToString());
		TemplateObj->SetStringField(TEXT("name"), DisplayName);
		TemplateObj->SetStringField(TEXT("mapPath"), Template.Map.ToString());

		TemplateArray.Add(MakeShared<FJsonValueObject>(TemplateObj));
	}

	ResultData->SetStringField(TEXT("action"), TEXT("list_templates"));
	ResultData->SetNumberField(TEXT("count"), Templates.Num());
	ResultData->SetArrayField(TEXT("templates"), TemplateArray);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d available map templates."), Templates.Num()), ResultData);
}

bool FMCPTool_OpenLevel::ValidateLevelPath(const FString& Path, FString& OutError)
{
	if (Path.IsEmpty())
	{
		OutError = TEXT("Level path cannot be empty");
		return false;
	}

	if (Path.Len() > 512)
	{
		OutError = TEXT("Level path exceeds maximum length of 512 characters");
		return false;
	}

	// Block engine levels
	if (Path.StartsWith(TEXT("/Engine/")) || Path.StartsWith(TEXT("/Script/")))
	{
		OutError = TEXT("Cannot open engine or script levels");
		return false;
	}

	// Block path traversal
	if (Path.Contains(TEXT("..")))
	{
		OutError = TEXT("Level path cannot contain path traversal sequences");
		return false;
	}

	// Check for dangerous characters
	using namespace UnrealClaudeConstants::MCPValidation;
	int32 FoundIndex;
	for (const TCHAR* c = DangerousChars; *c; ++c)
	{
		if (Path.FindChar(*c, FoundIndex))
		{
			OutError = FString::Printf(TEXT("Level path contains invalid character: '%c'"), *c);
			return false;
		}
	}

	// Must start with /Game/ for user content
	if (!Path.StartsWith(TEXT("/Game/")))
	{
		OutError = TEXT("Level path must start with '/Game/' to reference project content");
		return false;
	}

	return true;
}
