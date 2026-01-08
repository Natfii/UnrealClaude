# Unreal Engine 5.7 Asset Loading Context

This context is automatically loaded when working with asset loading and streaming.

## Reference Types

### Hard References (TObjectPtr)
Loads asset when the referencing object loads. Use for assets you always need.

```cpp
UPROPERTY(EditAnywhere)
TObjectPtr<UStaticMesh> AlwaysLoadedMesh;  // Loads with owner

UPROPERTY(EditAnywhere)
TObjectPtr<UTexture2D> AlwaysLoadedTexture;
```

### Soft References (TSoftObjectPtr)
Stores path string, not the asset. Load on demand.

```cpp
UPROPERTY(EditAnywhere)
TSoftObjectPtr<UStaticMesh> LazyMesh;  // Doesn't load until requested

UPROPERTY(EditAnywhere)
TSoftClassPtr<AActor> LazyActorClass;  // For class references
```

## Synchronous Loading

### LoadObject (Editor/Blocking)
```cpp
// Load asset synchronously - blocks until loaded
UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/MyMesh"));

// Load from soft reference
if (!SoftMesh.IsNull())
{
    UStaticMesh* Mesh = SoftMesh.LoadSynchronous();
}
```

**Warning**: Avoid synchronous loading during gameplay - causes hitches.

## Asynchronous Loading

### Using FStreamableManager
The primary async loading system.

```cpp
// Get the streamable manager
FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();

// Single asset async load
FSoftObjectPath AssetPath = SoftMesh.ToSoftObjectPath();
StreamableManager.RequestAsyncLoad(
    AssetPath,
    FStreamableDelegate::CreateUObject(this, &UMyClass::OnMeshLoaded)
);

// Callback
void UMyClass::OnMeshLoaded()
{
    // Asset is now loaded - get it from soft pointer
    UStaticMesh* Mesh = SoftMesh.Get();
    if (Mesh)
    {
        // Use the mesh
    }
}
```

### Batch Loading
```cpp
// Load multiple assets at once
TArray<FSoftObjectPath> AssetsToLoad;
AssetsToLoad.Add(SoftMesh1.ToSoftObjectPath());
AssetsToLoad.Add(SoftMesh2.ToSoftObjectPath());
AssetsToLoad.Add(SoftTexture.ToSoftObjectPath());

StreamableManager.RequestAsyncLoad(
    AssetsToLoad,
    FStreamableDelegate::CreateUObject(this, &UMyClass::OnAllAssetsLoaded)
);
```

### Handle Management
```cpp
// Store handle to control lifetime
TSharedPtr<FStreamableHandle> LoadHandle;

LoadHandle = StreamableManager.RequestAsyncLoad(
    AssetPath,
    FStreamableDelegate::CreateUObject(this, &UMyClass::OnLoaded),
    FStreamableManager::AsyncLoadHighPriority,
    true  // bManageActiveHandle - keeps assets in memory
);

// Cancel if needed
if (LoadHandle.IsValid())
{
    LoadHandle->CancelHandle();
}

// Release assets when done
LoadHandle.Reset();
```

### Load Priority
```cpp
FStreamableManager::AsyncLoadHighPriority    // Load first
FStreamableManager::DefaultAsyncLoadPriority // Normal priority
FStreamableManager::AsyncLoadLowPriority     // Load last
```

## Asset Manager

### Primary Assets
Assets registered with Asset Manager for advanced loading.

```cpp
// In your game's Asset Manager
void UMyAssetManager::StartInitialLoading()
{
    Super::StartInitialLoading();

    // Load all items of this type
    TArray<FPrimaryAssetId> ItemIds;
    GetPrimaryAssetIdList(FPrimaryAssetType("Item"), ItemIds);

    LoadPrimaryAssets(
        ItemIds,
        TArray<FName>(),  // Bundles to load
        FStreamableDelegate::CreateUObject(this, &UMyAssetManager::OnItemsLoaded)
    );
}
```

### Asset Bundles
Group assets for loading together.

```cpp
// In data asset
UPROPERTY(EditAnywhere, meta = (AssetBundles = "UI"))
TSoftObjectPtr<UTexture2D> UITexture;

UPROPERTY(EditAnywhere, meta = (AssetBundles = "Gameplay"))
TSoftObjectPtr<UStaticMesh> GameplayMesh;

// Load specific bundle
AssetManager->LoadPrimaryAsset(
    AssetId,
    TArray<FName>{ FName("UI") },  // Only load UI bundle
    Delegate
);
```

## Checking Load State

```cpp
// Check if soft pointer is valid (has path)
if (!SoftMesh.IsNull())
{
    // Has a path set
}

// Check if asset is currently loaded
if (SoftMesh.IsValid())
{
    // Asset is in memory
    UStaticMesh* Mesh = SoftMesh.Get();
}

// Check if pending (not yet loaded)
if (SoftMesh.IsPending())
{
    // Has path but not loaded
}
```

## Asset Registry

### Finding Assets
```cpp
FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

// Find assets by class
TArray<FAssetData> Assets;
AssetRegistry.Get().GetAssetsByClass(UStaticMesh::StaticClass()->GetClassPathName(), Assets);

// Find assets by path
AssetRegistry.Get().GetAssetsByPath(FName("/Game/Meshes"), Assets);

// Find assets by tag
FARFilter Filter;
Filter.TagsAndValues.Add(FName("Tag"), FString("Value"));
AssetRegistry.Get().GetAssets(Filter, Assets);
```

### Asset Data
```cpp
for (const FAssetData& Asset : Assets)
{
    // Asset info without loading
    FString AssetName = Asset.AssetName.ToString();
    FString AssetPath = Asset.GetSoftObjectPath().ToString();
    UClass* AssetClass = Asset.GetClass();

    // Load only if needed
    UObject* LoadedAsset = Asset.GetAsset();
}
```

## Memory Management

### Garbage Collection
```cpp
// Force garbage collection
GEngine->ForceGarbageCollection(true);

// Check if object is unreferenced
if (Object->IsUnreachable())
{
    // Will be collected
}
```

### Keeping Assets Loaded
```cpp
// Add to root to prevent GC (use carefully!)
Asset->AddToRoot();

// Remove from root when done
Asset->RemoveFromRoot();

// Better: Use a UPROPERTY reference
UPROPERTY()
TObjectPtr<UStaticMesh> KeepLoadedMesh;
```

## MCP Asset Operations

Available via `asset_search`, `asset_dependencies`, `asset_referencers`:

| Tool | Description |
|------|-------------|
| `asset_search` | Find assets by name/type/path |
| `asset_dependencies` | Get what an asset depends on |
| `asset_referencers` | Get what references an asset |

## Best Practices

1. **Use soft references** for optional/large assets
2. **Async load during loading screens** or before assets are needed
3. **Batch load related assets** together for efficiency
4. **Store handles** if you need assets to stay loaded
5. **Use Asset Manager** for primary game assets
6. **Avoid LoadObject in gameplay** - use async loading instead
7. **Check IsPending()** before assuming asset is available
