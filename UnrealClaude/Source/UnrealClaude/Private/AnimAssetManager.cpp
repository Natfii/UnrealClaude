// Copyright Natali Caggiano. All Rights Reserved.

#include "AnimAssetManager.h"
#include "AnimStateMachineEditor.h"
#include "AnimGraphEditor.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimStateNode.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "UObject/UObjectGlobals.h"

// ===== Asset Loading =====
// All methods delegate to LoadAnimAssetInternal<T> template to eliminate duplication

UAnimSequence* FAnimAssetManager::LoadAnimSequence(const FString& AssetPath, FString& OutError)
{
	return LoadAnimAssetInternal<UAnimSequence>(AssetPath, TEXT("AnimSequence"), OutError);
}

UBlendSpace* FAnimAssetManager::LoadBlendSpace(const FString& AssetPath, FString& OutError)
{
	return LoadAnimAssetInternal<UBlendSpace>(AssetPath, TEXT("BlendSpace"), OutError);
}

UBlendSpace1D* FAnimAssetManager::LoadBlendSpace1D(const FString& AssetPath, FString& OutError)
{
	return LoadAnimAssetInternal<UBlendSpace1D>(AssetPath, TEXT("BlendSpace1D"), OutError);
}

UAnimMontage* FAnimAssetManager::LoadMontage(const FString& AssetPath, FString& OutError)
{
	return LoadAnimAssetInternal<UAnimMontage>(AssetPath, TEXT("AnimMontage"), OutError);
}

UAnimationAsset* FAnimAssetManager::LoadAnimationAsset(const FString& AssetPath, FString& OutError)
{
	return LoadAnimAssetInternal<UAnimationAsset>(AssetPath, TEXT("AnimationAsset"), OutError);
}

// ===== Asset Validation =====

bool FAnimAssetManager::ValidateAnimationCompatibility(
	UAnimBlueprint* AnimBP,
	UAnimationAsset* AnimAsset,
	FString& OutError)
{
	if (!AnimBP)
	{
		OutError = TEXT("Invalid Animation Blueprint");
		return false;
	}

	if (!AnimAsset)
	{
		OutError = TEXT("Invalid animation asset");
		return false;
	}

	USkeleton* BPSkeleton = GetTargetSkeleton(AnimBP);
	USkeleton* AssetSkeleton = AnimAsset->GetSkeleton();

	if (!BPSkeleton)
	{
		OutError = TEXT("Animation Blueprint has no target skeleton");
		return false;
	}

	if (!AssetSkeleton)
	{
		OutError = TEXT("Animation asset has no skeleton");
		return false;
	}

	// Check skeleton compatibility
	if (!BPSkeleton->IsCompatibleForEditor(AssetSkeleton))
	{
		OutError = FString::Printf(
			TEXT("Skeleton mismatch: AnimBlueprint uses '%s', but asset uses '%s'"),
			*BPSkeleton->GetName(),
			*AssetSkeleton->GetName()
		);
		return false;
	}

	return true;
}

USkeleton* FAnimAssetManager::GetTargetSkeleton(UAnimBlueprint* AnimBP)
{
	if (!AnimBP) return nullptr;
	return AnimBP->TargetSkeleton.Get();
}

// ===== State Animation Assignment =====

bool FAnimAssetManager::SetStateAnimSequence(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	UAnimSequence* AnimSequence,
	FString& OutError)
{
	// Validate compatibility
	if (!ValidateAnimationCompatibility(AnimBP, AnimSequence, OutError))
	{
		return false;
	}

	// Find state's bound graph
	UEdGraph* StateGraph = FAnimGraphEditor::FindStateBoundGraph(AnimBP, StateMachineName, StateName, OutError);
	if (!StateGraph)
	{
		return false;
	}

	// Clear existing content
	if (!FAnimGraphEditor::ClearStateGraph(StateGraph, OutError))
	{
		return false;
	}

	// Create sequence player node
	FString NodeId;
	UEdGraphNode* SeqNode = FAnimGraphEditor::CreateAnimSequenceNode(
		StateGraph,
		AnimSequence,
		FVector2D(200, 100),
		NodeId,
		OutError
	);

	if (!SeqNode)
	{
		return false;
	}

	// Connect to output pose
	return FAnimGraphEditor::ConnectToOutputPose(StateGraph, NodeId, OutError);
}

