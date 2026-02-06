// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_GetLevelActors.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeUtils.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

FMCPToolResult FMCPTool_GetLevelActors::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Validate editor context using base class
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
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

	bool bBrief = ExtractOptionalBool(Params, TEXT("brief"), true);

	int32 Limit = 25;
	Params->TryGetNumberField(TEXT("limit"), Limit);
	if (Limit <= 0) Limit = 25;
	if (Limit > 1000) Limit = 1000; // Cap at 1000 for performance

	int32 Offset = 0;
	Params->TryGetNumberField(TEXT("offset"), Offset);
	if (Offset < 0) Offset = 0;

	// Collect actors
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	int32 MatchIndex = 0;  // Index among matching actors
	int32 AddedCount = 0;  // Count of actors added to result
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

		// Apply offset - skip until we reach the offset
		if (MatchIndex < Offset)
		{
			MatchIndex++;
			continue;
		}

		// Apply limit - stop adding after limit reached
		if (AddedCount >= Limit)
		{
			MatchIndex++;
			continue; // Keep counting total but don't add more
		}

		// Build actor info using base class helper
		TSharedPtr<FJsonObject> ActorJson = bBrief
			? BuildActorInfoJson(Actor)
			: BuildActorInfoWithTransformJson(Actor);

		if (!bBrief)
		{
			ActorJson->SetBoolField(TEXT("hidden"), Actor->IsHidden());

			// Add tags if any
			if (Actor->Tags.Num() > 0)
			{
				TArray<FString> TagStrings;
				for (const FName& Tag : Actor->Tags)
				{
					TagStrings.Add(Tag.ToString());
				}
				ActorJson->SetArrayField(TEXT("tags"), StringArrayToJsonArray(TagStrings));
			}
		}

		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		AddedCount++;
		MatchIndex++;
	}

	// Build result with pagination metadata
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("actors"), ActorsArray);
	ResultData->SetNumberField(TEXT("count"), AddedCount);
	ResultData->SetNumberField(TEXT("total"), TotalMatching);
	ResultData->SetNumberField(TEXT("offset"), Offset);
	ResultData->SetNumberField(TEXT("limit"), Limit);
	ResultData->SetBoolField(TEXT("hasMore"), (Offset + AddedCount) < TotalMatching);
	if ((Offset + AddedCount) < TotalMatching)
	{
		ResultData->SetNumberField(TEXT("nextOffset"), Offset + AddedCount);
	}
	ResultData->SetStringField(TEXT("levelName"), World->GetMapName());

	FString Message = FString::Printf(TEXT("Found %d actors"), AddedCount);
	if (TotalMatching > AddedCount)
	{
		Message += FString::Printf(TEXT(" (showing %d-%d of %d total)"), Offset + 1, Offset + AddedCount, TotalMatching);
	}

	return FMCPToolResult::Success(Message, ResultData);
}
