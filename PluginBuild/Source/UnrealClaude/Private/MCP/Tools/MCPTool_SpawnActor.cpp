// Copyright Your Name. All Rights Reserved.

#include "MCPTool_SpawnActor.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeUtils.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Blueprint.h"
#include "Kismet/GameplayStatics.h"

FMCPToolResult FMCPTool_SpawnActor::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Validate editor context using base class
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	// Extract and validate class path using base class helper
	FString ClassPath;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("class"), ClassPath, ParamError))
	{
		return ParamError.GetValue();
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

	// Parse transform using shared utilities
	const TSharedPtr<FJsonObject>* LocationObj = nullptr;
	const TSharedPtr<FJsonObject>* RotationObj = nullptr;
	const TSharedPtr<FJsonObject>* ScaleObj = nullptr;

	Params->TryGetObjectField(TEXT("location"), LocationObj);
	Params->TryGetObjectField(TEXT("rotation"), RotationObj);
	Params->TryGetObjectField(TEXT("scale"), ScaleObj);

	FVector Location = UnrealClaudeJsonUtils::ExtractVector(LocationObj ? *LocationObj : nullptr);
	FRotator Rotation = UnrealClaudeJsonUtils::ExtractRotator(RotationObj ? *RotationObj : nullptr);
	FVector Scale = UnrealClaudeJsonUtils::ExtractScale(ScaleObj ? *ScaleObj : nullptr);

	// Get optional name using base class helper
	FString ActorName = ExtractOptionalString(Params, TEXT("name"));

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

	// Mark the level as dirty using base class helper
	MarkWorldDirty(World);

	// Build result using shared JSON utilities
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actorName"), SpawnedActor->GetName());
	ResultData->SetStringField(TEXT("actorClass"), ActorClass->GetName());
	ResultData->SetStringField(TEXT("actorLabel"), SpawnedActor->GetActorLabel());
	ResultData->SetObjectField(TEXT("location"), UnrealClaudeJsonUtils::VectorToJson(Location));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Spawned actor '%s' of class '%s'"), *SpawnedActor->GetName(), *ActorClass->GetName()),
		ResultData
	);
}
