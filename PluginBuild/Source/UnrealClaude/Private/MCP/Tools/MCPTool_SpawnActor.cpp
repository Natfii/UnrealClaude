// Copyright Your Name. All Rights Reserved.

#include "MCPTool_SpawnActor.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Blueprint.h"
#include "Kismet/GameplayStatics.h"

FMCPToolResult FMCPTool_SpawnActor::Execute(const TSharedRef<FJsonObject>& Params)
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

	// Get class path
	FString ClassPath;
	if (!Params->TryGetStringField(TEXT("class"), ClassPath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: class"));
	}

	// Validate class path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateClassPath(ClassPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Try to find the class
	UClass* ActorClass = nullptr;

	// First, try loading as a full path
	ActorClass = LoadClass<AActor>(nullptr, *ClassPath);

	// If not found, try common prefixes
	if (!ActorClass)
	{
		// Try with /Script/Engine. prefix
		ActorClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassPath));
	}

	if (!ActorClass)
	{
		// Try with /Script/CoreUObject. prefix
		ActorClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassPath));
	}

	if (!ActorClass)
	{
		// Try finding by short name in any package
		ActorClass = FindObject<UClass>(nullptr, *ClassPath);
	}

	if (!ActorClass)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Could not find actor class: %s"), *ClassPath));
	}

	// Parse location
	FVector Location = FVector::ZeroVector;
	const TSharedPtr<FJsonObject>* LocationObj;
	if (Params->TryGetObjectField(TEXT("location"), LocationObj))
	{
		(*LocationObj)->TryGetNumberField(TEXT("x"), Location.X);
		(*LocationObj)->TryGetNumberField(TEXT("y"), Location.Y);
		(*LocationObj)->TryGetNumberField(TEXT("z"), Location.Z);
	}

	// Parse rotation
	FRotator Rotation = FRotator::ZeroRotator;
	const TSharedPtr<FJsonObject>* RotationObj;
	if (Params->TryGetObjectField(TEXT("rotation"), RotationObj))
	{
		(*RotationObj)->TryGetNumberField(TEXT("pitch"), Rotation.Pitch);
		(*RotationObj)->TryGetNumberField(TEXT("yaw"), Rotation.Yaw);
		(*RotationObj)->TryGetNumberField(TEXT("roll"), Rotation.Roll);
	}

	// Parse scale
	FVector Scale = FVector::OneVector;
	const TSharedPtr<FJsonObject>* ScaleObj;
	if (Params->TryGetObjectField(TEXT("scale"), ScaleObj))
	{
		(*ScaleObj)->TryGetNumberField(TEXT("x"), Scale.X);
		(*ScaleObj)->TryGetNumberField(TEXT("y"), Scale.Y);
		(*ScaleObj)->TryGetNumberField(TEXT("z"), Scale.Z);
	}

	// Get optional name
	FString ActorName;
	Params->TryGetStringField(TEXT("name"), ActorName);

	// Validate actor name if provided
	if (!ActorName.IsEmpty())
	{
		if (!FMCPParamValidator::ValidateActorName(ActorName, ValidationError))
		{
			return FMCPToolResult::Error(ValidationError);
		}
	}

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	if (!ActorName.IsEmpty())
	{
		SpawnParams.Name = FName(*ActorName);
	}
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FTransform SpawnTransform(Rotation, Location, Scale);

	AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);

	if (!SpawnedActor)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to spawn actor of class: %s"), *ClassPath));
	}

	// Mark the level as dirty
	World->MarkPackageDirty();

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actorName"), SpawnedActor->GetName());
	ResultData->SetStringField(TEXT("actorClass"), ActorClass->GetName());
	ResultData->SetStringField(TEXT("actorLabel"), SpawnedActor->GetActorLabel());

	TSharedPtr<FJsonObject> LocationResult = MakeShared<FJsonObject>();
	LocationResult->SetNumberField(TEXT("x"), Location.X);
	LocationResult->SetNumberField(TEXT("y"), Location.Y);
	LocationResult->SetNumberField(TEXT("z"), Location.Z);
	ResultData->SetObjectField(TEXT("location"), LocationResult);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Spawned actor '%s' of class '%s'"), *SpawnedActor->GetName(), *ActorClass->GetName()),
		ResultData
	);
}
