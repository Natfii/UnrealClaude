// Copyright Natali Caggiano. All Rights Reserved.

#include "ProjectContext.h"
#include "UnrealClaudeConstants.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Misc/App.h"

FProjectContextManager& FProjectContextManager::Get()
{
	static FProjectContextManager Instance;
	return Instance;
}

FProjectContextManager::FProjectContextManager()
	: bHasContext(false)
{
}

const FProjectContext& FProjectContextManager::GetContext(bool bForceRefresh)
{
	FScopeLock Lock(&ContextLock);

	if (!bHasContext || bForceRefresh)
	{
		RefreshContext();
	}

	return CachedContext;
}

void FProjectContextManager::RefreshContext()
{
	FScopeLock Lock(&ContextLock);

	UE_LOG(LogUnrealClaude, Log, TEXT("Refreshing project context..."));

	// Basic project info
	CachedContext.ProjectName = FApp::GetProjectName();
	CachedContext.ProjectPath = FPaths::ProjectDir();
	CachedContext.SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));
	CachedContext.EngineVersion = FEngineVersion::Current().ToString();
	CachedContext.GatheredAt = FDateTime::Now();

	// Clear previous data
	CachedContext.SourceFiles.Empty();
	CachedContext.UClasses.Empty();
	CachedContext.LevelActors.Empty();

	// Gather all context
	ScanSourceFiles();
	ParseUClasses();
	GatherLevelActors();
	CountAssets();

	bHasContext = true;

	UE_LOG(LogUnrealClaude, Log, TEXT("Project context gathered:"));
	UE_LOG(LogUnrealClaude, Log, TEXT("  - Source files: %d"), CachedContext.SourceFiles.Num());
	UE_LOG(LogUnrealClaude, Log, TEXT("  - UCLASS types: %d"), CachedContext.UClasses.Num());
	UE_LOG(LogUnrealClaude, Log, TEXT("  - Level actors: %d"), CachedContext.LevelActors.Num());
	UE_LOG(LogUnrealClaude, Log, TEXT("  - Total assets: %d"), CachedContext.AssetCount);
}

void FProjectContextManager::ScanSourceFiles()
{
	FString SourceDir = CachedContext.SourcePath;

	if (!FPaths::DirectoryExists(SourceDir))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Source directory not found: %s"), *SourceDir);
		return;
	}

	// Find all .h and .cpp files
	TArray<FString> FoundFiles;

	IFileManager::Get().FindFilesRecursive(
		FoundFiles,
		*SourceDir,
		TEXT("*.h"),
		true,  // Files
		false, // Directories
		false  // Clear array
	);

	IFileManager::Get().FindFilesRecursive(
		FoundFiles,
		*SourceDir,
		TEXT("*.cpp"),
		true,
		false,
		false
	);

	// Convert to relative paths
	for (const FString& FilePath : FoundFiles)
	{
		FString RelativePath = FilePath;
		FPaths::MakePathRelativeTo(RelativePath, *CachedContext.ProjectPath);
		CachedContext.SourceFiles.Add(RelativePath);
	}
}

int32 FProjectContextManager::SkipWhitespace(const FString& Content, int32 StartPos)
{
	int32 Pos = StartPos;
	while (Pos < Content.Len() && FChar::IsWhitespace(Content[Pos]))
	{
		Pos++;
	}
	return Pos;
}

FString FProjectContextManager::ParseIdentifier(const FString& Content, int32 StartPos, int32& OutEndPos)
{
	int32 EndPos = StartPos;
	while (EndPos < Content.Len() && (FChar::IsAlnum(Content[EndPos]) || Content[EndPos] == '_'))
	{
		EndPos++;
	}
	OutEndPos = EndPos;
	return Content.Mid(StartPos, EndPos - StartPos);
}

