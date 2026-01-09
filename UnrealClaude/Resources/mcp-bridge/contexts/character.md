# Unreal Engine 5.7 Character System Context

This context is automatically loaded when working with Character MCP tools.

## Core Classes

### ACharacter

Base class for characters with movement, collision, and skeletal mesh support.

```cpp
// ACharacter inherits from APawn
// Key components:
// - UCapsuleComponent (collision)
// - USkeletalMeshComponent (mesh + animation)
// - UCharacterMovementComponent (movement logic)

ACharacter* Character = GetWorld()->SpawnActor<ACharacter>(CharacterClass, SpawnTransform);

// Access components
UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
USkeletalMeshComponent* Mesh = Character->GetMesh();
UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
```

### UCharacterMovementComponent

Handles all character movement physics and state.

```cpp
// Walking/Running properties
Movement->MaxWalkSpeed = 600.0f;        // cm/s
Movement->MaxWalkSpeedCrouched = 300.0f;
Movement->MaxAcceleration = 2048.0f;    // cm/s^2
Movement->GroundFriction = 8.0f;

// Jumping properties
Movement->JumpZVelocity = 420.0f;       // Initial jump velocity
Movement->AirControl = 0.35f;           // 0-1, how much control in air
Movement->GravityScale = 1.0f;          // Multiplier for gravity

// Step/Slope handling
Movement->MaxStepHeight = 45.0f;        // Max height character can step up
Movement->SetWalkableFloorAngle(45.0f); // Max slope angle for walking

// Braking
Movement->BrakingDecelerationWalking = 2048.0f;
Movement->BrakingDecelerationFalling = 0.0f;
Movement->BrakingFriction = 0.0f;
Movement->bUseSeparateBrakingFriction = false;

// Swimming/Flying
Movement->MaxSwimSpeed = 300.0f;
Movement->MaxFlySpeed = 600.0f;
```

### Movement Modes

```cpp
// EMovementMode
MOVE_None,      // No movement
MOVE_Walking,   // On ground
MOVE_Falling,   // In air (jumping/falling)
MOVE_Swimming,  // In water volume
MOVE_Flying,    // Flying mode
MOVE_Custom     // Custom movement mode

// Check current mode
if (Movement->IsMovingOnGround()) { /* walking */ }
if (Movement->IsFalling()) { /* in air */ }
if (Movement->IsSwimming()) { /* swimming */ }

// Set movement mode
Movement->SetMovementMode(MOVE_Flying);
```

## MCP Tool: character

Query and modify ACharacter actors in the current level.

### Operations

| Operation | Description | Required Params |
|-----------|-------------|-----------------|
| `list_characters` | Find all characters | None (optional: class_filter, limit, offset) |
| `get_character_info` | Get character details | character_name |
| `get_movement_params` | Query movement properties | character_name |
| `set_movement_params` | Modify movement values | character_name + movement params |
| `get_components` | List character components | character_name |

### Example Usage

```json
// List all characters in level
{"operation": "list_characters"}

// Filter by class
{"operation": "list_characters", "class_filter": "BP_Player"}

// Get character info
{"operation": "get_character_info", "character_name": "BP_PlayerCharacter_0"}

// Get movement parameters
{"operation": "get_movement_params", "character_name": "MyCharacter"}

// Modify movement (any combination of params)
{
  "operation": "set_movement_params",
  "character_name": "MyCharacter",
  "max_walk_speed": 800,
  "jump_z_velocity": 600,
  "air_control": 0.5
}

// Get all components
{"operation": "get_components", "character_name": "MyCharacter"}

// Filter components by class
{"operation": "get_components", "character_name": "MyCharacter", "component_class": "Skeletal"}
```

### Movement Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| max_walk_speed | float | 0-10000 | Maximum walking speed (cm/s) |
| max_acceleration | float | 0-100000 | Maximum acceleration (cm/s^2) |
| ground_friction | float | 0-100 | Ground friction coefficient |
| jump_z_velocity | float | 0-10000 | Initial jump velocity (cm/s) |
| air_control | float | 0-1 | Air control factor |
| gravity_scale | float | -10 to 10 | Gravity multiplier |
| max_step_height | float | 0-500 | Maximum step height (cm) |
| walkable_floor_angle | float | 0-90 | Max walkable slope (degrees) |
| braking_deceleration_walking | float | 0-100000 | Braking decel when walking |
| braking_friction | float | 0-100 | Braking friction coefficient |

## MCP Tool: character_data

Create and manage character configuration DataAssets and stats DataTables.

### Data Structures

#### UCharacterConfigDataAsset

Blueprint-friendly configuration asset for character settings.

```cpp
UCLASS(BlueprintType)
class UCharacterConfigDataAsset : public UDataAsset
{
    // Identity
    FName ConfigId;
    FString DisplayName;
    FString Description;

    // Visuals (soft references for async loading)
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
    TSoftClassPtr<UAnimInstance> AnimBlueprintClass;

    // Movement
    float BaseWalkSpeed = 600.0f;
    float BaseRunSpeed = 1000.0f;
    float BaseJumpVelocity = 420.0f;
    float BaseAcceleration = 2048.0f;
    float BaseGroundFriction = 8.0f;
    float BaseAirControl = 0.35f;
    float BaseGravityScale = 1.0f;

    // Combat
    float BaseHealth = 100.0f;
    float BaseStamina = 100.0f;
    float BaseDamage = 10.0f;
    float BaseDefense = 0.0f;

    // Collision
    float CapsuleRadius = 42.0f;
    float CapsuleHalfHeight = 96.0f;

    // Progression
    TSoftObjectPtr<UDataTable> StatsTable;
    FName DefaultStatsRowName;

    // Tags
    TArray<FName> GameplayTags;
    bool bIsPlayerCharacter = false;
};
```

