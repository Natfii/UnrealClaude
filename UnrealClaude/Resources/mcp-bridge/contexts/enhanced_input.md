# Unreal Engine 5.7 Enhanced Input Context

This context is automatically loaded when working with Enhanced Input tools.

## Core Classes

### UInputAction

Represents an abstract input action (e.g., "Jump", "Move", "Look").

```cpp
// Create InputAction programmatically
UInputAction* JumpAction = NewObject<UInputAction>(Package, FName("IA_Jump"), RF_Public | RF_Standalone);
JumpAction->ValueType = EInputActionValueType::Boolean;  // Digital (bool)
```

**Value Types** (`EInputActionValueType`):
- `Boolean` - Digital input (button press), returns `bool`
- `Axis1D` - Single axis (trigger), returns `float`
- `Axis2D` - Two axes (thumbstick), returns `FVector2D`
- `Axis3D` - Three axes (motion controller), returns `FVector`

### UInputMappingContext

Maps physical keys to InputActions. Multiple contexts can be active with different priorities.

```cpp
// Create MappingContext
UInputMappingContext* DefaultContext = NewObject<UInputMappingContext>(Package, FName("IMC_Default"), RF_Public | RF_Standalone);

// Add key mapping
FEnhancedActionKeyMapping& JumpMapping = DefaultContext->MapKey(JumpAction, EKeys::SpaceBar);

// Add multiple keys for same action
DefaultContext->MapKey(MoveAction, EKeys::W);
DefaultContext->MapKey(MoveAction, EKeys::Gamepad_LeftStick_Up);
```

### FEnhancedActionKeyMapping

Individual key-to-action binding within a context.

```cpp
// Access mappings
const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();

// Mapping structure
struct FEnhancedActionKeyMapping
{
    UInputAction* Action;           // The action this triggers
    FKey Key;                       // Physical key (SpaceBar, W, etc.)
    TArray<UInputTrigger*> Triggers;  // When to trigger
    TArray<UInputModifier*> Modifiers; // How to modify input value
};

// Remove mapping
Context->UnmapKey(Action, EKeys::SpaceBar);
```

## Triggers (UInputTrigger subclasses)

Triggers determine WHEN an action fires.

### Basic Triggers

```cpp
// Pressed - Fires once when key pressed
UInputTriggerPressed* Pressed = NewObject<UInputTriggerPressed>();

// Released - Fires once when key released
UInputTriggerReleased* Released = NewObject<UInputTriggerReleased>();

// Down - Fires every frame while held
UInputTriggerDown* Down = NewObject<UInputTriggerDown>();
```

### Timed Triggers

```cpp
// Hold - Fires after held for duration
UInputTriggerHold* Hold = NewObject<UInputTriggerHold>();
Hold->HoldTimeThreshold = 0.5f;  // 500ms hold required
Hold->bIsOneShot = true;         // Fire once, or continuously?

// HoldAndRelease - Fires on release after hold duration
UInputTriggerHoldAndRelease* HoldRelease = NewObject<UInputTriggerHoldAndRelease>();
HoldRelease->HoldTimeThreshold = 1.0f;

// Tap - Quick press and release
UInputTriggerTap* Tap = NewObject<UInputTriggerTap>();
Tap->TapReleaseTimeThreshold = 0.2f;  // Max time between press/release
```

### Advanced Triggers

```cpp
// Pulse - Repeating trigger at interval while held
UInputTriggerPulse* Pulse = NewObject<UInputTriggerPulse>();
Pulse->Interval = 0.1f;  // Fire every 100ms
Pulse->TriggerLimit = 0; // 0 = unlimited

// ChordAction - Requires another action to be active
UInputTriggerChordAction* Chord = NewObject<UInputTriggerChordAction>();
Chord->ChordAction = ShiftAction;  // Only fires when Shift is held
```

## Modifiers (UInputModifier subclasses)

Modifiers transform the raw input value.

### Negate

Inverts input values (e.g., S key for backward movement).

```cpp
UInputModifierNegate* Negate = NewObject<UInputModifierNegate>();
Negate->bX = true;  // Negate X axis
Negate->bY = true;  // Negate Y axis
Negate->bZ = true;  // Negate Z axis
```

### Swizzle Axis

Remaps axes (e.g., convert X input to Y output).

```cpp
UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>();
Swizzle->Order = EInputAxisSwizzle::YXZ;  // Swap X and Y

// Available orders: YXZ, ZYX, XZY, YZX, ZXY
```

### Scalar

Multiplies input by scale factor.

```cpp
UInputModifierScalar* Scalar = NewObject<UInputModifierScalar>();
Scalar->Scalar = FVector(2.0f, 1.0f, 1.0f);  // Double X sensitivity
```

