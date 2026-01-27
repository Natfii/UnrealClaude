# Unreal Engine 5.7 Actor & Component Context

This context is automatically loaded when working with Actor and Component manipulation.

## Spawning Actors

### Basic SpawnActor
```cpp
// Simple spawn
FActorSpawnParameters SpawnParams;
SpawnParams.Owner = this;
SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

AActor* Actor = GetWorld()->SpawnActor<AMyActor>(
    AMyActor::StaticClass(),
    SpawnLocation,
    SpawnRotation,
    SpawnParams
);
```

### SpawnActorDeferred - Configure Before Construction
Use when you need to set properties BEFORE the construction script runs.

```cpp
// Spawn deferred - actor exists but BeginPlay hasn't run
AMyActor* Actor = GetWorld()->SpawnActorDeferred<AMyActor>(
    AMyActor::StaticClass(),
    SpawnTransform,
    nullptr,                  // Owner
    nullptr,                  // Instigator
    ESpawnActorCollisionHandlingMethod::AlwaysSpawn
);

if (Actor)
{
    // Configure properties BEFORE construction script
    Actor->MyProperty = SomeValue;
    Actor->InitializeFromData(Data);

    // Finalize spawning - runs construction script and BeginPlay
    Actor->FinishSpawning(SpawnTransform);
}
```

### Spawn Collision Handling
```cpp
enum ESpawnActorCollisionHandlingMethod
{
    Undefined,              // Use class default
    AlwaysSpawn,            // Spawn even if colliding
    AdjustIfPossibleButAlwaysSpawn,  // Try to adjust, spawn anyway
    AdjustIfPossibleButDontSpawnIfColliding,  // Adjust or fail
    DontSpawnIfColliding    // Fail if any collision
};
```

## Actor Lifecycle

1. **PostInitializeComponents** - Components initialized
2. **BeginPlay** - Gameplay begins
3. **Tick** - Called every frame (if enabled)
4. **EndPlay** - Gameplay ends (with reason)
5. **Destroyed** - Actor destroyed

### EndPlay Reasons
```cpp
enum EEndPlayReason
{
    Destroyed,          // Actor->Destroy() called
    LevelTransition,    // Level is being unloaded
    EndPlayInEditor,    // PIE ended
    RemovedFromWorld,   // Removed from world
    Quit                // Application quitting
};
```

## Iterating Actors

### TActorIterator - Iterate by Type
```cpp
// Iterate all actors of a type
for (TActorIterator<AMyActor> It(GetWorld()); It; ++It)
{
    AMyActor* Actor = *It;
    if (IsValid(Actor))
    {
        Actor->DoSomething();
    }
}

// All actors in world
for (TActorIterator<AActor> It(GetWorld()); It; ++It)
{
    AActor* Actor = *It;
}
```

### GetAllActorsOfClass
```cpp
TArray<AActor*> FoundActors;
UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMyActor::StaticClass(), FoundActors);

for (AActor* Actor : FoundActors)
{
    // Process actor
}
```

### GetAllActorsWithTag
```cpp
TArray<AActor*> TaggedActors;
UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("MyTag"), TaggedActors);
```

## Actor Properties and Functions

### Transform
```cpp
// Get transform
FVector Location = Actor->GetActorLocation();
FRotator Rotation = Actor->GetActorRotation();
FVector Scale = Actor->GetActorScale3D();
FTransform Transform = Actor->GetActorTransform();

// Set transform
Actor->SetActorLocation(NewLocation);
Actor->SetActorRotation(NewRotation);
Actor->SetActorScale3D(NewScale);
Actor->SetActorTransform(NewTransform);

// Relative movement
Actor->AddActorWorldOffset(FVector(100, 0, 0));
Actor->AddActorWorldRotation(FRotator(0, 45, 0));
```

### Hierarchy
```cpp
// Attach to another actor
Actor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);

// Attach to component
Actor->AttachToComponent(ParentComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

// Detach
Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

// Get attached actors
TArray<AActor*> AttachedActors;
Actor->GetAttachedActors(AttachedActors);
```

### Tags
```cpp
// Add tag
Actor->Tags.Add(FName("Enemy"));

// Check tag
if (Actor->ActorHasTag(FName("Enemy")))
{
    // Is enemy
}
```

### Validity Checks
```cpp
// Check if actor is valid and not pending kill
if (IsValid(Actor))
{
    // Safe to use
}

// Alternative check
if (Actor && !Actor->IsPendingKillPending())
{
    // Safe to use
}
```

## Components

### Adding Components Dynamically
```cpp
// In constructor (using CreateDefaultSubobject)
MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
RootComponent = MeshComponent;

// At runtime (using NewObject)
UStaticMeshComponent* NewMesh = NewObject<UStaticMeshComponent>(this);
NewMesh->SetStaticMesh(SomeMesh);
NewMesh->SetupAttachment(RootComponent);
NewMesh->RegisterComponent();
```

