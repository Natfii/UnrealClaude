// Copyright Your Name. All Rights Reserved.

#include "BlueprintLoader.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetToolsModule.h"
#include "Factories/BlueprintFactory.h"

UBlueprint* FBlueprintLoader::LoadBlueprint(const FString& BlueprintPath, FString& OutError)
{
	if (BlueprintPath.IsEmpty())
	{
		OutError = TEXT("Blueprint path cannot be empty");
		return nullptr;
	}

	// Try to load the Blueprint directly
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);

	// If not found, try with /Game/ prefix
	if (!Blueprint)
	{
		FString AdjustedPath = BlueprintPath;
		if (!AdjustedPath.StartsWith(TEXT("/")))
		{
			AdjustedPath = TEXT("/Game/") + AdjustedPath;
		}
		Blueprint = LoadObject<UBlueprint>(nullptr, *AdjustedPath);
	}

	if (!Blueprint)
	{
		OutError = FString::Printf(TEXT("Could not load Blueprint: %s"), *BlueprintPath);
		return nullptr;
	}

	return Blueprint;
}

bool FBlueprintLoader::ValidateBlueprintPath(const FString& BlueprintPath, FString& OutError)
{
	// Delegate to MCPParamValidator for comprehensive security validation
	return FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, OutError);
}

bool FBlueprintLoader::IsBlueprintEditable(UBlueprint* Blueprint, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UPackage* Package = Blueprint->GetPackage();
	if (!Package)
	{
		OutError = TEXT("Blueprint package is invalid");
		return false;
	}

	// Block engine Blueprints
	FString PackageName = Package->GetName();
	if (PackageName.StartsWith(TEXT("/Engine/")) || PackageName.StartsWith(TEXT("/Script/")))
	{
		OutError = TEXT("Cannot modify engine Blueprints");
		return false;
	}

	// Block cooked packages
	if (Package->HasAnyPackageFlags(PKG_Cooked))
	{
		OutError = TEXT("Blueprint package is read-only (cooked)");
		return false;
	}

	return true;
}

bool FBlueprintLoader::CompileBlueprint(UBlueprint* Blueprint, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	// Mark as modified before compilation
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	// Compile
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// Check for errors
	if (Blueprint->Status == BS_Error)
	{
		OutError = TEXT("Blueprint compilation failed");
		return false;
	}

	return true;
}

void FBlueprintLoader::MarkBlueprintDirty(UBlueprint* Blueprint)
{
	if (Blueprint)
	{
		Blueprint->MarkPackageDirty();
	}
}

UBlueprint* FBlueprintLoader::CreateBlueprint(
	const FString& PackagePath,
	const FString& BlueprintName,
	UClass* ParentClass,
	EBlueprintType BlueprintType,
	FString& OutError)
{
	if (PackagePath.IsEmpty() || BlueprintName.IsEmpty())
	{
		OutError = TEXT("Package path and Blueprint name are required");
		return nullptr;
	}

	if (!ParentClass)
	{
		OutError = TEXT("Parent class is required");
		return nullptr;
	}

	// Create the package path
	FString FullPath = PackagePath / BlueprintName;
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
		return nullptr;
	}

	// Create Blueprint factory
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClass;
	Factory->BlueprintType = BlueprintType;

	// Create the Blueprint
	UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
		UBlueprint::StaticClass(),
		Package,
		FName(*BlueprintName),
		RF_Public | RF_Standalone,
		nullptr,
		GWarn
	));

	if (!NewBlueprint)
	{
		OutError = TEXT("Failed to create Blueprint");
		return nullptr;
	}

	// Mark package dirty
	Package->MarkPackageDirty();

	UE_LOG(LogUnrealClaude, Log, TEXT("Created Blueprint: %s (Parent: %s)"),
		*FullPath, *ParentClass->GetName());

	return NewBlueprint;
}

UClass* FBlueprintLoader::FindParentClass(const FString& ParentClassName, FString& OutError)
{
	if (ParentClassName.IsEmpty())
	{
		OutError = TEXT("Parent class name cannot be empty");
		return nullptr;
	}

	UClass* ParentClass = nullptr;

	// Try full path first
	ParentClass = LoadClass<UObject>(nullptr, *ParentClassName);

	// Try common engine prefixes
	if (!ParentClass)
	{
		ParentClass = LoadClass<UObject>(nullptr,
			*FString::Printf(TEXT("/Script/Engine.%s"), *ParentClassName));
	}

	if (!ParentClass)
	{
		ParentClass = LoadClass<UObject>(nullptr,
			*FString::Printf(TEXT("/Script/CoreUObject.%s"), *ParentClassName));
	}

	// Try finding by short name
	if (!ParentClass)
	{
		ParentClass = FindObject<UClass>(nullptr, *ParentClassName);
	}

	if (!ParentClass)
	{
		OutError = FString::Printf(TEXT("Parent class not found: %s"), *ParentClassName);
		return nullptr;
	}

	return ParentClass;
}