bool FAnimAssetManager::SetStateBlendSpace(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	UBlendSpace* BlendSpace,
	const TMap<FString, FString>& ParameterBindings,
	FString& OutError)
{
	// Validate compatibility
	if (!ValidateAnimationCompatibility(AnimBP, BlendSpace, OutError))
	{
		return false;
	}

	// Find state's bound graph
	UEdGraph* StateGraph = FAnimGraphEditor::FindStateBoundGraph(AnimBP, StateMachineName, StateName, OutError);
	if (!StateGraph)
	{
		return false;
	}

	// Clear existing content
	if (!FAnimGraphEditor::ClearStateGraph(StateGraph, OutError))
	{
		return false;
	}

	// Create BlendSpace player node
	FString NodeId;
	UEdGraphNode* BSNode = FAnimGraphEditor::CreateBlendSpaceNode(
		StateGraph,
		BlendSpace,
		FVector2D(200, 100),
		NodeId,
		OutError
	);

	if (!BSNode)
	{
		return false;
	}

	// TODO: Bind parameters to variables
	// This requires creating variable get nodes and connecting them to the BlendSpace input pins
	// For now, we'll just create the BlendSpace node and connect to output

	// Connect to output pose
	return FAnimGraphEditor::ConnectToOutputPose(StateGraph, NodeId, OutError);
}

bool FAnimAssetManager::SetStateBlendSpace1D(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	UBlendSpace1D* BlendSpace,
	const FString& ParameterBinding,
	FString& OutError)
{
	// Validate compatibility
	if (!ValidateAnimationCompatibility(AnimBP, BlendSpace, OutError))
	{
		return false;
	}

	// Find state's bound graph
	UEdGraph* StateGraph = FAnimGraphEditor::FindStateBoundGraph(AnimBP, StateMachineName, StateName, OutError);
	if (!StateGraph)
	{
		return false;
	}

	// Clear existing content
	if (!FAnimGraphEditor::ClearStateGraph(StateGraph, OutError))
	{
		return false;
	}

	// Create BlendSpace1D player node
	FString NodeId;
	UEdGraphNode* BSNode = FAnimGraphEditor::CreateBlendSpace1DNode(
		StateGraph,
		BlendSpace,
		FVector2D(200, 100),
		NodeId,
		OutError
	);

	if (!BSNode)
	{
		return false;
	}

	// Connect to output pose
	return FAnimGraphEditor::ConnectToOutputPose(StateGraph, NodeId, OutError);
}

bool FAnimAssetManager::SetStateMontage(
	UAnimBlueprint* AnimBP,
	const FString& StateMachineName,
	const FString& StateName,
	UAnimMontage* Montage,
	FString& OutError)
{
	// Validate compatibility
	if (!ValidateAnimationCompatibility(AnimBP, Montage, OutError))
	{
		return false;
	}

	// Find state's bound graph
	UEdGraph* StateGraph = FAnimGraphEditor::FindStateBoundGraph(AnimBP, StateMachineName, StateName, OutError);
	if (!StateGraph)
	{
		return false;
	}

	// For montage, we typically don't replace the state graph
	// Instead, montages are played via AnimInstance->Montage_Play()
	// But we can set up a montage slot node if needed

	OutError = TEXT("Montage assignment to states not yet implemented. Use PlayMontage via AnimInstance.");
	return false;
}

// ===== Asset Discovery =====

TArray<FString> FAnimAssetManager::FindAnimationAssets(
	const FString& SearchPattern,
	const FString& AssetType,
	USkeleton* TargetSkeleton)
{
	TArray<FString> Results;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Determine class filter
	TArray<FTopLevelAssetPath> ClassPaths;
	if (AssetType.Equals(TEXT("AnimSequence"), ESearchCase::IgnoreCase))
	{
		ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	}
	else if (AssetType.Equals(TEXT("BlendSpace"), ESearchCase::IgnoreCase))
	{
		ClassPaths.Add(UBlendSpace::StaticClass()->GetClassPathName());
	}
	else if (AssetType.Equals(TEXT("BlendSpace1D"), ESearchCase::IgnoreCase))
	{
		ClassPaths.Add(UBlendSpace1D::StaticClass()->GetClassPathName());
	}
	else if (AssetType.Equals(TEXT("Montage"), ESearchCase::IgnoreCase))
	{
		ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	}
	else // All
	{
		ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
		ClassPaths.Add(UBlendSpace::StaticClass()->GetClassPathName());
		ClassPaths.Add(UBlendSpace1D::StaticClass()->GetClassPathName());
		ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	}

	// Query assets
	FARFilter Filter;
	Filter.ClassPaths = ClassPaths;
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(TEXT("/Game")));

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	// Filter by pattern and skeleton
	for (const FAssetData& AssetData : AssetDataList)
	{
		FString AssetName = AssetData.AssetName.ToString();

		// Check pattern match
		if (!SearchPattern.IsEmpty() && !AssetName.Contains(SearchPattern))
		{
			continue;
		}

		// Check skeleton compatibility
		if (TargetSkeleton)
		{
			UAnimationAsset* Asset = Cast<UAnimationAsset>(AssetData.GetAsset());
			if (Asset && Asset->GetSkeleton() != TargetSkeleton)
			{
				continue;
			}
		}

		Results.Add(AssetData.GetObjectPathString());
	}

	return Results;
}

