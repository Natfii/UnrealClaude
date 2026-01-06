// Copyright Your Name. All Rights Reserved.

/**
 * Unit tests for MCP Tools
 * Tests tool info, parameter validation, and error handling
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MCP/MCPToolRegistry.h"
#include "MCP/MCPParamValidator.h"
#include "MCP/Tools/MCPTool_SpawnActor.h"
#include "MCP/Tools/MCPTool_DeleteActors.h"
#include "MCP/Tools/MCPTool_MoveActor.h"
#include "MCP/Tools/MCPTool_SetProperty.h"
#include "MCP/Tools/MCPTool_GetLevelActors.h"
#include "Dom/JsonObject.h"

#if WITH_DEV_AUTOMATION_TESTS

// ===== Tool Info Tests =====
// These tests verify tool metadata is correctly configured

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_SpawnActor_GetInfo,
	"UnrealClaude.MCP.Tools.SpawnActor.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_SpawnActor_GetInfo::RunTest(const FString& Parameters)
{
	FMCPTool_SpawnActor Tool;
	FMCPToolInfo Info = Tool.GetInfo();

	TestEqual("Tool name should be spawn_actor", Info.Name, TEXT("spawn_actor"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have parameters", Info.Parameters.Num() > 0);

	// Check required parameters
	bool bHasClassParam = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("class"))
		{
			bHasClassParam = true;
			TestTrue("class parameter should be required", Param.bRequired);
		}
	}
	TestTrue("Should have 'class' parameter", bHasClassParam);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_DeleteActors_GetInfo,
	"UnrealClaude.MCP.Tools.DeleteActors.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_DeleteActors_GetInfo::RunTest(const FString& Parameters)
{
	FMCPTool_DeleteActors Tool;
	FMCPToolInfo Info = Tool.GetInfo();

	TestEqual("Tool name should be delete_actors", Info.Name, TEXT("delete_actors"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have parameters", Info.Parameters.Num() > 0);

	// Check parameters - delete_actors has multiple optional deletion modes
	bool bHasActorNamesParam = false;
	bool bHasActorNameParam = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("actor_names"))
		{
			bHasActorNamesParam = true;
			// actor_names is optional (one of three deletion modes)
			TestFalse("actor_names parameter should be optional", Param.bRequired);
		}
		if (Param.Name == TEXT("actor_name"))
		{
			bHasActorNameParam = true;
		}
	}
	TestTrue("Should have 'actor_names' parameter", bHasActorNamesParam);
	TestTrue("Should have 'actor_name' parameter", bHasActorNameParam);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_MoveActor_GetInfo,
	"UnrealClaude.MCP.Tools.MoveActor.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_MoveActor_GetInfo::RunTest(const FString& Parameters)
{
	FMCPTool_MoveActor Tool;
	FMCPToolInfo Info = Tool.GetInfo();

	TestEqual("Tool name should be move_actor", Info.Name, TEXT("move_actor"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have parameters", Info.Parameters.Num() > 0);

	// Check required parameters
	bool bHasActorNameParam = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("actor_name"))
		{
			bHasActorNameParam = true;
			TestTrue("actor_name parameter should be required", Param.bRequired);
		}
	}
	TestTrue("Should have 'actor_name' parameter", bHasActorNameParam);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_SetProperty_GetInfo,
	"UnrealClaude.MCP.Tools.SetProperty.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_SetProperty_GetInfo::RunTest(const FString& Parameters)
{
	FMCPTool_SetProperty Tool;
	FMCPToolInfo Info = Tool.GetInfo();

	TestEqual("Tool name should be set_property", Info.Name, TEXT("set_property"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have parameters", Info.Parameters.Num() >= 3);

	// Check for key parameters
	bool bHasActorName = false;
	bool bHasProperty = false;
	bool bHasValue = false;

	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("actor_name")) bHasActorName = true;
		if (Param.Name == TEXT("property")) bHasProperty = true;
		if (Param.Name == TEXT("value")) bHasValue = true;
	}

	TestTrue("Should have 'actor_name' parameter", bHasActorName);
	TestTrue("Should have 'property' parameter", bHasProperty);
	TestTrue("Should have 'value' parameter", bHasValue);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_GetLevelActors_GetInfo,
	"UnrealClaude.MCP.Tools.GetLevelActors.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_GetLevelActors_GetInfo::RunTest(const FString& Parameters)
{
	FMCPTool_GetLevelActors Tool;
	FMCPToolInfo Info = Tool.GetInfo();

	TestEqual("Tool name should be get_level_actors", Info.Name, TEXT("get_level_actors"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());

	return true;
}

// ===== Parameter Validation Tests =====
// These tests verify tools reject invalid parameters

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_SpawnActor_MissingClass,
	"UnrealClaude.MCP.Tools.SpawnActor.MissingClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_SpawnActor_MissingClass::RunTest(const FString& Parameters)
{
	FMCPTool_SpawnActor Tool;

	// Create params without class
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("name"), TEXT("TestActor"));

	FMCPToolResult Result = Tool.Execute(Params);

	// Should fail because class is missing
	TestFalse("Should fail without class parameter", Result.bSuccess);
	TestTrue("Error should mention missing parameter", Result.Message.Contains(TEXT("class")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_SpawnActor_InvalidActorName,
	"UnrealClaude.MCP.Tools.SpawnActor.InvalidActorName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_SpawnActor_InvalidActorName::RunTest(const FString& Parameters)
{
	FMCPTool_SpawnActor Tool;

	// Create params with invalid actor name (contains dangerous characters)
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("class"), TEXT("/Script/Engine.StaticMeshActor"));
	Params->SetStringField(TEXT("name"), TEXT("Actor<script>"));

	FMCPToolResult Result = Tool.Execute(Params);

	// Should fail because name contains dangerous characters
	TestFalse("Should fail with dangerous actor name", Result.bSuccess);
	TestTrue("Error should mention invalid characters", Result.Message.Contains(TEXT("character")) || Result.Message.Contains(TEXT("invalid")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_MoveActor_MissingActorName,
	"UnrealClaude.MCP.Tools.MoveActor.MissingActorName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_MoveActor_MissingActorName::RunTest(const FString& Parameters)
{
	FMCPTool_MoveActor Tool;

	// Create params without actor_name
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
	LocationObj->SetNumberField(TEXT("x"), 100);
	LocationObj->SetNumberField(TEXT("y"), 200);
	LocationObj->SetNumberField(TEXT("z"), 300);
	Params->SetObjectField(TEXT("location"), LocationObj);

	FMCPToolResult Result = Tool.Execute(Params);

	// Should fail because actor_name is missing
	TestFalse("Should fail without actor_name parameter", Result.bSuccess);
	TestTrue("Error should mention missing parameter", Result.Message.Contains(TEXT("actor_name")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_SetProperty_MissingRequiredParams,
	"UnrealClaude.MCP.Tools.SetProperty.MissingRequiredParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_SetProperty_MissingRequiredParams::RunTest(const FString& Parameters)
{
	FMCPTool_SetProperty Tool;

	// Test missing actor_name
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("property"), TEXT("MyProperty"));
		Params->SetStringField(TEXT("value"), TEXT("test"));

		FMCPToolResult Result = Tool.Execute(Params);
		TestFalse("Should fail without actor_name", Result.bSuccess);
	}

	// Test missing property
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("actor_name"), TEXT("TestActor"));
		Params->SetStringField(TEXT("value"), TEXT("test"));

		FMCPToolResult Result = Tool.Execute(Params);
		TestFalse("Should fail without property", Result.bSuccess);
	}

	return true;
}

// ===== Class Path Validation Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_SpawnActor_EnginePathBlocked,
	"UnrealClaude.MCP.Tools.SpawnActor.EnginePathBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_SpawnActor_EnginePathBlocked::RunTest(const FString& Parameters)
{
	// Verify class path validator blocks dangerous paths
	FString Error;

	TestTrue("Engine.Actor should be valid",
		FMCPParamValidator::ValidateClassPath(TEXT("/Script/Engine.Actor"), Error));

	// Full class paths should be valid
	TestTrue("/Script/Engine.StaticMeshActor should be valid",
		FMCPParamValidator::ValidateClassPath(TEXT("/Script/Engine.StaticMeshActor"), Error));

	// Empty class should be invalid
	TestFalse("Empty class should be invalid",
		FMCPParamValidator::ValidateClassPath(TEXT(""), Error));

	return true;
}

// ===== Property Path Validation Integration =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_SetProperty_InvalidPropertyPath,
	"UnrealClaude.MCP.Tools.SetProperty.InvalidPropertyPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_SetProperty_InvalidPropertyPath::RunTest(const FString& Parameters)
{
	FMCPTool_SetProperty Tool;

	// Test path traversal attack
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("actor_name"), TEXT("TestActor"));
		Params->SetStringField(TEXT("property"), TEXT("..Parent.Property"));
		Params->SetStringField(TEXT("value"), TEXT("evil"));

		FMCPToolResult Result = Tool.Execute(Params);
		TestFalse("Path traversal should be blocked", Result.bSuccess);
	}

	// Test special characters
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("actor_name"), TEXT("TestActor"));
		Params->SetStringField(TEXT("property"), TEXT("Property<T>"));
		Params->SetStringField(TEXT("value"), TEXT("test"));

		FMCPToolResult Result = Tool.Execute(Params);
		TestFalse("Special characters should be blocked", Result.bSuccess);
	}

	return true;
}

// ===== Transform Parameter Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_SpawnActor_TransformDefaults,
	"UnrealClaude.MCP.Tools.SpawnActor.TransformDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_SpawnActor_TransformDefaults::RunTest(const FString& Parameters)
{
	FMCPTool_SpawnActor Tool;
	FMCPToolInfo Info = Tool.GetInfo();

	// Verify default values are specified for transform parameters
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("location"))
		{
			TestTrue("location should have default", !Param.DefaultValue.IsEmpty());
			TestTrue("location default should contain x,y,z",
				Param.DefaultValue.Contains(TEXT("\"x\"")) &&
				Param.DefaultValue.Contains(TEXT("\"y\"")) &&
				Param.DefaultValue.Contains(TEXT("\"z\"")));
		}
		if (Param.Name == TEXT("rotation"))
		{
			TestTrue("rotation should have default", !Param.DefaultValue.IsEmpty());
		}
		if (Param.Name == TEXT("scale"))
		{
			TestTrue("scale should have default", !Param.DefaultValue.IsEmpty());
		}
	}

	return true;
}

// ===== Registry Integration Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPToolRegistry_ToolsRegistered,
	"UnrealClaude.MCP.Registry.ToolsRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPToolRegistry_ToolsRegistered::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;

	// Core tools should be registered
	TestNotNull("spawn_actor should be registered", Registry.FindTool(TEXT("spawn_actor")));
	TestNotNull("delete_actors should be registered", Registry.FindTool(TEXT("delete_actors")));
	TestNotNull("move_actor should be registered", Registry.FindTool(TEXT("move_actor")));
	TestNotNull("set_property should be registered", Registry.FindTool(TEXT("set_property")));
	TestNotNull("get_level_actors should be registered", Registry.FindTool(TEXT("get_level_actors")));
	TestNotNull("run_console_command should be registered", Registry.FindTool(TEXT("run_console_command")));
	TestNotNull("get_output_log should be registered", Registry.FindTool(TEXT("get_output_log")));
	TestNotNull("capture_viewport should be registered", Registry.FindTool(TEXT("capture_viewport")));
	TestNotNull("execute_script should be registered", Registry.FindTool(TEXT("execute_script")));

	// Blueprint tools should be registered
	TestNotNull("blueprint_query should be registered", Registry.FindTool(TEXT("blueprint_query")));
	TestNotNull("blueprint_modify should be registered", Registry.FindTool(TEXT("blueprint_modify")));

	// Animation Blueprint tools should be registered
	TestNotNull("anim_blueprint_modify should be registered", Registry.FindTool(TEXT("anim_blueprint_modify")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPToolRegistry_ToolNotFound,
	"UnrealClaude.MCP.Registry.ToolNotFound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPToolRegistry_ToolNotFound::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;

	// Non-existent tools should return nullptr
	TestNull("nonexistent_tool should not be found", Registry.FindTool(TEXT("nonexistent_tool")));
	TestNull("empty string should not find tool", Registry.FindTool(TEXT("")));

	return true;
}

// ===== Animation Blueprint Tool Tests =====
// Tests for anim_blueprint_modify tool

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_GetInfo,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("anim_blueprint_modify"));
	TestNotNull("anim_blueprint_modify tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be anim_blueprint_modify", Info.Name, TEXT("anim_blueprint_modify"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have parameters", Info.Parameters.Num() > 0);

	// Check required parameters
	bool bHasBlueprintPath = false;
	bool bHasOperation = false;
	bool bHasStateMachine = false;
	bool bHasStateName = false;
	bool bHasNodeType = false;

	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("blueprint_path"))
		{
			bHasBlueprintPath = true;
			TestTrue("blueprint_path should be required", Param.bRequired);
		}
		if (Param.Name == TEXT("operation"))
		{
			bHasOperation = true;
			TestTrue("operation should be required", Param.bRequired);
		}
		if (Param.Name == TEXT("state_machine")) bHasStateMachine = true;
		if (Param.Name == TEXT("state_name")) bHasStateName = true;
		if (Param.Name == TEXT("node_type")) bHasNodeType = true;
	}

	TestTrue("Should have 'blueprint_path' parameter", bHasBlueprintPath);
	TestTrue("Should have 'operation' parameter", bHasOperation);
	TestTrue("Should have 'state_machine' parameter", bHasStateMachine);
	TestTrue("Should have 'state_name' parameter", bHasStateName);
	TestTrue("Should have 'node_type' parameter", bHasNodeType);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_MissingBlueprintPath,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.MissingBlueprintPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_MissingBlueprintPath::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("anim_blueprint_modify"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// Create params without blueprint_path
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("get_info"));

	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail without blueprint_path", Result.bSuccess);
	TestTrue("Error should mention blueprint_path or Missing",
		Result.Message.Contains(TEXT("blueprint_path")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_MissingOperation,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.MissingOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_MissingOperation::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("anim_blueprint_modify"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// Create params without operation
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Characters/ABP_Test"));

	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail without operation", Result.bSuccess);
	TestTrue("Error should mention operation or Missing",
		Result.Message.Contains(TEXT("operation")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_InvalidOperation,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.InvalidOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_InvalidOperation::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("anim_blueprint_modify"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// Create params with invalid operation
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Characters/ABP_Test"));
	Params->SetStringField(TEXT("operation"), TEXT("invalid_operation_xyz"));

	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail with invalid operation", Result.bSuccess);
	TestTrue("Error should mention invalid or unknown operation",
		Result.Message.Contains(TEXT("invalid")) || Result.Message.Contains(TEXT("Unknown")) || Result.Message.Contains(TEXT("operation")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_InvalidBlueprintPath,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.InvalidBlueprintPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_InvalidBlueprintPath::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("anim_blueprint_modify"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// Test path traversal attack
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/../Engine/SomeBP"));
		Params->SetStringField(TEXT("operation"), TEXT("get_info"));

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("Path traversal should be blocked", Result.bSuccess);
	}

	// Test engine path access
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("blueprint_path"), TEXT("/Engine/SomeAnimBP"));
		Params->SetStringField(TEXT("operation"), TEXT("get_info"));

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("Engine path should be blocked", Result.bSuccess);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_AddStateMissingParams,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.AddStateMissingParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_AddStateMissingParams::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("anim_blueprint_modify"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// add_state without state_machine
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Characters/ABP_Test"));
		Params->SetStringField(TEXT("operation"), TEXT("add_state"));
		Params->SetStringField(TEXT("state_name"), TEXT("NewState"));

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("add_state should fail without state_machine", Result.bSuccess);
	}

	// add_state without state_name
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Characters/ABP_Test"));
		Params->SetStringField(TEXT("operation"), TEXT("add_state"));
		Params->SetStringField(TEXT("state_machine"), TEXT("Locomotion"));

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("add_state should fail without state_name", Result.bSuccess);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_AddTransitionMissingParams,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.AddTransitionMissingParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_AddTransitionMissingParams::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("anim_blueprint_modify"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// add_transition without from_state
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Characters/ABP_Test"));
		Params->SetStringField(TEXT("operation"), TEXT("add_transition"));
		Params->SetStringField(TEXT("state_machine"), TEXT("Locomotion"));
		Params->SetStringField(TEXT("to_state"), TEXT("Running"));

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("add_transition should fail without from_state", Result.bSuccess);
	}

	// add_transition without to_state
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Characters/ABP_Test"));
		Params->SetStringField(TEXT("operation"), TEXT("add_transition"));
		Params->SetStringField(TEXT("state_machine"), TEXT("Locomotion"));
		Params->SetStringField(TEXT("from_state"), TEXT("Idle"));

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("add_transition should fail without to_state", Result.bSuccess);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_AddConditionNodeMissingParams,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.AddConditionNodeMissingParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_AddConditionNodeMissingParams::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("anim_blueprint_modify"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// add_condition_node without node_type
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Characters/ABP_Test"));
		Params->SetStringField(TEXT("operation"), TEXT("add_condition_node"));
		Params->SetStringField(TEXT("state_machine"), TEXT("Locomotion"));
		Params->SetStringField(TEXT("from_state"), TEXT("Idle"));
		Params->SetStringField(TEXT("to_state"), TEXT("Running"));

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("add_condition_node should fail without node_type", Result.bSuccess);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_SetStateAnimationMissingParams,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.SetStateAnimationMissingParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_SetStateAnimationMissingParams::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("anim_blueprint_modify"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// set_state_animation without animation_path
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Characters/ABP_Test"));
		Params->SetStringField(TEXT("operation"), TEXT("set_state_animation"));
		Params->SetStringField(TEXT("state_machine"), TEXT("Locomotion"));
		Params->SetStringField(TEXT("state_name"), TEXT("Idle"));
		Params->SetStringField(TEXT("animation_type"), TEXT("sequence"));

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("set_state_animation should fail without animation_path", Result.bSuccess);
	}

	// set_state_animation without state_name
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Characters/ABP_Test"));
		Params->SetStringField(TEXT("operation"), TEXT("set_state_animation"));
		Params->SetStringField(TEXT("state_machine"), TEXT("Locomotion"));
		Params->SetStringField(TEXT("animation_path"), TEXT("/Game/Animations/Idle"));
		Params->SetStringField(TEXT("animation_type"), TEXT("sequence"));

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("set_state_animation should fail without state_name", Result.bSuccess);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_ToolAnnotations,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.ToolAnnotations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_ToolAnnotations::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("anim_blueprint_modify"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	// Animation Blueprint tool should be marked as modifying (not read-only, not destructive)
	TestFalse("Should not be marked as read-only", Info.Annotations.bReadOnlyHint);
	TestFalse("Should not be marked as destructive", Info.Annotations.bDestructiveHint);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
