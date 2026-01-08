# Unreal Engine 5.7 Animation Blueprint Context

This context is automatically loaded when working with Animation Blueprint tools.

## UAnimInstance - Core Animation Class

The `UAnimInstance` class is the C++ base for Animation Blueprints.

### State Machine Bindings

```cpp
// Bind in NativeInitializeAnimation (called once on init)
void UMyAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    // State entry binding
    AddNativeStateEntryBinding(
        FName("Locomotion"),    // State machine name
        FName("Idle"),          // State name
        FOnGraphStateChanged::CreateUObject(this, &UMyAnimInstance::OnEnterIdle)
    );

    // State exit binding
    AddNativeStateExitBinding(
        FName("Locomotion"),
        FName("Running"),
        FOnGraphStateChanged::CreateUObject(this, &UMyAnimInstance::OnExitRunning)
    );
}

// Callback signature - MUST match exactly
void UMyAnimInstance::OnEnterIdle(
    const FAnimNode_StateMachine& Machine,
    int32 PrevStateIndex,
    int32 NextStateIndex)
{
    // React to entering Idle state
}
```

### Animation Time Functions

```cpp
// Get remaining time in seconds (use in transition rules)
float GetRelevantAnimTimeRemaining();

// Get remaining time as fraction 0.0-1.0
float GetRelevantAnimTimeRemainingFraction();

// Common transition condition: when <10% animation remains
// Blueprint: GetRelevantAnimTimeRemainingFraction <= 0.1
```

### Thread-Safe Update (Fast Path)

```cpp
// Override for thread-safe property updates
void UMyAnimInstance::BlueprintThreadSafeUpdateAnimation(float DeltaTime)
{
    // Safe to read game state and update anim properties
    Speed = GetOwningActor()->GetVelocity().Size();
    bIsInAir = !GetOwningActor()->GetCharacterMovement()->IsMovingOnGround();
}
```

## Animation Blueprint Graph Nodes

### State Machine Structure
- **UAnimGraphNode_StateMachine** - State machine container node
- **UAnimStateNode** - Individual state (contains animation logic)
- **UAnimStateTransitionNode** - Transition between states
- **UAnimStateEntryNode** - Entry point into state machine

### Common Animation Nodes
- **UAnimGraphNode_SequencePlayer** - Play animation sequence
- **UAnimGraphNode_BlendSpacePlayer** - Sample blend space
- **UAnimGraphNode_LayeredBoneBlend** - Layer animations by bone
- **UAnimGraphNode_TwoWayBlend** - Blend between two poses

## Transition Rule Graphs

Transition graphs use K2 nodes for logic:

```cpp
// Common transition condition nodes
UK2Node_TransitionRuleGetter   // Get time remaining, state info
UK2Node_CallFunction           // Call comparison functions (Greater, Less, Equal)
UK2Node_VariableGet            // Get blueprint variables
```

### Comparison Functions (KismetMathLibrary)
- `Greater_FloatFloat`, `Less_FloatFloat`, `GreaterEqual_FloatFloat`
- `Greater_IntInt`, `Less_IntInt` - for integer comparisons
- `EqualEqual_BoolBool`, `NotEqual_BoolBool` - for bool comparisons

### Pin Type Detection
When connecting variable pins to comparison nodes, ensure type matching:
- **PC_Real** (float/double) → Use `*_FloatFloat` or `*_DoubleDouble`
- **PC_Int** → Use `*_IntInt`
- **PC_Boolean** → Use `*_BoolBool`

## Blend Spaces

### 2D Blend Space (UBlendSpace)
```cpp
// Bind parameters to blueprint variables
TMap<FString, FString> Bindings;
Bindings.Add(TEXT("Speed"), TEXT("Speed"));        // X axis
Bindings.Add(TEXT("Direction"), TEXT("Direction")); // Y axis

FAnimationBlueprintUtils::SetStateBlendSpace(
    AnimBP, TEXT("Locomotion"), TEXT("Moving"),
    TEXT("/Game/BlendSpaces/Locomotion_BS"),
    Bindings, OutError);
```

### 1D Blend Space (UBlendSpace1D)
```cpp
FAnimationBlueprintUtils::SetStateBlendSpace1D(
    AnimBP, TEXT("Locomotion"), TEXT("Moving"),
    TEXT("/Game/BlendSpaces/Speed_BS1D"),
    TEXT("Speed"),  // Single axis binding
    OutError);
```

## MCP Animation Operations

Available operations via `anim_blueprint_modify` tool:

| Operation | Description |
|-----------|-------------|
| `create_state_machine` | Create new state machine in AnimGraph |
| `add_state` | Add state to state machine |
| `add_transition` | Create transition between states |
| `set_state_animation` | Assign animation to state |
| `add_condition_node` | Add condition to transition graph |
| `connect_condition_nodes` | Wire condition logic |
| `add_comparison_chain` | Create variable → comparison → result chain |
| `set_entry_state` | Set which state is the entry point |
| `get_state_machine_diagram` | Get ASCII visualization |
| `validate_blueprint` | Check for errors and warnings |

## Best Practices

1. **Use Thread-Safe Updates**: Override `BlueprintThreadSafeUpdateAnimation` for Fast Path optimization
2. **Bind in NativeInitializeAnimation**: State bindings should be set up once at init
3. **Match Pin Types**: Ensure comparison nodes match the variable type being compared
4. **Compile After Changes**: Always compile the Animation Blueprint after modifications
5. **Auto-Save Enabled**: Blueprints auto-save after successful compilation
