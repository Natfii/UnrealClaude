// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "CharacterDataTypes.generated.h"

/**
 * Data Table row structure for character stats
 * Used with UDataTable for configurable character attributes
 */
USTRUCT(BlueprintType)
struct UNREALCLAUDE_API FCharacterStatsRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Unique identifier for this stats configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName StatsId;

	/** Display name for this configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString DisplayName;

	/** Base health value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals", meta = (ClampMin = "0.0"))
	float BaseHealth = 100.0f;

	/** Maximum health value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals", meta = (ClampMin = "0.0"))
	float MaxHealth = 100.0f;

	/** Base stamina value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals", meta = (ClampMin = "0.0"))
	float BaseStamina = 100.0f;

	/** Maximum stamina value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals", meta = (ClampMin = "0.0"))
	float MaxStamina = 100.0f;

	/** Walking speed in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float WalkSpeed = 600.0f;

	/** Running/sprint speed in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float RunSpeed = 1000.0f;

	/** Jump height velocity in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "5000.0"))
	float JumpVelocity = 420.0f;

	/** Damage multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float DamageMultiplier = 1.0f;

	/** Defense multiplier (damage reduction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float DefenseMultiplier = 1.0f;

	/** Experience points multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float XPMultiplier = 1.0f;

	/** Character level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "1"))
	int32 Level = 1;

	/** Custom gameplay tags for this configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	TArray<FName> Tags;

	FCharacterStatsRow() = default;
};

/**
 * Data Asset for character configuration
 * Contains base stats, mesh/animation references, and gameplay settings
 */
UCLASS(BlueprintType)
class UNREALCLAUDE_API UCharacterConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Unique identifier for this character config */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName ConfigId;

	/** Display name shown in UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString DisplayName;

	/** Description of this character configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (MultiLine = "true"))
	FString Description;

	/** Reference to skeletal mesh asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Reference to animation blueprint class */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TSoftClassPtr<UAnimInstance> AnimBlueprintClass;

	/** Base movement stats */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseWalkSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseRunSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseJumpVelocity = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseAcceleration = 2048.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseGroundFriction = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseAirControl = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseGravityScale = 1.0f;

	/** Base combat stats */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float BaseHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float BaseStamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float BaseDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float BaseDefense = 0.0f;

	/** Capsule collision settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CapsuleRadius = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CapsuleHalfHeight = 96.0f;

	/** Optional reference to a stats DataTable for level-based progression */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
	TSoftObjectPtr<UDataTable> StatsTable;

	/** Default row name in stats table to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
	FName DefaultStatsRowName;

	/** Gameplay tags for categorization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	TArray<FName> GameplayTags;

	/** Whether this is an NPC or player character config */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type")
	bool bIsPlayerCharacter = false;

	/** Get the stats row from the referenced table if available */
	UFUNCTION(BlueprintCallable, Category = "Character Config")
	FCharacterStatsRow GetStatsRow(FName RowName = NAME_None) const
	{
		if (!StatsTable.IsNull())
		{
			if (UDataTable* Table = StatsTable.LoadSynchronous())
			{
				FName LookupName = RowName.IsNone() ? DefaultStatsRowName : RowName;
				if (FCharacterStatsRow* Row = Table->FindRow<FCharacterStatsRow>(LookupName, TEXT("")))
				{
					return *Row;
				}
			}
		}
		return FCharacterStatsRow();
	}
};