#### FCharacterStatsRow

DataTable row structure for level-based or variant stats.

```cpp
USTRUCT(BlueprintType)
struct FCharacterStatsRow : public FTableRowBase
{
    // Identity
    FName StatsId;
    FString DisplayName;

    // Vitals
    float BaseHealth = 100.0f;
    float MaxHealth = 100.0f;
    float BaseStamina = 100.0f;
    float MaxStamina = 100.0f;

    // Movement
    float WalkSpeed = 600.0f;
    float RunSpeed = 1000.0f;
    float JumpVelocity = 420.0f;

    // Combat/Progression
    float DamageMultiplier = 1.0f;
    float DefenseMultiplier = 1.0f;
    float XPMultiplier = 1.0f;
    int32 Level = 1;

    // Tags
    TArray<FName> Tags;
};
```

### Operations

| Operation | Description | Required Params |
|-----------|-------------|-----------------|
| `create_character_data` | Create config DataAsset | asset_name |
| `query_character_data` | Search configs | None (optional filters) |
| `get_character_data` | Get config details | asset_path |
| `update_character_data` | Modify config | asset_path |
| `create_stats_table` | Create stats DataTable | asset_name |
| `query_stats_table` | Get table rows | table_path |
| `add_stats_row` | Add row to table | table_path, row_name |
| `update_stats_row` | Modify row | table_path, row_name |
| `remove_stats_row` | Delete row | table_path, row_name |
| `apply_character_data` | Apply config to actor | asset_path, character_name |

### Example Workflow

```json
// 1. Create a character config DataAsset
{
  "operation": "create_character_data",
  "asset_name": "DA_PlayerConfig",
  "config_id": "player_default",
  "display_name": "Default Player",
  "base_walk_speed": 600,
  "base_jump_velocity": 500,
  "base_health": 100,
  "is_player_character": true,
  "gameplay_tags": ["Player", "Human"]
}

// 2. Create a stats DataTable for level progression
{
  "operation": "create_stats_table",
  "asset_name": "DT_PlayerStats"
}

// 3. Add stats rows for different levels
{
  "operation": "add_stats_row",
  "table_path": "/Game/Characters/DT_PlayerStats",
  "row_name": "Level1",
  "stats_id": "lvl1",
  "display_name": "Level 1",
  "max_health": 100,
  "walk_speed": 600,
  "damage_multiplier": 1.0,
  "level": 1
}

{
  "operation": "add_stats_row",
  "table_path": "/Game/Characters/DT_PlayerStats",
  "row_name": "Level10",
  "stats_id": "lvl10",
  "display_name": "Level 10",
  "max_health": 250,
  "walk_speed": 700,
  "damage_multiplier": 1.5,
  "level": 10
}

// 4. Query the stats table
{
  "operation": "query_stats_table",
  "table_path": "/Game/Characters/DT_PlayerStats"
}

// 5. Apply config to a runtime character
{
  "operation": "apply_character_data",
  "asset_path": "/Game/Characters/DA_PlayerConfig",
  "character_name": "BP_PlayerCharacter_0",
  "apply_movement": true,
  "apply_mesh": false
}
```

## Common Patterns

### Character Setup in C++

```cpp
void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Load config from DataAsset
    if (UCharacterConfigDataAsset* Config = ConfigAsset.LoadSynchronous())
    {
        // Apply movement settings
        if (UCharacterMovementComponent* CMC = GetCharacterMovement())
        {
            CMC->MaxWalkSpeed = Config->BaseWalkSpeed;
            CMC->JumpZVelocity = Config->BaseJumpVelocity;
            CMC->AirControl = Config->BaseAirControl;
        }

        // Apply capsule
        if (UCapsuleComponent* Capsule = GetCapsuleComponent())
        {
            Capsule->SetCapsuleRadius(Config->CapsuleRadius);
            Capsule->SetCapsuleHalfHeight(Config->CapsuleHalfHeight);
        }
    }
}
```

### Blueprint Integration

```cpp
// In Blueprint, use the GetStatsRow function:
UFUNCTION(BlueprintCallable)
FCharacterStatsRow UCharacterConfigDataAsset::GetStatsRow(FName RowName) const
{
    if (UDataTable* Table = StatsTable.LoadSynchronous())
    {
        if (FCharacterStatsRow* Row = Table->FindRow<FCharacterStatsRow>(RowName, TEXT("")))
        {
            return *Row;
        }
    }
    return FCharacterStatsRow();
}
```

### Iterating Characters

```cpp
// Find all characters in world
for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
{
    ACharacter* Character = *It;
    if (IsValid(Character))
    {
        // Process character
        FString Name = Character->GetName();
        FVector Location = Character->GetActorLocation();
    }
}
```

## Default Asset Paths

- Character configs: `/Game/Characters/`
- Stats tables: `/Game/Characters/`

Both tools use `package_path` parameter to customize output location.
