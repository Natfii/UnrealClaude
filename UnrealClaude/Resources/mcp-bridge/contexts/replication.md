# Unreal Engine 5.7 Replication & Networking Context

This context is automatically loaded when working with multiplayer/networking.

## Property Replication

### Basic Replicated Property
```cpp
UPROPERTY(Replicated)
float Health;

// In GetLifetimeReplicatedProps
void AMyActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMyActor, Health);
}
```

### ReplicatedUsing (RepNotify)
Calls a function when property replicates to clients.

```cpp
UPROPERTY(ReplicatedUsing = OnRep_Health)
float Health;

// RepNotify callback - MUST have UFUNCTION()
UFUNCTION()
void OnRep_Health();

// Or with old value parameter
UFUNCTION()
void OnRep_Health(float OldHealth);

// Implementation
void AMyActor::OnRep_Health(float OldHealth)
{
    // React to health change on client
    if (Health < OldHealth)
    {
        PlayDamageEffect();
    }
}

// In GetLifetimeReplicatedProps
DOREPLIFETIME(AMyActor, Health);
```

**Important**: In C++, OnRep only fires on clients. If server needs the callback too, call it manually after changing the value.

## Replication Conditions

### DOREPLIFETIME_CONDITION
```cpp
void AMyActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Always replicate
    DOREPLIFETIME(AMyActor, AlwaysReplicated);

    // Only to owning client
    DOREPLIFETIME_CONDITION(AMyActor, OwnerOnlyData, COND_OwnerOnly);

    // Only if actor has initial authority
    DOREPLIFETIME_CONDITION(AMyActor, InitialOnlyData, COND_InitialOnly);

    // Skip owner, replicate to everyone else
    DOREPLIFETIME_CONDITION(AMyActor, SkipOwner, COND_SkipOwner);

    // Only on simulated proxies
    DOREPLIFETIME_CONDITION(AMyActor, SimulatedData, COND_SimulatedOnly);

    // Only on autonomous proxy
    DOREPLIFETIME_CONDITION(AMyActor, AutonomousData, COND_AutonomousOnly);
}
```

### Condition Values
| Condition | Description |
|-----------|-------------|
| `COND_None` | No condition, always replicate |
| `COND_InitialOnly` | Only on initial replication |
| `COND_OwnerOnly` | Only to owning connection |
| `COND_SkipOwner` | Everyone except owner |
| `COND_SimulatedOnly` | Simulated actors only |
| `COND_AutonomousOnly` | Autonomous proxy only |
| `COND_SimulatedOrPhysics` | Simulated or physics |
| `COND_InitialOrOwner` | Initial or to owner |
| `COND_Custom` | Custom condition check |

### Force RepNotify
```cpp
// Always trigger OnRep even if value is same
DOREPLIFETIME_CONDITION_NOTIFY(AMyActor, Health, COND_None, REPNOTIFY_Always);
```

## Push Model Replication (UE5)

More efficient - only marks dirty when actually changed.

```cpp
// Enable push model for class
bReplicateUsingRegisteredSubObjectList = true;

// Mark property dirty when changed
void AMyActor::SetHealth(float NewHealth)
{
    Health = NewHealth;
    MARK_PROPERTY_DIRTY_FROM_NAME(AMyActor, Health, this);
}

// In GetLifetimeReplicatedProps
FDoRepLifetimeParams Params;
Params.bIsPushBased = true;
DOREPLIFETIME_WITH_PARAMS_FAST(AMyActor, Health, Params);
```

## RPCs (Remote Procedure Calls)

### Server RPC (Client calls, Server executes)
```cpp
UFUNCTION(Server, Reliable, WithValidation)
void ServerDoAction(FVector Target);

void AMyActor::ServerDoAction_Implementation(FVector Target)
{
    // Runs on server
    PerformAction(Target);
}

bool AMyActor::ServerDoAction_Validate(FVector Target)
{
    // Validate input - return false to disconnect cheater
    return Target.Size() < 10000.0f;
}
```

### Client RPC (Server calls, Client executes)
```cpp
UFUNCTION(Client, Reliable)
void ClientShowMessage(const FString& Message);

void AMyActor::ClientShowMessage_Implementation(const FString& Message)
{
    // Runs on owning client
    ShowUIMessage(Message);
}
```

### Multicast RPC (Server calls, All clients execute)
```cpp
UFUNCTION(NetMulticast, Reliable)
void MulticastPlayEffect(FVector Location);

void AMyActor::MulticastPlayEffect_Implementation(FVector Location)
{
    // Runs on server AND all clients
    SpawnEffect(Location);
}
```

### RPC Specifiers
| Specifier | Description |
|-----------|-------------|
| `Server` | Client to server |
| `Client` | Server to owning client |
| `NetMulticast` | Server to all clients |
| `Reliable` | Guaranteed delivery (use sparingly) |
| `Unreliable` | May be dropped (for frequent updates) |
| `WithValidation` | Adds _Validate function for cheat prevention |

## Actor Replication Setup

```cpp
AMyActor::AMyActor()
{
    // Enable replication
    bReplicates = true;

    // Replicate movement
    SetReplicateMovement(true);

    // Net update frequency
    NetUpdateFrequency = 66.0f;    // Times per second
    MinNetUpdateFrequency = 33.0f; // Minimum

    // Priority (relative to other actors)
    NetPriority = 1.0f;
}
```

## Network Role Checks

```cpp
// Check authority (server owns this actor)
if (HasAuthority())
{
    // Server-side logic
}

// Check local role
ENetRole LocalRole = GetLocalRole();
switch (LocalRole)
{
    case ROLE_Authority:        // Server
    case ROLE_AutonomousProxy:  // Locally controlled
    case ROLE_SimulatedProxy:   // Replicated from server
    case ROLE_None:             // Not replicated
}

// Check remote role
ENetRole RemoteRole = GetRemoteRole();
```

## Ownership

```cpp
// Set owner (for COND_OwnerOnly)
Actor->SetOwner(PlayerController);

// Get owner
AActor* Owner = Actor->GetOwner();

// Check if locally owned
bool bIsLocallyOwned = Actor->IsOwnedBy(LocalPlayerController);
```

## Replicating UObjects

### Subobject Replication
```cpp
// In constructor
Component = CreateDefaultSubobject<UMyComponent>(TEXT("MyComponent"));

// Or register dynamically
void AMyActor::BeginPlay()
{
    Super::BeginPlay();
    AddReplicatedSubObject(DynamicComponent);
}

// Enable subobject list
bReplicateUsingRegisteredSubObjectList = true;
```

## Connection Handling

```cpp
// Check if player controller
APlayerController* PC = Cast<APlayerController>(GetOwner());
if (PC && PC->IsLocalController())
{
    // This is the local player's controller
}

// Get net connection
UNetConnection* Connection = GetNetConnection();
```

## Best Practices

1. **Minimize replicated properties** - only replicate what's needed
2. **Use conditions** - COND_OwnerOnly for private data
3. **Prefer Unreliable** for frequent updates (movement)
4. **Use Reliable sparingly** - guaranteed but costly
5. **Validate Server RPCs** - prevent cheating
6. **Use Push Model** for large classes with few changes
7. **Check HasAuthority()** before server-only logic
8. **Set appropriate NetUpdateFrequency** - balance accuracy vs bandwidth
