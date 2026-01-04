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

	// Load the actor class using base class helper (handles fallback prefixes)
	UClass* ActorClass = LoadActorClass(ClassPath, ParamError);
	if (!ActorClass)
	{
		return ParamError.GetValue();
	}

	// Parse transform using base class helpers (consolidated transform extraction)
	FVector Location = ExtractVectorParam(Params, TEXT("location"));
	FRotator Rotation = ExtractRotatorParam(Params, TEXT("rotation"));
	FVector Scale = ExtractScaleParam(Params, TEXT("scale"));

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
