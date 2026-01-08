# Unreal Engine 5.7 Blueprint Graph Context

This context is automatically loaded when working with Blueprint manipulation tools.

## Core Classes

### UBlueprint
Base class for all Blueprint assets. Key properties:
- `GeneratedClass` - The UClass generated from this blueprint
- `ParentClass` - The class this blueprint inherits from
- `BlueprintType` - BPTYPE_Normal, BPTYPE_Const, BPTYPE_MacroLibrary, etc.

### UEdGraph
Container for graph nodes. Every Blueprint has multiple graphs:
- **EventGraph** - Main execution graph
- **ConstructionScript** - Called on actor construction
- **Function graphs** - User-defined functions
- **Macro graphs** - Reusable node sequences

### UEdGraphNode
Base class for all graph nodes. Key members:
- `NodeGuid` - Unique identifier
- `NodePosX`, `NodePosY` - Position in graph
- `Pins` - Array of UEdGraphPin

### UEdGraphPin
Connection point on a node:
- `PinName` - Identifier
- `PinType` - FEdGraphPinType with category (PC_Exec, PC_Boolean, PC_Int, PC_Real, etc.)
- `Direction` - EGPD_Input or EGPD_Output
- `LinkedTo` - Array of connected pins
- `DefaultValue` - Default value as string

## UK2Node Hierarchy

```
UK2Node (Blueprint node base)
├── UK2Node_CallFunction      - Function calls
├── UK2Node_VariableGet       - Get variable value
├── UK2Node_VariableSet       - Set variable value
├── UK2Node_Event             - Event nodes (BeginPlay, Tick)
├── UK2Node_IfThenElse        - Branch node
├── UK2Node_MacroInstance     - Macro usage
├── UK2Node_Composite         - Collapsed graph
└── UK2Node_FunctionEntry     - Function entry point
```

## FBlueprintEditorUtils

Utility class for Blueprint manipulation. Key functions:

```cpp
// Find blueprint from graph
UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);

// Add member variable
FBlueprintEditorUtils::AddMemberVariable(
    Blueprint,
    TEXT("MyVariable"),
    FEdGraphPinType(UEdGraphSchema_K2::PC_Float)
);

// Remove member variable
FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, TEXT("MyVariable"));

// Mark blueprint as modified
FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

// Compile blueprint
FKismetEditorUtilities::CompileBlueprint(Blueprint);

// Add new function graph
UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
    Blueprint,
    TEXT("MyFunction"),
    UEdGraph::StaticClass(),
    UEdGraphSchema_K2::StaticClass()
);
```

## Pin Type Categories (PC_*)

| Category | Description | C++ Type |
|----------|-------------|----------|
| `PC_Exec` | Execution pin (white) | N/A |
| `PC_Boolean` | Bool pin (red) | bool |
| `PC_Byte` | Byte pin | uint8 |
| `PC_Int` | Integer pin (cyan) | int32 |
| `PC_Int64` | 64-bit integer | int64 |
| `PC_Real` | Float/Double (green) | float/double |
| `PC_Name` | FName pin | FName |
| `PC_String` | FString pin (magenta) | FString |
| `PC_Text` | FText pin (pink) | FText |
| `PC_Struct` | Struct pin | UScriptStruct* |
| `PC_Object` | Object reference (blue) | UObject* |
| `PC_Class` | Class reference (purple) | UClass* |
| `PC_SoftObject` | Soft object ref | TSoftObjectPtr |
| `PC_SoftClass` | Soft class ref | TSoftClassPtr |
| `PC_Enum` | Enum value | UEnum* |
| `PC_Wildcard` | Any type (grey) | varies |

## Creating Nodes Programmatically

```cpp
// Spawn a function call node
UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
CallNode->FunctionReference.SetExternalMember(
    GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_FloatFloat),
    UKismetMathLibrary::StaticClass()
);
CallNode->AllocateDefaultPins();
Graph->AddNode(CallNode, false, false);
CallNode->NodePosX = 200;
CallNode->NodePosY = 100;

// Spawn a variable get node
UK2Node_VariableGet* VarGet = NewObject<UK2Node_VariableGet>(Graph);
VarGet->VariableReference.SetSelfMember(FName("MyVariable"));
VarGet->AllocateDefaultPins();
Graph->AddNode(VarGet, false, false);
```

## Connecting Pins

```cpp
// Find pins by name
UEdGraphPin* OutputPin = SourceNode->FindPin(TEXT("ReturnValue"));
UEdGraphPin* InputPin = TargetNode->FindPin(TEXT("A"));

// Connect pins
if (OutputPin && InputPin)
{
    const UEdGraphSchema* Schema = Graph->GetSchema();
    Schema->TryCreateConnection(OutputPin, InputPin);
}
```

## MCP Blueprint Operations

Available via `blueprint_modify` tool:

| Operation | Description |
|-----------|-------------|
| `add_variable` | Add member variable to blueprint |
| `remove_variable` | Remove member variable |
| `add_function` | Create new function graph |
| `remove_function` | Delete function graph |
| `add_node` | Add node to graph |
| `remove_node` | Remove node from graph |
| `connect_pins` | Wire two pins together |
| `disconnect_pins` | Break pin connection |
| `set_pin_default` | Set default value on pin |

Available via `blueprint_query` tool:

| Operation | Description |
|-----------|-------------|
| `get_variables` | List all member variables |
| `get_functions` | List all function graphs |
| `get_graph_nodes` | Get nodes in a graph |
| `get_node_pins` | Get pins on a node |
| `find_references` | Find usages of variable/function |

## Compilation

```cpp
// Full recompile
FKismetEditorUtilities::CompileBlueprint(Blueprint);

// Check for compile errors
if (Blueprint->Status == BS_Error)
{
    // Blueprint has errors - check Blueprint->Message
}

// Mark dirty (needs save)
Blueprint->MarkPackageDirty();
```

## Best Practices

1. **Always call AllocateDefaultPins()** after creating nodes
2. **Use Schema->TryCreateConnection()** for type-safe connections
3. **MarkBlueprintAsModified()** after any changes
4. **Compile after modifications** to validate changes
5. **Check pin compatibility** before connecting (use Schema->ArePinsCompatible)
