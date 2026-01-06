# UnrealClaude - Claude Code Instructions for Unreal Engine 5.7

This file provides guidance to Claude Code when working with the UnrealClaude plugin and Unreal Engine 5.7 projects.

## Project Overview

**UnrealClaude** is an Unreal Engine 5.7 plugin that provides MCP (Model Context Protocol) integration, enabling Claude AI to interact directly with the Unreal Editor via REST API tools.

### Key Directories
- `Source/UnrealClaude/Private/MCP/` - MCP server and tool implementations
- `Source/UnrealClaude/Private/MCP/Tools/` - Individual MCP tools
- `Source/UnrealClaude/Private/Tests/` - Automation tests
- `Resources/mcp-bridge/` - Node.js MCP bridge

### Build Commands
```bash
# Build plugin (from project root)
"C:/Xboxgames/UE_5.7/Engine/Build/BatchFiles/RunUAT.bat" BuildPlugin -Plugin="UnrealClaude.uplugin" -Package="../PluginBuild" -TargetPlatforms=Win64 -Rocket

# Run tests (in Unreal Editor console)
Automation RunTests UnrealClaude
```

---

## Unreal Engine 5.7 C++ Standards

### IMPORTANT: Stay Focused on UE 5.7
- This project targets **Unreal Engine 5.7.1** exclusively
- Use UE 5.7 API patterns and conventions
- Do NOT suggest deprecated APIs or patterns from older engine versions
- When uncertain about API availability, verify against UE 5.7 documentation

### File Organization
- **Maximum 500 lines per file** - Split large files into logical units
- **Maximum 50 lines per function** - Extract helper functions for clarity
- **Private/** folder for implementation files
- **Public/** folder for headers meant for external use

### Header File Template
```cpp
// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "YourClass.generated.h"  // For UObject-derived classes

/**
 * Brief description of the class purpose.
 *
 * Detailed description if needed.
 */
UCLASS()
class MODULENAME_API UYourClass : public UObject
{
    GENERATED_BODY()

public:
    // Public interface

protected:
    // Protected members

private:
    // Private implementation
};
```

### UPROPERTY Specifiers (UE 5.7)
```cpp
// Editor-visible and Blueprint-accessible
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category")
int32 EditableProperty;

// Read-only in Blueprint
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runtime")
bool bRuntimeValue;

// Class defaults only (not per-instance)
UPROPERTY(EditDefaultsOnly, Category = "Config")
TSubclassOf<AActor> ActorClass;

// Instance-only editing
UPROPERTY(EditInstanceOnly, Category = "Level")
AActor* TargetActor;

// Replicated property
UPROPERTY(ReplicatedUsing = OnRep_Value)
float ReplicatedValue;

// Transient (not serialized)
UPROPERTY(Transient)
float CachedValue;

// With metadata constraints
UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "100.0"))
float Percentage;

// Asset references (UE 5.x preferred)
UPROPERTY(EditAnywhere)
TObjectPtr<UTexture2D> Texture;  // Hard reference

UPROPERTY(EditAnywhere)
TSoftObjectPtr<UStaticMesh> MeshAsset;  // Soft reference
```

### UFUNCTION Specifiers (UE 5.7)
```cpp
// Blueprint callable (has execution pins)
UFUNCTION(BlueprintCallable, Category = "MyCategory")
void DoSomething();

// Blueprint pure (no execution pins, no side effects)
UFUNCTION(BlueprintPure, Category = "MyCategory")
int32 GetValue() const;

// Blueprint implementable event
UFUNCTION(BlueprintImplementableEvent, Category = "Events")
void OnSomethingHappened();

// Blueprint native event (C++ default, BP override)
UFUNCTION(BlueprintNativeEvent, Category = "Events")
void OnEvent();
void OnEvent_Implementation();

// Editor callable (for debugging)
UFUNCTION(CallInEditor, BlueprintCallable)
void DebugFunction();

