// Copyright Your Name. All Rights Reserved.

#include "MCPTool_DeleteActors.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

FMCPToolResult FMCPTool_DeleteActors::Execute(const TSharedRef<FJsonObject>& Params)
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

	// Collect actors to delete
	TArray<AActor*> ActorsToDelete;
	TArray<FString> DeletedNames;
	TArray<FString> NotFoundNames;
	FString ValidationError;

	// Check for single actor_name
	FString SingleActorName;
	if (Params->TryGetStringField(TEXT("actor_name"), SingleActorName))
	{
		// Validate actor name
		if (!FMCPParamValidator::ValidateActorName(SingleActorName, ValidationError))
		{
			return FMCPToolResult::Error(ValidationError);
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && (Actor->GetName() == SingleActorName || Actor->GetActorLabel() == SingleActorName))
			{
				ActorsToDelete.Add(Actor);
				DeletedNames.Add(Actor->GetName());
				break;
			}
		}
		if (ActorsToDelete.Num() == 0)
		{
			NotFoundNames.Add(SingleActorName);
		}
	}

	// Check for actor_names array
	const TArray<TSharedPtr<FJsonValue>>* ActorNamesArray;
	if (Params->TryGetArrayField(TEXT("actor_names"), ActorNamesArray))
	{
		for (const TSharedPtr<FJsonValue>& NameValue : *ActorNamesArray)
		{
			FString ActorName;
			if (NameValue->TryGetString(ActorName))
			{
				// Validate each actor name
				if (!FMCPParamValidator::ValidateActorName(ActorName, ValidationError))
				{
					return FMCPToolResult::Error(ValidationError);
				}

				bool bFound = false;
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					AActor* Actor = *It;
					if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
					{
						ActorsToDelete.AddUnique(Actor);
						DeletedNames.AddUnique(Actor->GetName());
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					NotFoundNames.Add(ActorName);
				}
			}
		}
	}

	// Check for class filter
	FString ClassFilter;
	if (Params->TryGetStringField(TEXT("class_filter"), ClassFilter) && !ClassFilter.IsEmpty())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor)
			{
				FString ActorClassName = Actor->GetClass()->GetName();
				if (ActorClassName.Contains(ClassFilter, ESearchCase::IgnoreCase))
				{
					ActorsToDelete.AddUnique(Actor);
					DeletedNames.AddUnique(Actor->GetName());
				}
			}
		}
	}

	// Validate we have something to delete
	if (ActorsToDelete.Num() == 0)
	{
		if (NotFoundNames.Num() > 0)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("No actors found: %s"), *FString::Join(NotFoundNames, TEXT(", "))));
		}
		return FMCPToolResult::Error(TEXT("No actors specified or found to delete. Provide actor_name, actor_names array, or class_filter."));
	}

	// Delete actors
	for (AActor* Actor : ActorsToDelete)
	{
		if (Actor && IsValid(Actor))
		{
			World->EditorDestroyActor(Actor, false);
		}
	}

	// Mark dirty
	World->MarkPackageDirty();

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> DeletedArray;
	for (const FString& Name : DeletedNames)
	{
		DeletedArray.Add(MakeShared<FJsonValueString>(Name));
	}
	ResultData->SetArrayField(TEXT("deleted"), DeletedArray);
	ResultData->SetNumberField(TEXT("count"), DeletedNames.Num());

	if (NotFoundNames.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArray;
		for (const FString& Name : NotFoundNames)
		{
			NotFoundArray.Add(MakeShared<FJsonValueString>(Name));
		}
		ResultData->SetArrayField(TEXT("notFound"), NotFoundArray);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Deleted %d actor(s)"), DeletedNames.Num()),
		ResultData
	);
}
