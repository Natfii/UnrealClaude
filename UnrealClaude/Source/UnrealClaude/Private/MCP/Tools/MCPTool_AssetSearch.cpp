// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_AssetSearch.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

FMCPToolResult FMCPTool_AssetSearch::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Get AssetRegistry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Extract parameters
	FString ClassFilter = ExtractOptionalString(Params, TEXT("class_filter"));
	FString PathFilter = ExtractOptionalString(Params, TEXT("path_filter"), TEXT("/Game/"));
	FString NamePattern = ExtractOptionalString(Params, TEXT("name_pattern"));
	int32 Limit = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("limit"), 25), 1, 1000);
	int32 Offset = FMath::Max(0, ExtractOptionalNumber<int32>(Params, TEXT("offset"), 0));

	// Build FARFilter
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;

	// Apply path filter
	if (!PathFilter.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*PathFilter));
	}

	// Apply class filter
	if (!ClassFilter.IsEmpty())
	{
		// Try to resolve class path - handle both full paths and short names
		FString ClassPath = ClassFilter;

		// If it's a short name, try common prefixes
		if (!ClassPath.StartsWith(TEXT("/")))
		{
			// Try /Script/Engine first (common classes like StaticMesh, Blueprint)
			UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassFilter));

			if (!FoundClass)
			{
				// Try /Script/CoreUObject
				FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassFilter));
			}

			if (!FoundClass)
			{
				// Try /Script/Niagara for particle systems
				FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Niagara.%s"), *ClassFilter));
			}

			if (!FoundClass)
			{
				// Try direct find
				FoundClass = FindObject<UClass>(nullptr, *ClassFilter);
			}

			if (FoundClass)
			{
				ClassPath = FoundClass->GetClassPathName().ToString();
			}
			else
			{
				// Build path manually as fallback
				ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassFilter);
			}
		}

		Filter.ClassPaths.Add(FTopLevelAssetPath(ClassPath));
	}

	// Query assets
	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAssets(Filter, AllAssets);

	// Apply name pattern filter (post-query, case-insensitive)
	TArray<FAssetData> FilteredAssets;
	if (!NamePattern.IsEmpty())
	{
		for (const FAssetData& Asset : AllAssets)
		{
			if (Asset.AssetName.ToString().Contains(NamePattern, ESearchCase::IgnoreCase))
			{
				FilteredAssets.Add(Asset);
			}
		}
	}
	else
	{
		FilteredAssets = MoveTemp(AllAssets);
	}

	// Calculate pagination
	int32 Total = FilteredAssets.Num();
	int32 StartIndex = FMath::Min(Offset, Total);
	int32 EndIndex = FMath::Min(StartIndex + Limit, Total);
	int32 Count = EndIndex - StartIndex;
	bool bHasMore = EndIndex < Total;

	// Build result array
	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (int32 i = StartIndex; i < EndIndex; ++i)
	{
		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetDataToJson(FilteredAssets[i])));
	}

	// Build result data
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("assets"), AssetsArray);
	ResultData->SetNumberField(TEXT("count"), Count);
	ResultData->SetNumberField(TEXT("total"), Total);
	ResultData->SetNumberField(TEXT("offset"), StartIndex);
	ResultData->SetNumberField(TEXT("limit"), Limit);
	ResultData->SetBoolField(TEXT("hasMore"), bHasMore);
	if (bHasMore)
	{
		ResultData->SetNumberField(TEXT("nextOffset"), EndIndex);
	}

	// Build message
	FString Message;
	if (Total == 0)
	{
		Message = TEXT("No assets found matching the search criteria");
	}
	else if (Count == Total)
	{
		Message = FString::Printf(TEXT("Found %d asset%s"), Total, Total == 1 ? TEXT("") : TEXT("s"));
	}
	else
	{
		Message = FString::Printf(TEXT("Found %d assets (showing %d-%d of %d total)"),
			Count, StartIndex + 1, EndIndex, Total);
	}

	return FMCPToolResult::Success(Message, ResultData);
}

TSharedPtr<FJsonObject> FMCPTool_AssetSearch::AssetDataToJson(const FAssetData& AssetData) const
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	// Full object path (e.g., /Game/Characters/BP_Player.BP_Player)
	Json->SetStringField(TEXT("path"), AssetData.GetObjectPathString());

	// Asset name without path
	Json->SetStringField(TEXT("name"), AssetData.AssetName.ToString());

	// Class name (short form)
	Json->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());

	// Package path (folder containing the asset)
	Json->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());

	return Json;
}