// With return display name
UFUNCTION(BlueprintCallable, meta = (ReturnDisplayName = "Success"))
bool TryDoSomething();
```

### Blueprint Function Library (UE 5.7)
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyFunctionLibrary.generated.h"

UCLASS()
class UMyFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Pure static function
    UFUNCTION(BlueprintPure, Category = "Utility")
    static float CalculateDistance(const FVector& A, const FVector& B);

    // Callable static function with world context
    UFUNCTION(BlueprintCallable, Category = "Spawning",
        meta = (WorldContext = "WorldContextObject"))
    static AActor* SpawnActorSafe(UObject* WorldContextObject,
        TSubclassOf<AActor> ActorClass, FTransform SpawnTransform);
};
```

---

## Animation Blueprint API (UE 5.7)

### State Machine Bindings
```cpp
// Add state entry callback
void UMyAnimInstance::SetupStateMachineBindings()
{
    AddNativeStateEntryBinding(
        FName("Locomotion"),      // State machine name
        FName("Idle"),            // State name
        FOnGraphStateChanged::CreateUObject(this, &UMyAnimInstance::OnEnterIdle)
    );

    AddNativeStateExitBinding(
        FName("Locomotion"),
        FName("Running"),
        FOnGraphStateChanged::CreateUObject(this, &UMyAnimInstance::OnExitRunning)
    );

    AddNativeTransitionBinding(
        FName("Locomotion"),
        FName("Idle"),            // From state
        FName("Running"),         // To state
        FCanTakeTransition::CreateUObject(this, &UMyAnimInstance::CanTransitionToRunning),
        FName("IdleToRunning")    // Transition name
    );
}
```

### Animation State Machine Library Functions
```cpp
// Get current state name
FName CurrentState = UAnimationStateMachineLibrary::GetState(UpdateContext, StateMachineRef);

// Get remaining animation time
float TimeRemaining = UAnimationStateMachineLibrary::GetRelevantAnimTimeRemaining(
    UpdateContext, StateResultRef);

// Get remaining time as fraction (0.0 - 1.0)
float TimeFraction = UAnimationStateMachineLibrary::GetRelevantAnimTimeRemainingFraction(
    UpdateContext, StateResultRef);

// Check blend state
bool bBlendingIn = UAnimationStateMachineLibrary::IsStateBlendingIn(UpdateContext, StateResultRef);
bool bBlendingOut = UAnimationStateMachineLibrary::IsStateBlendingOut(UpdateContext, StateResultRef);

// Manually set state (use carefully)
UAnimationStateMachineLibrary::SetState(
    UpdateContext, StateMachineRef,
    FName("TargetState"),
    0.25f,                        // Blend duration
    ETransitionLogicType::StandardBlend,
    nullptr,                      // Blend profile
    EAlphaBlendOption::Linear,
    nullptr                       // Custom curve
);
```

---

## Editor Plugin Development (UE 5.7)

### Slate Widget Patterns
```cpp
// Creating widgets with SNew
TSharedRef<SWidget> CreateWidget()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Hello")))
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SButton)
            .OnClicked(this, &SMyWidget::OnButtonClicked)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Click Me")))
            ]
        ];
}

// Assigning to member variable with SAssignNew
TSharedPtr<SEditableTextBox> TextBox;
SAssignNew(TextBox, SEditableTextBox)
    .HintText(FText::FromString(TEXT("Enter text...")));
```

### Editor Subsystem Pattern
```cpp
UCLASS()
class UMyEditorSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "MySubsystem")
    void DoEditorThing();
};
```

---

## MCP Tool Development Guidelines

### Creating New MCP Tools
1. Create header in `Private/MCP/Tools/MCPTool_YourTool.h`
2. Inherit from `FMCPToolBase`
3. Implement `GetInfo()` with proper parameters and annotations
4. Implement `Execute()` with parameter validation
5. Register in `MCPToolRegistry.h`
6. Add tests in `Private/Tests/MCPToolTests.cpp`