bool FProjectContextManager::ParseSingleUClass(const FString& FileContent, const FString& RelativePath, int32 UClassPos, int32& OutNextSearchPos)
{
	// Find the class keyword after UCLASS(...)
	int32 ClassPos = FileContent.Find(TEXT("class "), ESearchCase::CaseSensitive, ESearchDir::FromStart, UClassPos);
	if (ClassPos == INDEX_NONE || ClassPos > UClassPos + UnrealClaudeConstants::Context::MaxUClassToClassKeywordDistance)
	{
		OutNextSearchPos = UClassPos + 6;
		return false;
	}

	// Parse first identifier after "class " (might be API macro)
	int32 NameStart = ClassPos + 6;
	int32 NameEnd;
	FString FirstIdent = ParseIdentifier(FileContent, NameStart, NameEnd);

	// Skip whitespace and get next identifier
	NameStart = SkipWhitespace(FileContent, NameEnd);
	int32 SecondEnd;
	FString SecondIdent = ParseIdentifier(FileContent, NameStart, SecondEnd);

	// Determine actual class name (handle API macro suffix)
	FString ClassName;
	int32 ClassNameEnd;
	if (FirstIdent.EndsWith(TEXT("_API")) && !SecondIdent.IsEmpty())
	{
		ClassName = SecondIdent;
		ClassNameEnd = SecondEnd;
	}
	else
	{
		ClassName = SecondIdent.IsEmpty() ? FirstIdent : SecondIdent;
		ClassNameEnd = SecondIdent.IsEmpty() ? NameEnd : SecondEnd;
	}

	// Find parent class (after ": public ")
	FString ParentClass;
	int32 InheritPos = FileContent.Find(TEXT(": public "), ESearchCase::IgnoreCase, ESearchDir::FromStart, ClassNameEnd);
	if (InheritPos != INDEX_NONE && InheritPos < ClassNameEnd + UnrealClaudeConstants::Context::MaxClassNameToInheritanceDistance)
	{
		int32 ParentEnd;
		ParentClass = ParseIdentifier(FileContent, InheritPos + 9, ParentEnd);
	}

	OutNextSearchPos = ClassPos + 6;

	// Validate and store result
	if (ClassName.IsEmpty() || ClassName.Len() <= 1)
	{
		return false;
	}

	FUClassInfo ClassInfo;
	ClassInfo.ClassName = ClassName;
	ClassInfo.ParentClass = ParentClass;
	ClassInfo.FilePath = RelativePath;
	ClassInfo.bIsBlueprint = false;
	CachedContext.UClasses.Add(ClassInfo);
	CachedContext.CppClassCount++;

	return true;
}

void FProjectContextManager::ParseUClasses()
{
	// Scan header files for UCLASS declarations
	for (const FString& RelativePath : CachedContext.SourceFiles)
	{
		if (!RelativePath.EndsWith(TEXT(".h")))
		{
			continue;
		}

		FString FullPath = FPaths::Combine(CachedContext.ProjectPath, RelativePath);
		FString FileContent;
		if (!FFileHelper::LoadFileToString(FileContent, *FullPath))
		{
			continue;
		}

		// Find all UCLASS declarations in this file
		int32 SearchStart = 0;
		while (true)
		{
			int32 UClassPos = FileContent.Find(TEXT("UCLASS"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchStart);
			if (UClassPos == INDEX_NONE)
			{
				break;
			}

			ParseSingleUClass(FileContent, RelativePath, UClassPos, SearchStart);
		}
	}
}

void FProjectContextManager::GatherLevelActors()
{
	if (!GEditor)
	{
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return;
	}

	CachedContext.CurrentLevelName = World->GetMapName();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Skip hidden/transient actors
		if (Actor->HasAnyFlags(RF_Transient))
		{
			continue;
		}

		FLevelActorInfo ActorInfo;
		ActorInfo.Name = Actor->GetName();
		ActorInfo.Label = Actor->GetActorLabel();
		ActorInfo.ClassName = Actor->GetClass()->GetName();
		ActorInfo.Location = Actor->GetActorLocation();

		CachedContext.LevelActors.Add(ActorInfo);
	}
}

void FProjectContextManager::CountAssets()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Get all assets in the game content directory
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(TEXT("/Game")), AssetDataList, true);

	CachedContext.AssetCount = AssetDataList.Num();

	// Count blueprints
	CachedContext.BlueprintCount = 0;
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetClassPath.GetAssetName() == FName(TEXT("Blueprint")) ||
			AssetData.AssetClassPath.GetAssetName() == FName(TEXT("WidgetBlueprint")))
		{
			CachedContext.BlueprintCount++;
		}
	}
}

