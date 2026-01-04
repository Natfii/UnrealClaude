// Copyright Your Name. All Rights Reserved.

#include "MCPToolBase.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

TOptional<FMCPToolResult> FMCPToolBase::ValidateEditorContext(UWorld*& OutWorld) const
{
	OutWorld = nullptr;

	if (!GEditor)
	{
		return FMCPToolResult::Error(TEXT("Editor not available"));
	}

	OutWorld = GEditor->GetEditorWorldContext().World();
	if (!OutWorld)
	{
		return FMCPToolResult::Error(TEXT("No active world"));
	}

	return TOptional<FMCPToolResult>(); // Success - no error
}

AActor* FMCPToolBase::FindActorByNameOrLabel(UWorld* World, const FString& NameOrLabel) const
{
	if (!World || NameOrLabel.IsEmpty())
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && (Actor->GetName() == NameOrLabel || Actor->GetActorLabel() == NameOrLabel))
		{
			return Actor;
		}
	}

	return nullptr;
}

void FMCPToolBase::MarkWorldDirty(UWorld* World) const
{
	if (World)
	{
		World->MarkPackageDirty();
	}
}

void FMCPToolBase::MarkActorDirty(AActor* Actor) const
{
	if (Actor)
	{
		Actor->MarkPackageDirty();
		if (UWorld* World = Actor->GetWorld())
		{
			World->MarkPackageDirty();
		}
	}
}