### Dead Zone

Applies dead zone to analog inputs.

```cpp
UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>();
DeadZone->LowerThreshold = 0.2f;  // Ignore below 20%
DeadZone->UpperThreshold = 1.0f;  // Full at 100%
DeadZone->Type = EDeadZoneType::Axial;  // or Radial

// EDeadZoneType::Axial - Apply per-axis
// EDeadZoneType::Radial - Apply to magnitude
```

## Common Key Names (FKey/EKeys)

### Keyboard
- Letters: `A`, `B`, `C`, ... `Z`
- Numbers: `Zero`, `One`, ... `Nine`
- Function: `F1`, `F2`, ... `F12`
- Special: `SpaceBar`, `Enter`, `Escape`, `Tab`, `BackSpace`
- Modifiers: `LeftShift`, `RightShift`, `LeftControl`, `RightControl`, `LeftAlt`, `RightAlt`
- Arrows: `Left`, `Right`, `Up`, `Down`

### Mouse
- Buttons: `LeftMouseButton`, `RightMouseButton`, `MiddleMouseButton`, `ThumbMouseButton`, `ThumbMouseButton2`
- Axes: `MouseX`, `MouseY`, `MouseScrollUp`, `MouseScrollDown`, `MouseWheelAxis`

### Gamepad
- Face Buttons: `Gamepad_FaceButton_Bottom` (A/X), `Gamepad_FaceButton_Right` (B/O), `Gamepad_FaceButton_Left` (X/Square), `Gamepad_FaceButton_Top` (Y/Triangle)
- Shoulders: `Gamepad_LeftShoulder`, `Gamepad_RightShoulder`
- Triggers: `Gamepad_LeftTrigger`, `Gamepad_RightTrigger`, `Gamepad_LeftTriggerAxis`, `Gamepad_RightTriggerAxis`
- Sticks: `Gamepad_LeftStick_Up/Down/Left/Right`, `Gamepad_RightStick_Up/Down/Left/Right`
- Stick Buttons: `Gamepad_LeftThumbstick`, `Gamepad_RightThumbstick`
- D-Pad: `Gamepad_DPad_Up`, `Gamepad_DPad_Down`, `Gamepad_DPad_Left`, `Gamepad_DPad_Right`
- Special: `Gamepad_Special_Left` (Select/Share), `Gamepad_Special_Right` (Start/Options)

## Runtime Usage (In Game Code)

### Setting Up Enhanced Input in C++

```cpp
// In PlayerController or Pawn
void AMyPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Get Enhanced Input subsystem
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        // Add mapping context with priority
        Subsystem->AddMappingContext(DefaultMappingContext, 0);
    }

    // Bind actions
    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
    {
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Triggered, this, &AMyPlayerController::OnJump);
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyPlayerController::OnMove);
    }
}

// Action handlers
void AMyPlayerController::OnJump(const FInputActionValue& Value)
{
    bool bJumping = Value.Get<bool>();
}

void AMyPlayerController::OnMove(const FInputActionValue& Value)
{
    FVector2D MovementVector = Value.Get<FVector2D>();
}
```

### ETriggerEvent

- `Started` - Trigger just activated
- `Ongoing` - Trigger still active (for Hold, etc.)
- `Canceled` - Trigger was interrupted
- `Triggered` - Primary fire event
- `Completed` - Trigger finished (for Hold, etc.)

## MCP Tool Operations

The `enhanced_input` MCP tool supports these operations:

| Operation | Description |
|-----------|-------------|
| `create_input_action` | Create new UInputAction asset |
| `create_mapping_context` | Create new UInputMappingContext asset |
| `add_mapping` | Add key→action binding to context |
| `remove_mapping` | Remove binding from context |
| `add_trigger` | Add trigger to existing mapping |
| `add_modifier` | Add modifier to existing mapping |
| `query_context` | List all mappings in a context |
| `query_action` | Get InputAction details |

### Example Workflow

```json
// 1. Create InputAction
{"operation": "create_input_action", "name": "IA_Jump", "value_type": "Digital"}

// 2. Create MappingContext
{"operation": "create_mapping_context", "name": "IMC_Default"}

// 3. Add key mapping
{"operation": "add_mapping",
 "context_path": "/Game/Input/IMC_Default",
 "action_path": "/Game/Input/IA_Jump",
 "key": "SpaceBar"}

// 4. Add hold trigger (optional)
{"operation": "add_trigger",
 "context_path": "/Game/Input/IMC_Default",
 "action_path": "/Game/Input/IA_Jump",
 "trigger_type": "Hold",
 "hold_time": 0.5}

// 5. Query to verify
{"operation": "query_context", "context_path": "/Game/Input/IMC_Default"}
```
