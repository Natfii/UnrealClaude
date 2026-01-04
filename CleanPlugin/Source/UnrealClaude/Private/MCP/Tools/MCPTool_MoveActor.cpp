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
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Get current transform
	FVector CurrentLocation = Actor->GetActorLocation();
	FRotator CurrentRotation = Actor->GetActorRotation();
	FVector CurrentScale = Actor->GetActorScale3D();

	// Check if relative mode using base class helper
	bool bRelative = ExtractOptionalBool(Params, TEXT("relative"), false);

	// Apply new location if provided
	const TSharedPtr<FJsonObject>* LocationObj;
	bool bLocationChanged = false;
	if (Params->TryGetObjectField(TEXT("location"), LocationObj))
	{
		FVector NewLocation;
		if (bRelative)
		{
			NewLocation = CurrentLocation;
			double Val;
			if ((*LocationObj)->TryGetNumberField(TEXT("x"), Val)) NewLocation.X += Val;
			if ((*LocationObj)->TryGetNumberField(TEXT("y"), Val)) NewLocation.Y += Val;
			if ((*LocationObj)->TryGetNumberField(TEXT("z"), Val)) NewLocation.Z += Val;
		}
		else
		{
			NewLocation = CurrentLocation;
			(*LocationObj)->TryGetNumberField(TEXT("x"), NewLocation.X);
			(*LocationObj)->TryGetNumberField(TEXT("y"), NewLocation.Y);
			(*LocationObj)->TryGetNumberField(TEXT("z"), NewLocation.Z);
		}
		Actor->SetActorLocation(NewLocation);
		bLocationChanged = true;
	}

	// Apply new rotation if provided
	const TSharedPtr<FJsonObject>* RotationObj;
	bool bRotationChanged = false;
	if (Params->TryGetObjectField(TEXT("rotation"), RotationObj))
	{
		FRotator NewRotation;
		if (bRelative)
		{
			NewRotation = CurrentRotation;
			double Val;
			if ((*RotationObj)->TryGetNumberField(TEXT("pitch"), Val)) NewRotation.Pitch += Val;
			if ((*RotationObj)->TryGetNumberField(TEXT("yaw"), Val)) NewRotation.Yaw += Val;
			if ((*RotationObj)->TryGetNumberField(TEXT("roll"), Val)) NewRotation.Roll += Val;
		}
		else
		{
			NewRotation = CurrentRotation;
			(*RotationObj)->TryGetNumberField(TEXT("pitch"), NewRotation.Pitch);
			(*RotationObj)->TryGetNumberField(TEXT("yaw"), NewRotation.Yaw);
			(*RotationObj)->TryGetNumberField(TEXT("roll"), NewRotation.Roll);
		}
		Actor->SetActorRotation(NewRotation);
		bRotationChanged = true;
	}

	// Apply new scale if provided
	const TSharedPtr<FJsonObject>* ScaleObj;
	bool bScaleChanged = false;
	if (Params->TryGetObjectField(TEXT("scale"), ScaleObj))
	{
		FVector NewScale;
		if (bRelative)
		{
			NewScale = CurrentScale;
			double Val;
			if ((*ScaleObj)->TryGetNumberField(TEXT("x"), Val)) NewScale.X *= Val;
			if ((*ScaleObj)->TryGetNumberField(TEXT("y"), Val)) NewScale.Y *= Val;
			if ((*ScaleObj)->TryGetNumberField(TEXT("z"), Val)) NewScale.Z *= Val;
		}
		else
		{
			NewScale = CurrentScale;
			(*ScaleObj)->TryGetNumberField(TEXT("x"), NewScale.X);
			(*ScaleObj)->TryGetNumberField(TEXT("y"), NewScale.Y);
			(*ScaleObj)->TryGetNumberField(TEXT("z"), NewScale.Z);
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