// ===== Serialization =====

TSharedPtr<FJsonObject> FAnimAssetManager::SerializeAnimAssetInfo(UAnimationAsset* Asset)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!Asset) return Json;

	Json->SetStringField(TEXT("name"), Asset->GetName());
	Json->SetStringField(TEXT("path"), Asset->GetPathName());
	Json->SetStringField(TEXT("class"), Asset->GetClass()->GetName());

	if (USkeleton* Skeleton = Asset->GetSkeleton())
	{
		Json->SetStringField(TEXT("skeleton"), Skeleton->GetName());
	}

	// Add type-specific info
	if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
	{
		Json->SetNumberField(TEXT("length"), Sequence->GetPlayLength());
		Json->SetNumberField(TEXT("num_frames"), Sequence->GetNumberOfSampledKeys());
	}
	else if (UBlendSpace* BS = Cast<UBlendSpace>(Asset))
	{
		Json = SerializeBlendSpaceInfo(BS);
	}

	return Json;
}

TSharedPtr<FJsonObject> FAnimAssetManager::SerializeBlendSpaceInfo(UBlendSpace* BlendSpace)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!BlendSpace) return Json;

	Json->SetStringField(TEXT("name"), BlendSpace->GetName());
	Json->SetStringField(TEXT("path"), BlendSpace->GetPathName());
	Json->SetStringField(TEXT("class"), BlendSpace->GetClass()->GetName());

	// Add axis info
	TSharedPtr<FJsonObject> AxisXJson = MakeShared<FJsonObject>();
	AxisXJson->SetStringField(TEXT("name"), BlendSpace->GetBlendParameter(0).DisplayName);
	AxisXJson->SetNumberField(TEXT("min"), BlendSpace->GetBlendParameter(0).Min);
	AxisXJson->SetNumberField(TEXT("max"), BlendSpace->GetBlendParameter(0).Max);
	Json->SetObjectField(TEXT("axis_x"), AxisXJson);

	// Check if 2D
	if (!BlendSpace->IsA<UBlendSpace1D>())
	{
		TSharedPtr<FJsonObject> AxisYJson = MakeShared<FJsonObject>();
		AxisYJson->SetStringField(TEXT("name"), BlendSpace->GetBlendParameter(1).DisplayName);
		AxisYJson->SetNumberField(TEXT("min"), BlendSpace->GetBlendParameter(1).Min);
		AxisYJson->SetNumberField(TEXT("max"), BlendSpace->GetBlendParameter(1).Max);
		Json->SetObjectField(TEXT("axis_y"), AxisYJson);
	}

	return Json;
}

// ===== Private Helpers =====

FString FAnimAssetManager::ResolveAnimAssetPath(const FString& AssetPath)
{
	// If already a full path, return as-is
	if (AssetPath.StartsWith(TEXT("/Game/")) || AssetPath.StartsWith(TEXT("/Script/")))
	{
		return AssetPath;
	}

	// Try to construct full path
	FString FullPath = TEXT("/Game/Animations/") + AssetPath;

	// Ensure proper asset reference format (Path.AssetName)
	if (!FullPath.Contains(TEXT(".")))
	{
		FullPath += TEXT(".") + FPaths::GetBaseFilename(AssetPath);
	}

	return FullPath;
}

TArray<FString> FAnimAssetManager::GetCommonSearchPaths()
{
	return TArray<FString>{
		TEXT("/Game/Animations"),
		TEXT("/Game/Characters"),
		TEXT("/Game/Characters/Animations"),
		TEXT("/Game/Assets/Animations"),
		TEXT("/Game")
	};
}

bool FAnimAssetManager::ClearAndSetupStateGraph(UEdGraph* StateGraph, FString& OutError)
{
	return FAnimGraphEditor::ClearStateGraph(StateGraph, OutError);
}
