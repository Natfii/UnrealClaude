// Copyright Your Name. All Rights Reserved.

#include "MCPTool_GetLevelActors.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

FMCPToolResult FMCPTool_GetLevelActors::Execute(const TSharedRef<FJsonObject>& Params)
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

	// Parse parameters
	FString ClassFilter;
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

	FString NameFilter;
	Params->TryGetStringField(TEXT("name_filter"), NameFilter);

	// Validate filter strings (basic length check to prevent abuse)
	FString ValidationError;
	if (!ClassFilter.IsEmpty() && !FMCPParamValidator::ValidateStringLength(ClassFilter, TEXT("class_filter"), 256, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}
	if (!NameFilter.IsEmpty() && !FMCPParamValidator::ValidateStringLength(NameFilter, TEXT("name_filter"), 256, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	bool bIncludeHidden = false;
	Params->TryGetBoolField(TEXT("include_hidden"), bIncludeHidden);

	int32 Limit = 100;
	Params->TryGetNumberField(TEXT("limit"), Limit);
	if (Limit <= 0) Limit = INT32_MAX;

	// Collect actors
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	int32 Count = 0;
	int32 TotalMatching = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Skip hidden actors if not requested
		if (!bIncludeHidden && Actor->IsHidden())
		{
			continue;
		}

		// Apply class filter
		if (!ClassFilter.IsEmpty())
		{
			FString ActorClassName = Actor->GetClass()->GetName();
			if (!ActorClassName.Contains(ClassFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		// Apply name filter
		if (!NameFilter.IsEmpty())
		{
			FString ActorName = Actor->GetName();
			FString ActorLabel = Actor->GetActorLabel();
			if (!ActorName.Contains(NameFilter, ESearchCase::IgnoreCase) &&
				!ActorLabel.Contains(NameFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		TotalMatching++;

		// Apply limit
		if (Count >= Limit)
		{
			continue; // Keep counting but don't add more
		}

		// Build actor info
		TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
		ActorJson->SetStringField(TEXT("name"), Actor->GetName());
		ActorJson->SetStringField(TEXT("label"), Actor->GetActorLabel());
		ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		ActorJson->SetBoolField(TEXT("hidden"), Actor->IsHidden());

		// Add location
		FVector Location = Actor->GetActorLocation();
		TSharedPtr<FJsonObject> LocationJson = MakeShared<FJsonObject>();
		LocationJson->SetNumberField(TEXT("x"), Location.X);
		LocationJson->SetNumberField(TEXT("y"), Location.Y);
		LocationJson->SetNumberField(TEXT("z"), Location.Z);
		ActorJson->SetObjectField(TEXT("location"), LocationJson);

		// Add rotation
		FRotator Rotation = Actor->GetActorRotation();
		TSharedPtr<FJsonObject> RotationJson = MakeShared<FJsonObject>();
		RotationJson->SetNumberField(TEXT("pitch"), Rotation.Pitch);
		RotationJson->SetNumberField(TEXT("yaw"), Rotation.Yaw);
		RotationJson->SetNumberField(TEXT("roll"), Rotation.Roll);
		ActorJson->SetObjectField(TEXT("rotation"), RotationJson);

		// Add scale
		FVector Scale = Actor->GetActorScale3D();
		TSharedPtr<FJsonObject> ScaleJson = MakeShared<FJsonObject>();
		ScaleJson->SetNumberField(TEXT("x"), Scale.X);
		ScaleJson->SetNumberField(TEXT("y"), Scale.Y);
		ScaleJson->SetNumberField(TEXT("z"), Scale.Z);
		ActorJson->SetObjectField(TEXT("scale"), ScaleJson);

		// Add tags if any
		if (Actor->Tags.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> TagsArray;
			for (const FName& Tag : Actor->Tags)
			{
				TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
			ActorJson->SetArrayField(TEXT("tags"), TagsArray);
		}

		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		Count++;
	}

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("actors"), ActorsArray);
	ResultData->SetNumberField(TEXT("count"), Count);
	ResultData->SetNumberField(TEXT("totalMatching"), TotalMatching);
	ResultData->SetStringField(TEXT("levelName"), World->GetMapName());

	FString Message = FString::Printf(TEXT("Found %d actors"), Count);
	if (TotalMatching > Count)
	{
		Message += FString::Printf(TEXT(" (showing %d of %d matching)"), Count, TotalMatching);
	}

	return FMCPToolResult::Success(Message, ResultData);
}