### Finding Components
```cpp
// Get component by type
UStaticMeshComponent* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>();

// Get all components of type
TArray<UStaticMeshComponent*> Meshes;
Actor->GetComponents<UStaticMeshComponent>(Meshes);

// Get component by name
UActorComponent* Comp = Actor->GetDefaultSubobjectByName(TEXT("MyComponent"));
```

### Component Hierarchy
```cpp
// Set root component
Actor->SetRootComponent(NewRootComponent);

// Attach component to another
ChildComponent->SetupAttachment(ParentComponent);
ChildComponent->AttachToComponent(ParentComponent, FAttachmentTransformRules::KeepRelativeTransform);
```

## Destroying Actors

```cpp
// Destroy immediately
Actor->Destroy();

// Destroy with lifespan
Actor->SetLifeSpan(5.0f);  // Destroy after 5 seconds

// Check pending destruction
if (Actor->IsPendingKillPending())
{
    // Actor is being destroyed
}
```

## World Context

### Getting World
```cpp
// From actor
UWorld* World = GetWorld();

// From component
UWorld* World = Component->GetWorld();

// From object with world context
UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
```

### World Functions
```cpp
// Timer manager
GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &AMyActor::OnTimer, 1.0f, true);

// Line trace
FHitResult Hit;
GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility);

// Overlap test
TArray<FOverlapResult> Overlaps;
GetWorld()->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(100));
```

## MCP Actor Operations

Available via `spawn_actor`, `move_actor`, `delete_actors`, `get_level_actors`:

| Tool | Description |
|------|-------------|
| `spawn_actor` | Spawn actor by class path |
| `move_actor` | Set actor location/rotation/scale |
| `delete_actors` | Remove actors by name/pattern |
| `get_level_actors` | List actors with optional filtering |
| `set_property` | Modify actor properties |

## Level Management

### Loading Levels in Editor (UE 5.7)

The `open_level` MCP tool provides level management without console commands.

```cpp
#include "FileHelpers.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"

// Load an existing map by filename
// Engine handles save prompt internally before switching
FString Filename;
FPackageName::TryConvertLongPackageNameToFilename(
    TEXT("/Game/Maps/MyLevel"), Filename, FPackageName::GetMapPackageExtension());
UWorld* LoadedWorld = UEditorLoadingAndSavingUtils::LoadMap(Filename);

// Create a new blank map
// bSaveCurrent: if true, prompts user to save current level first
UWorld* NewWorld = UEditorLoadingAndSavingUtils::NewBlankMap(/*bSaveCurrent=*/true);

// Check if a package exists before attempting to load
bool bExists = FPackageName::DoesPackageExist(TEXT("/Game/Maps/MyLevel"));
```

### Map Templates (UE 5.7)

```cpp
// Access available map templates via GUnrealEd
if (GUnrealEd)
{
    const TArray<FTemplateMapInfo>& Templates = GUnrealEd->GetTemplateMapInfos();
    for (const FTemplateMapInfo& Template : Templates)
    {
        FString DisplayName = FPaths::GetBaseFilename(Template.Map.ToString());
        FString MapPath = Template.Map.ToString();
        // Template.ThumbnailTexture contains preview image path
    }
}

// Load a template map (same API as opening existing maps)
FString TemplateFilename;
FPackageName::TryConvertLongPackageNameToFilename(
    TemplatePackageName, TemplateFilename, FPackageName::GetMapPackageExtension());
UWorld* NewWorld = UEditorLoadingAndSavingUtils::LoadMap(TemplateFilename);
```

### World Info After Loading
```cpp
if (LoadedWorld)
{
    FString MapName = LoadedWorld->GetMapName();       // e.g., "MyLevel"
    FString WorldName = LoadedWorld->GetName();         // e.g., "MyLevel"
    FString PackageName = LoadedWorld->GetOutermost()->GetName(); // Full package path
}
```

### Level Path Validation
Level paths used with MCP tools must:
- Start with `/Game/` (project content only)
- Not contain path traversal (`..`)
- Not reference `/Engine/` or `/Script/` paths
- Not exceed 512 characters
- Not contain dangerous characters (`<>|&;$(){}[]!*?~`)

### MCP Level Operations

Available via `open_level`:

| Action | Parameters | Description |
|--------|-----------|-------------|
| `open` | `level_path` (required) | Load an existing map by asset path (e.g., `/Game/Maps/MyLevel`) |
| `new` | `template` (optional), `save_current` (optional) | Create a new blank map or from a named template |
| `save_as` | `save_path` (required) | Save the current level to a specified asset path (e.g., `/Game/Maps/MyLevel`) |
| `list_templates` | (none) | List all available map templates with names and paths |

## Best Practices

1. **Always use IsValid()** before accessing actors
2. **Use SpawnActorDeferred** when you need pre-construction setup
3. **RegisterComponent()** after creating components at runtime
4. **Clean up timers** in EndPlay to prevent crashes
5. **Use FAttachmentTransformRules** explicitly for clarity
6. **Use `open_level` tool** instead of console commands for level switching
7. **Use `list_templates`** before creating template-based maps to discover available options
8. **Check `asset_search`** for map assets before attempting to open a level by path