### Tool Parameter Validation Pattern
```cpp
FMCPToolResult FMCPTool_YourTool::Execute(const TSharedRef<FJsonObject>& Params)
{
    // Extract and validate required parameters
    FString RequiredParam;
    TOptional<FMCPToolResult> Error;
    if (!ExtractRequiredString(Params, TEXT("param_name"), RequiredParam, Error))
    {
        return Error.GetValue();
    }

    // Validate paths for security
    if (!ValidateBlueprintPathParam(BlueprintPath, Error))
    {
        return Error.GetValue();
    }

    // Extract optional parameters with defaults
    int32 OptionalInt = Params->GetIntegerField(TEXT("optional_param"));

    // Do work...

    return FMCPToolResult::Success(TEXT("Operation completed"));
}
```

### Tool Annotations
```cpp
// Read-only tool (safe, no modifications)
Info.Annotations = FMCPToolAnnotations::ReadOnly();

// Modifying tool (changes state, reversible)
Info.Annotations = FMCPToolAnnotations::Modifying();

// Destructive tool (cannot be undone via MCP)
Info.Annotations = FMCPToolAnnotations::Destructive();
```

---

## Testing Standards

### Automation Test Pattern
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMyTest,
    "UnrealClaude.Category.TestName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMyTest::RunTest(const FString& Parameters)
{
    // Test assertions
    TestTrue("Description of what should be true", bCondition);
    TestFalse("Description of what should be false", bCondition);
    TestEqual("Values should match", ActualValue, ExpectedValue);
    TestNotNull("Pointer should not be null", Pointer);
    TestNull("Pointer should be null", Pointer);

    return true;
}
```

### Test Categories for This Project
- `UnrealClaude.MCP.Tools.*` - Individual tool tests
- `UnrealClaude.MCP.ParamValidator.*` - Parameter validation tests
- `UnrealClaude.MCP.Registry.*` - Tool registry tests

---

## Security Considerations

### Parameter Validation (ALWAYS DO THIS)
1. **Path Validation**: Block `/Engine/`, `/Script/`, path traversal (`../`)
2. **Actor Name Validation**: Block special characters `<>|&;$(){}[]!*?~`
3. **Console Command Validation**: Block dangerous commands (quit, crash, shutdown)
4. **Numeric Validation**: Check for NaN, Infinity, reasonable bounds

### Never Do
- Execute arbitrary code without permission dialogs
- Allow access to engine internals
- Skip parameter validation
- Trust user input without sanitization

---

## Common UE 5.7 Patterns

### Safe Actor Iteration
```cpp
for (TActorIterator<AActor> It(GetWorld()); It; ++It)
{
    AActor* Actor = *It;
    if (IsValid(Actor))
    {
        // Process actor
    }
}
```

### Deferred Spawning
```cpp
AActor* Actor = GetWorld()->SpawnActorDeferred<AMyActor>(
    AMyActor::StaticClass(),
    SpawnTransform,
    nullptr,
    nullptr,
    ESpawnActorCollisionHandlingMethod::AlwaysSpawn
);

if (Actor)
{
    // Configure actor before it begins play
    Actor->SomeProperty = Value;
    Actor->FinishSpawning(SpawnTransform);
}
```

### Asset Loading (UE 5.7)
```cpp
// Synchronous load (editor only, avoid in runtime)
UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);

// Async load
FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
StreamableManager.RequestAsyncLoad(AssetPath,
    FStreamableDelegate::CreateUObject(this, &UMyClass::OnAssetLoaded));
```

### Property Iteration
```cpp
for (TFieldIterator<FProperty> PropIt(Object->GetClass()); PropIt; ++PropIt)
{
    FProperty* Property = *PropIt;
    FString PropertyName = Property->GetName();

    if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
    {
        // Handle numeric property
    }
    else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        // Handle struct property
    }
}
```

---

## Commit Standards

When committing changes to this project:
- `feat:` New MCP tools or features
- `fix:` Bug fixes
- `test:` Test additions or fixes
- `docs:` Documentation updates
- `refactor:` Code refactoring without behavior changes

Always run `Automation RunTests UnrealClaude` before committing.
