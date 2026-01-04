// Copyright Your Name. All Rights Reserved.

#include "MCPTool_MoveActor.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeUtils.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

FMCPToolResult FMCPTool_MoveActor::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Validate editor context using base class
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	// Extract and validate actor name using base class helper
	FString ActorName;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, ParamError))
	{
		return ParamError.GetValue();
	}

	// Find the actor using base class helper
	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor)
	{
		return ActorNotFoundError(ActorName);
	}

	// Get current transform
	FVector CurrentLocation = Actor->GetActorLocation();
	FRotator CurrentRotation = Actor->GetActorRotation();
	FVector CurrentScale = Actor->GetActorScale3D();

	// Check if relative mode using base class helper
	bool bRelative = ExtractOptionalBool(Params, TEXT("relative"), false);

	// Apply new location if provided (using base class transform helpers)
	FVector NewLocation = CurrentLocation;
	bool bLocationChanged = ExtractVectorComponents(Params, TEXT("location"), NewLocation, bRelative);
	if (bLocationChanged)
	{
		Actor->SetActorLocation(NewLocation);
	}

	// Apply new rotation if provided (using base class transform helpers)
	FRotator NewRotation = CurrentRotation;
	bool bRotationChanged = ExtractRotatorComponents(Params, TEXT("rotation"), NewRotation, bRelative);
	if (bRotationChanged)
	{
		Actor->SetActorRotation(NewRotation);
	}

	// Apply new scale if provided
	// Note: Scale uses multiplicative relative mode, handled specially
	FVector NewScale = CurrentScale;
	bool bScaleChanged = false;
	if (HasVectorParam(Params, TEXT("scale")))
	{
		if (bRelative)
		{
			// Multiplicative scale for relative mode
			FVector ScaleMultiplier = ExtractVectorParam(Params, TEXT("scale"), FVector::OneVector);
			NewScale = CurrentScale * ScaleMultiplier;
		}
		else
		{
			// Absolute scale replacement
			ExtractVectorComponents(Params, TEXT("scale"), NewScale, false);
		}
		Actor->SetActorScale3D(NewScale);
		bScaleChanged = true;
	}

	// Check if anything changed
	if (!bLocationChanged && !bRotationChanged && !bScaleChanged)
	{
		return FMCPToolResult::Error(TEXT("No transform changes specified. Provide location, rotation, or scale."));
	}

	// Mark dirty using base class helper
	Actor->MarkPackageDirty();
	MarkWorldDirty(World);

	// Build result with new transform using shared utilities
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor"), Actor->GetName());
	ResultData->SetObjectField(TEXT("location"), UnrealClaudeJsonUtils::VectorToJson(Actor->GetActorLocation()));
	ResultData->SetObjectField(TEXT("rotation"), UnrealClaudeJsonUtils::RotatorToJson(Actor->GetActorRotation()));
	ResultData->SetObjectField(TEXT("scale"), UnrealClaudeJsonUtils::VectorToJson(Actor->GetActorScale3D()));

	// Build change description
	TArray<FString> Changes;
	if (bLocationChanged) Changes.Add(TEXT("location"));
	if (bRotationChanged) Changes.Add(TEXT("rotation"));
	if (bScaleChanged) Changes.Add(TEXT("scale"));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Updated %s for actor '%s'"), *FString::Join(Changes, TEXT(", ")), *Actor->GetName()),
		ResultData
	);
}