FString FProjectContextManager::FormatContextForPrompt() const
{
	FScopeLock Lock(&ContextLock);

	if (!bHasContext)
	{
		return TEXT("");
	}

	FString Context;

	Context += TEXT("\n\n=== PROJECT CONTEXT ===\n\n");

	// Project info
	Context += FString::Printf(TEXT("Project: %s\n"), *CachedContext.ProjectName);
	Context += FString::Printf(TEXT("Engine: %s\n"), *CachedContext.EngineVersion);
	Context += FString::Printf(TEXT("Level: %s\n\n"), *CachedContext.CurrentLevelName);

	// Statistics
	Context += FString::Printf(TEXT("Source Files: %d\n"), CachedContext.SourceFiles.Num());
	Context += FString::Printf(TEXT("C++ Classes: %d\n"), CachedContext.CppClassCount);
	Context += FString::Printf(TEXT("Blueprints: %d\n"), CachedContext.BlueprintCount);
	Context += FString::Printf(TEXT("Total Assets: %d\n"), CachedContext.AssetCount);
	Context += FString::Printf(TEXT("Level Actors: %d\n\n"), CachedContext.LevelActors.Num());

	// List UCLASS types (limit to avoid prompt bloat)
	if (CachedContext.UClasses.Num() > 0)
	{
		Context += TEXT("Project C++ Classes:\n");
		int32 MaxClasses = FMath::Min(CachedContext.UClasses.Num(), UnrealClaudeConstants::Context::MaxClassesToFormat);
		for (int32 i = 0; i < MaxClasses; ++i)
		{
			const FUClassInfo& ClassInfo = CachedContext.UClasses[i];
			if (!ClassInfo.ParentClass.IsEmpty())
			{
				Context += FString::Printf(TEXT("  - %s : %s\n"), *ClassInfo.ClassName, *ClassInfo.ParentClass);
			}
			else
			{
				Context += FString::Printf(TEXT("  - %s\n"), *ClassInfo.ClassName);
			}
		}
		if (CachedContext.UClasses.Num() > MaxClasses)
		{
			Context += FString::Printf(TEXT("  ... and %d more\n"), CachedContext.UClasses.Num() - MaxClasses);
		}
		Context += TEXT("\n");
	}

	// List source files (abbreviated)
	if (CachedContext.SourceFiles.Num() > 0)
	{
		Context += TEXT("Source Structure:\n");

		// Group by directory
		TMap<FString, TArray<FString>> FilesByDir;
		for (const FString& FilePath : CachedContext.SourceFiles)
		{
			FString Dir = FPaths::GetPath(FilePath);
			FilesByDir.FindOrAdd(Dir).Add(FPaths::GetCleanFilename(FilePath));
		}

		int32 DirCount = 0;
		for (const auto& Pair : FilesByDir)
		{
			if (DirCount++ >= UnrealClaudeConstants::Context::MaxDirectoriesToShow)
			{
				Context += FString::Printf(TEXT("  ... and %d more directories\n"), FilesByDir.Num() - UnrealClaudeConstants::Context::MaxDirectoriesToShow);
				break;
			}
			Context += FString::Printf(TEXT("  %s/ (%d files)\n"), *Pair.Key, Pair.Value.Num());
		}
		Context += TEXT("\n");
	}

	// List level actors (grouped by class, limited)
	if (CachedContext.LevelActors.Num() > 0)
	{
		Context += TEXT("Level Actors (by type):\n");

		TMap<FString, int32> ActorsByClass;
		for (const FLevelActorInfo& ActorInfo : CachedContext.LevelActors)
		{
			ActorsByClass.FindOrAdd(ActorInfo.ClassName)++;
		}

		// Sort by count (descending)
		TArray<TPair<FString, int32>> SortedActors;
		for (const auto& Pair : ActorsByClass)
		{
			SortedActors.Add(Pair);
		}
		SortedActors.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) {
			return A.Value > B.Value;
		});

		int32 Shown = 0;
		for (const auto& Pair : SortedActors)
		{
			if (Shown++ >= 15) // Limit types shown
			{
				Context += FString::Printf(TEXT("  ... and %d more types\n"), SortedActors.Num() - 15);
				break;
			}
			Context += FString::Printf(TEXT("  - %s: %d\n"), *Pair.Key, Pair.Value);
		}
		Context += TEXT("\n");
	}

	Context += TEXT("=== END PROJECT CONTEXT ===\n");

	return Context;
}

FString FProjectContextManager::GetContextSummary() const
{
	FScopeLock Lock(&ContextLock);

	if (!bHasContext)
	{
		return TEXT("No context gathered yet");
	}

	return FString::Printf(
		TEXT("%s | %d files | %d classes | %d actors | %d assets"),
		*CachedContext.ProjectName,
		CachedContext.SourceFiles.Num(),
		CachedContext.UClasses.Num(),
		CachedContext.LevelActors.Num(),
		CachedContext.AssetCount
	);
}

FTimespan FProjectContextManager::GetTimeSinceRefresh() const
{
	FScopeLock Lock(&ContextLock);

	if (!bHasContext)
	{
		return FTimespan::MaxValue();
	}

	return FDateTime::Now() - CachedContext.GatheredAt;
}
