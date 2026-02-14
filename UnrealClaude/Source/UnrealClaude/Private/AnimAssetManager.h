// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimBlueprint.h"
#include "Dom/JsonObject.h"

// Forward declarations
class UAnimSequence;
class UBlendSpace;
class UBlendSpace1D;
class UAnimMontage;
class UAnimationAsset;
class USkeleton;
class UAnimStateNode;
class UEdGraph;

/**
 * Animation Asset Manager
 *
 * Responsibilities:
 * - Loading animation assets (sequences, blend spaces, montages)
 * - Validating animation asset compatibility with skeleton
 * - Setting animation references in states
 * - Parameter binding for blend spaces
 *
 * Supported Asset Types:
 * - UAnimSequence: Single animation clip
 * - UBlendSpace: 2D blend space (Direction + Speed)
 * - UBlendSpace1D: 1D blend space (single parameter)
 * - UAnimMontage: Montage for triggered animations
 */
class FAnimAssetManager
{
public:
	// ===== Asset Loading =====

	/**
	 * Load animation sequence by path
	 * Supports full paths (/Game/...) and short names
	 * @param AssetPath - Full path or short name
	 * @param OutError - Error message if failed
	 * @return Loaded sequence or nullptr
	 */
	static UAnimSequence* LoadAnimSequence(const FString& AssetPath, FString& OutError);

	/**
	 * Load BlendSpace (2D)
	 */
	static UBlendSpace* LoadBlendSpace(const FString& AssetPath, FString& OutError);

	/**
	 * Load BlendSpace1D
	 */
	static UBlendSpace1D* LoadBlendSpace1D(const FString& AssetPath, FString& OutError);

	/**
	 * Load animation montage
	 */
	static UAnimMontage* LoadMontage(const FString& AssetPath, FString& OutError);

	/**
	 * Load any animation asset (auto-detect type)
	 */
	static UAnimationAsset* LoadAnimationAsset(const FString& AssetPath, FString& OutError);

	// ===== Asset Validation =====

	/**
	 * Validate that animation asset is compatible with AnimBlueprint skeleton
	 */
	static bool ValidateAnimationCompatibility(
		UAnimBlueprint* AnimBP,
		UAnimationAsset* AnimAsset,
		FString& OutError
	);

	/**
	 * Get skeleton from AnimBlueprint
	 */
	static USkeleton* GetTargetSkeleton(UAnimBlueprint* AnimBP);

	// ===== State Animation Assignment (Level 5) =====

	/**
	 * Set animation sequence for a state
	 * Clears existing animation and sets new sequence player
	 * @param AnimBP - Animation Blueprint
	 * @param StateMachineName - State machine containing the state
	 * @param StateName - Target state name
	 * @param AnimSequence - Animation sequence to assign
	 * @param OutError - Error message if failed
	 * @return true if successful
	 */
	static bool SetStateAnimSequence(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		UAnimSequence* AnimSequence,
		FString& OutError
	);

	/**
	 * Set BlendSpace for a state
	 * @param ParameterBindings - Map of BlendSpace parameter names to Blueprint variable names
	 *                           e.g., { "X": "Speed", "Y": "Direction" }
	 */
	static bool SetStateBlendSpace(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		UBlendSpace* BlendSpace,
		const TMap<FString, FString>& ParameterBindings,
		FString& OutError
	);

	/**
	 * Set BlendSpace1D for a state
	 */
	static bool SetStateBlendSpace1D(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		UBlendSpace1D* BlendSpace,
		const FString& ParameterBinding,
		FString& OutError
	);

	/**
	 * Set montage for a state
	 */
	static bool SetStateMontage(
		UAnimBlueprint* AnimBP,
		const FString& StateMachineName,
		const FString& StateName,
		UAnimMontage* Montage,
		FString& OutError
	);

	// ===== Asset Discovery =====

	/**
	 * Find animation assets in project
	 * @param SearchPattern - Name pattern to match
	 * @param AssetType - "AnimSequence", "BlendSpace", "BlendSpace1D", "Montage", or "All"
	 * @param TargetSkeleton - Optional skeleton to filter by
	 * @return Array of asset paths
	 */
	static TArray<FString> FindAnimationAssets(
		const FString& SearchPattern,
		const FString& AssetType = TEXT("All"),
		USkeleton* TargetSkeleton = nullptr
	);

	// ===== Serialization =====

	static TSharedPtr<FJsonObject> SerializeAnimAssetInfo(UAnimationAsset* Asset);
	static TSharedPtr<FJsonObject> SerializeBlendSpaceInfo(UBlendSpace* BlendSpace);

private:
	// Asset path resolution
	static FString ResolveAnimAssetPath(const FString& AssetPath);
	static TArray<FString> GetCommonSearchPaths();

	// Graph modification helpers
	static bool ClearAndSetupStateGraph(UEdGraph* StateGraph, FString& OutError);

	/**
	 * Template method for loading animation assets
	 * Eliminates code duplication across LoadAnimSequence, LoadBlendSpace, etc.
	 * @param AssetPath - Full path or short name
	 * @param AssetTypeName - Human-readable type name for error messages
	 * @param OutError - Error message if failed
	 * @return Loaded asset or nullptr
	 */
	template<typename T>
	static T* LoadAnimAssetInternal(const FString& AssetPath, const FString& AssetTypeName, FString& OutError)
	{
		FString ResolvedPath = ResolveAnimAssetPath(AssetPath);

		T* Asset = LoadObject<T>(nullptr, *ResolvedPath);

		if (!Asset)
		{
			// Try common search paths
			for (const FString& SearchPath : GetCommonSearchPaths())
			{
				FString FullPath = SearchPath / AssetPath;
				if (!FullPath.EndsWith(TEXT(".") + AssetPath))
				{
					FullPath += TEXT(".") + FPaths::GetBaseFilename(AssetPath);
				}
				Asset = LoadObject<T>(nullptr, *FullPath);
				if (Asset) break;
			}
		}

		if (!Asset)
		{
			OutError = FString::Printf(
				TEXT("Failed to load %s '%s'. Use full path like '/Game/Animations/MyAsset.MyAsset'."),
				*AssetTypeName, *AssetPath
			);
		}

		return Asset;
	}
};
