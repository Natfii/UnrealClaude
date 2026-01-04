// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Information about a UCLASS found in the project
 */
struct FUClassInfo
{
	/** Class name */
	FString ClassName;

	/** Parent class name */
	FString ParentClass;

	/** File path where the class is defined */
	FString FilePath;

	/** Whether it's a Blueprint class */
	bool bIsBlueprint;

	FUClassInfo()
		: bIsBlueprint(false)
	{}
};

/**
 * Information about an actor in the current level
 */
struct FLevelActorInfo
{
	/** Actor name */
	FString Name;

	/** Actor label (display name) */
	FString Label;

	/** Class name */
	FString ClassName;

	/** Location */
	FVector Location;
};

/**
 * Structured project context information
 */
struct UNREALCLAUDE_API FProjectContext
{
	/** Project name */
	FString ProjectName;

	/** Project directory path */
	FString ProjectPath;

	/** Source directory path */
	FString SourcePath;

	/** Engine version */
	FString EngineVersion;

	/** List of source files in the project */
	TArray<FString> SourceFiles;

	/** UCLASS definitions found in headers */
	TArray<FUClassInfo> UClasses;

	/** Actors in the current level */
	TArray<FLevelActorInfo> LevelActors;

	/** Current level name */
	FString CurrentLevelName;

	/** Total asset count */
	int32 AssetCount;

	/** Blueprint count */
	int32 BlueprintCount;

	/** C++ class count */
	int32 CppClassCount;

	/** Timestamp when context was gathered */
	FDateTime GatheredAt;

	FProjectContext()
		: AssetCount(0)
		, BlueprintCount(0)
		, CppClassCount(0)
	{}
};

/**
 * Manager for gathering and caching project context
 */
class UNREALCLAUDE_API FProjectContextManager
{
public:
	static FProjectContextManager& Get();

	/** Gather project context (refresh if needed) */
	const FProjectContext& GetContext(bool bForceRefresh = false);

	/** Force a context refresh */
	void RefreshContext();

	/** Format context for inclusion in system prompt */
	FString FormatContextForPrompt() const;

	/** Get a summary of the context */
	FString GetContextSummary() const;

	/** Check if context has been gathered */
	bool HasContext() const { return bHasContext; }

	/** Get time since last refresh */
	FTimespan GetTimeSinceRefresh() const;

private:
	FProjectContextManager();

	/** Scan source files in the project */
	void ScanSourceFiles();

	/** Parse UCLASS declarations from headers */
	void ParseUClasses();

	/** Gather current level actors */
	void GatherLevelActors();

	/** Count assets in the project */
	void CountAssets();

	/** Cached project context */
	FProjectContext CachedContext;

	/** Whether context has been gathered */
	bool bHasContext;

	/** Critical section for thread safety */
	mutable FCriticalSection ContextLock;
};
