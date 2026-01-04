// Copyright Your Name. All Rights Reserved.

#include "MCPTool_MoveActor.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

FMCPToolResult FMCPTool_MoveActor::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Validate we're in editor
	if (!GEditor)
	{
		return FMCPToolResult::Error(TEXT("Editor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FMCPToolResult::Error(TEXT("No active world"));
	}

	// Get actor name
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: actor_name"));
	}

	// Validate actor name
	FString ValidationError;
	if (!FMCPParamValidator::ValidateActorName(ActorName, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Find the actor
	AActor* Actor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* TestActor = *It;
		if (TestActor && (TestActor->GetName() == ActorName || TestActor->GetActorLabel() == ActorName))
		{
			Actor = TestActor;
			break;
		}
	}

	if (!Actor)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Get current transform
	FVector CurrentLocation = Actor->GetActorLocation();
	FRotator CurrentRotation = Actor->GetActorRotation();
	FVector CurrentScale = Actor->GetActorScale3D();

	// Check if relative mode
	bool bRelative = false;
	Params->TryGetBoolField(TEXT("relative"), bRelative);

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

	// Mark dirty
	Actor->MarkPackageDirty();
	World->MarkPackageDirty();

	// Build result with new transform
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor"), Actor->GetName());

	// Add new location
	FVector FinalLocation = Actor->GetActorLocation();
	TSharedPtr<FJsonObject> LocationResult = MakeShared<FJsonObject>();
	LocationResult->SetNumberField(TEXT("x"), FinalLocation.X);
	LocationResult->SetNumberField(TEXT("y"), FinalLocation.Y);
	LocationResult->SetNumberField(TEXT("z"), FinalLocation.Z);
	ResultData->SetObjectField(TEXT("location"), LocationResult);

	// Add new rotation
	FRotator FinalRotation = Actor->GetActorRotation();
	TSharedPtr<FJsonObject> RotationResult = MakeShared<FJsonObject>();
	RotationResult->SetNumberField(TEXT("pitch"), FinalRotation.Pitch);
	RotationResult->SetNumberField(TEXT("yaw"), FinalRotation.Yaw);
	RotationResult->SetNumberField(TEXT("roll"), FinalRotation.Roll);
	ResultData->SetObjectField(TEXT("rotation"), RotationResult);

	// Add new scale
	FVector FinalScale = Actor->GetActorScale3D();
	TSharedPtr<FJsonObject> ScaleResult = MakeShared<FJsonObject>();
	ScaleResult->SetNumberField(TEXT("x"), FinalScale.X);
	ScaleResult->SetNumberField(TEXT("y"), FinalScale.Y);
	ScaleResult->SetNumberField(TEXT("z"), FinalScale.Z);
	ResultData->SetObjectField(TEXT("scale"), ScaleResult);

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
