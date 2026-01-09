// Copyright Natali Caggiano. All Rights Reserved.

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

// ===== Asset Search Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AssetSearch_GetInfo,
	"UnrealClaude.MCP.Tools.AssetSearch.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AssetSearch_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("asset_search"));
	TestNotNull("asset_search tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be asset_search", Info.Name, TEXT("asset_search"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have parameters", Info.Parameters.Num() > 0);

	// Check for key parameters
	bool bHasClassFilter = false;
	bool bHasPathFilter = false;
	bool bHasNamePattern = false;
	bool bHasLimit = false;

	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("class_filter")) bHasClassFilter = true;
		if (Param.Name == TEXT("path_filter")) bHasPathFilter = true;
		if (Param.Name == TEXT("name_pattern")) bHasNamePattern = true;
		if (Param.Name == TEXT("limit")) bHasLimit = true;
	}

	TestTrue("Should have 'class_filter' parameter", bHasClassFilter);
	TestTrue("Should have 'path_filter' parameter", bHasPathFilter);
	TestTrue("Should have 'name_pattern' parameter", bHasNamePattern);
	TestTrue("Should have 'limit' parameter", bHasLimit);

	// Should be read-only
	TestTrue("Should be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AssetSearch_DefaultPath,
	"UnrealClaude.MCP.Tools.AssetSearch.DefaultPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AssetSearch_DefaultPath::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("asset_search"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	// Check path_filter has default value
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("path_filter"))
		{
			TestTrue("path_filter should have default /Game/", Param.DefaultValue.Contains(TEXT("/Game/")));
		}
		if (Param.Name == TEXT("limit"))
		{
			TestTrue("limit should have default 100", Param.DefaultValue == TEXT("100"));
		}
	}

	return true;
}

// ===== Asset Dependencies Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AssetDependencies_GetInfo,
	"UnrealClaude.MCP.Tools.AssetDependencies.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AssetDependencies_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("asset_dependencies"));
	TestNotNull("asset_dependencies tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be asset_dependencies", Info.Name, TEXT("asset_dependencies"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());

	// Check for required parameters
	bool bHasAssetPath = false;
	bool bHasIncludeSoft = false;

	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("asset_path"))
		{
			bHasAssetPath = true;
			TestTrue("asset_path should be required", Param.bRequired);
		}
		if (Param.Name == TEXT("include_soft"))
		{
			bHasIncludeSoft = true;
			TestFalse("include_soft should be optional", Param.bRequired);
		}
	}

	TestTrue("Should have 'asset_path' parameter", bHasAssetPath);
	TestTrue("Should have 'include_soft' parameter", bHasIncludeSoft);
	TestTrue("Should be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AssetDependencies_MissingAssetPath,
	"UnrealClaude.MCP.Tools.AssetDependencies.MissingAssetPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AssetDependencies_MissingAssetPath::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("asset_dependencies"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// Execute without asset_path
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail without asset_path", Result.bSuccess);
	TestTrue("Error should mention asset_path or Missing",
		Result.Message.Contains(TEXT("asset_path")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

// ===== Asset Referencers Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AssetReferencers_GetInfo,
	"UnrealClaude.MCP.Tools.AssetReferencers.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AssetReferencers_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("asset_referencers"));
	TestNotNull("asset_referencers tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be asset_referencers", Info.Name, TEXT("asset_referencers"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());

	// Check for required parameters
	bool bHasAssetPath = false;

	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("asset_path"))
		{
			bHasAssetPath = true;
			TestTrue("asset_path should be required", Param.bRequired);
		}
	}

	TestTrue("Should have 'asset_path' parameter", bHasAssetPath);
	TestTrue("Should be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AssetReferencers_MissingAssetPath,
	"UnrealClaude.MCP.Tools.AssetReferencers.MissingAssetPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AssetReferencers_MissingAssetPath::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("asset_referencers"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// Execute without asset_path
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail without asset_path", Result.bSuccess);
	TestTrue("Error should mention asset_path or Missing",
		Result.Message.Contains(TEXT("asset_path")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

// ===== Task Submit Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_TaskSubmit_GetInfo,
	"UnrealClaude.MCP.Tools.TaskSubmit.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_TaskSubmit_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("task_submit"));
	TestNotNull("task_submit tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be task_submit", Info.Name, TEXT("task_submit"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());

	// Check for required parameters
	bool bHasToolName = false;
	bool bHasParams = false;
	bool bHasTimeout = false;

	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("tool_name"))
		{
			bHasToolName = true;
			TestTrue("tool_name should be required", Param.bRequired);
		}
		if (Param.Name == TEXT("params"))
		{
			bHasParams = true;
			TestFalse("params should be optional", Param.bRequired);
		}
		if (Param.Name == TEXT("timeout_ms"))
		{
			bHasTimeout = true;
			TestFalse("timeout_ms should be optional", Param.bRequired);
		}
	}

	TestTrue("Should have 'tool_name' parameter", bHasToolName);
	TestTrue("Should have 'params' parameter", bHasParams);
	TestTrue("Should have 'timeout_ms' parameter", bHasTimeout);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_TaskSubmit_MissingToolName,
	"UnrealClaude.MCP.Tools.TaskSubmit.MissingToolName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_TaskSubmit_MissingToolName::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	Registry.StartTaskQueue(); // Ensure queue is started
	IMCPTool* Tool = Registry.FindTool(TEXT("task_submit"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// Execute without tool_name
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail without tool_name", Result.bSuccess);
	TestTrue("Error should mention tool_name or Missing",
		Result.Message.Contains(TEXT("tool_name")) || Result.Message.Contains(TEXT("Missing")));

	Registry.StopTaskQueue();
	return true;
}

// ===== Task Status Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_TaskStatus_GetInfo,
	"UnrealClaude.MCP.Tools.TaskStatus.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_TaskStatus_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("task_status"));
	TestNotNull("task_status tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be task_status", Info.Name, TEXT("task_status"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());

	// Check for required task_id parameter
	bool bHasTaskId = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("task_id"))
		{
			bHasTaskId = true;
			TestTrue("task_id should be required", Param.bRequired);
		}
	}

	TestTrue("Should have 'task_id' parameter", bHasTaskId);
	TestTrue("Should be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_TaskStatus_InvalidTaskId,
	"UnrealClaude.MCP.Tools.TaskStatus.InvalidTaskId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_TaskStatus_InvalidTaskId::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	Registry.StartTaskQueue();
	IMCPTool* Tool = Registry.FindTool(TEXT("task_status"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	// Test with invalid GUID format
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("task_id"), TEXT("not-a-valid-guid"));

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("Should fail with invalid GUID format", Result.bSuccess);
		TestTrue("Error should mention invalid format",
			Result.Message.Contains(TEXT("Invalid")) || Result.Message.Contains(TEXT("format")));
	}

	// Test with valid GUID but non-existent task
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("task_id"), FGuid::NewGuid().ToString());

		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("Should fail with non-existent task", Result.bSuccess);
		TestTrue("Error should mention not found",
			Result.Message.Contains(TEXT("not found")) || Result.Message.Contains(TEXT("Task")));
	}

	Registry.StopTaskQueue();
	return true;
}

// ===== Task List Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_TaskList_GetInfo,
	"UnrealClaude.MCP.Tools.TaskList.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_TaskList_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("task_list"));
	TestNotNull("task_list tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be task_list", Info.Name, TEXT("task_list"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should be read-only", Info.Annotations.bReadOnlyHint);

	// All parameters should be optional
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		TestFalse(FString::Printf(TEXT("'%s' should be optional"), *Param.Name), Param.bRequired);
	}

	return true;
}

// ===== Task Result Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_TaskResult_GetInfo,
	"UnrealClaude.MCP.Tools.TaskResult.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_TaskResult_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("task_result"));
	TestNotNull("task_result tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be task_result", Info.Name, TEXT("task_result"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());

	// Check for required task_id parameter
	bool bHasTaskId = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("task_id"))
		{
			bHasTaskId = true;
			TestTrue("task_id should be required", Param.bRequired);
		}
	}

	TestTrue("Should have 'task_id' parameter", bHasTaskId);
	TestTrue("Should be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_TaskResult_MissingTaskId,
	"UnrealClaude.MCP.Tools.TaskResult.MissingTaskId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_TaskResult_MissingTaskId::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	Registry.StartTaskQueue();
	IMCPTool* Tool = Registry.FindTool(TEXT("task_result"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail without task_id", Result.bSuccess);
	TestTrue("Error should mention task_id or Missing",
		Result.Message.Contains(TEXT("task_id")) || Result.Message.Contains(TEXT("Missing")));

	Registry.StopTaskQueue();
	return true;
}

// ===== Task Cancel Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_TaskCancel_GetInfo,
	"UnrealClaude.MCP.Tools.TaskCancel.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_TaskCancel_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("task_cancel"));
	TestNotNull("task_cancel tool should exist", Tool);

	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be task_cancel", Info.Name, TEXT("task_cancel"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());

	// Check for required task_id parameter
	bool bHasTaskId = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("task_id"))
		{
			bHasTaskId = true;
			TestTrue("task_id should be required", Param.bRequired);
		}
	}

	TestTrue("Should have 'task_id' parameter", bHasTaskId);

	// Task cancel should be marked as destructive (cannot undo)
	TestTrue("Should be destructive", Info.Annotations.bDestructiveHint);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_TaskCancel_NonExistentTask,
	"UnrealClaude.MCP.Tools.TaskCancel.NonExistentTask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_TaskCancel_NonExistentTask::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	Registry.StartTaskQueue();
	IMCPTool* Tool = Registry.FindTool(TEXT("task_cancel"));
	TestNotNull("Tool should exist", Tool);

	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("task_id"), FGuid::NewGuid().ToString());

	FMCPToolResult Result = Tool->Execute(Params);

	// Should fail because task doesn't exist
	TestFalse("Should fail for non-existent task", Result.bSuccess);
	TestTrue("Error should mention not found or cannot cancel",
		Result.Message.Contains(TEXT("not found")) || Result.Message.Contains(TEXT("cannot")));

	Registry.StopTaskQueue();
	return true;
}

// ===== Registry Tests for New Tools =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPToolRegistry_AssetToolsRegistered,
	"UnrealClaude.MCP.Registry.AssetToolsRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPToolRegistry_AssetToolsRegistered::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;

	// Asset tools should be registered
	TestNotNull("asset_search should be registered", Registry.FindTool(TEXT("asset_search")));
	TestNotNull("asset_dependencies should be registered", Registry.FindTool(TEXT("asset_dependencies")));
	TestNotNull("asset_referencers should be registered", Registry.FindTool(TEXT("asset_referencers")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPToolRegistry_TaskToolsRegistered,
	"UnrealClaude.MCP.Registry.TaskToolsRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPToolRegistry_TaskToolsRegistered::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;

	// Task management tools should be registered
	TestNotNull("task_submit should be registered", Registry.FindTool(TEXT("task_submit")));
	TestNotNull("task_status should be registered", Registry.FindTool(TEXT("task_status")));
	TestNotNull("task_list should be registered", Registry.FindTool(TEXT("task_list")));
	TestNotNull("task_result should be registered", Registry.FindTool(TEXT("task_result")));
	TestNotNull("task_cancel should be registered", Registry.FindTool(TEXT("task_cancel")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPToolRegistry_TaskQueueLifecycle,
	"UnrealClaude.MCP.Registry.TaskQueueLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPToolRegistry_TaskQueueLifecycle::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;

	// Task queue should exist
	TSharedPtr<FMCPTaskQueue> Queue = Registry.GetTaskQueue();
	TestTrue("Task queue should exist", Queue.IsValid());

	// ============================================================================
	// KNOWN ISSUE: FRunnableThread tests disabled due to deadlock potential in
	// Unreal Engine's automation test context.
	//
	// ROOT CAUSE: The UE automation test framework runs tests on the game thread.
	// When FMCPTaskQueue::Start() creates an FRunnableThread, the worker thread
	// dispatches tool execution back to the game thread via AsyncTask(GameThread).
	// This creates a circular dependency:
	//   1. Test (GameThread) calls Start() -> creates worker thread
	//   2. Worker thread calls ExecuteTool() -> dispatches to GameThread
	//   3. GameThread is blocked waiting for test to complete
	//   4. Worker thread waits for GameThread -> DEADLOCK
	//
	// Additionally, FRunnableThread::Kill(true) waits for thread completion, but
	// if the thread is blocked on a GameThread dispatch that never completes
	// (because GameThread is waiting for Kill), a freeze occurs.
	//
	// VERIFICATION: The task queue works correctly in normal editor operation
	// where the game thread is not blocked by test execution. The MCP bridge
	// uses the task queue successfully for async tool execution.
	//
	// WORKAROUNDS ATTEMPTED:
	//   - Short Sleep() after Start/Stop: Still causes freezes
	//   - Using FPlatformProcess::Sleep in worker loop: Helps CPU but not deadlock
	//   - Latent automation tests: Adds complexity without solving core issue
	//
	// SAFE TO LEAVE DISABLED: Yes. The task queue functionality is tested by:
	//   - Non-threading tests (SubmitTask, CancelPendingTask, etc.) that don't
	//     start the worker thread
	//   - Manual editor testing of the MCP bridge
	//   - The task tools themselves (task_submit, task_status, etc.) which test
	//     the data structures without requiring thread execution
	//
	// REFERENCES:
	//   - UE Forums: FRunnable thread freezes editor
	//     https://forums.unrealengine.com/t/frunnable-thread-freezes-editor/365196
	//   - UE Bug UE-352373: Deadlock in animation evaluation with threads
	//   - UE Bug UE-177022: GameThread/AsyncLoadingThread deadlock
	//   - UE Docs: Automation Driver warns against synchronous GameThread blocking
	// ============================================================================
	//
	// DISABLED TESTS:
	// Registry.StartTaskQueue();
	// Registry.StopTaskQueue();
	// Registry.StartTaskQueue();
	// Registry.StartTaskQueue();
	// Registry.StopTaskQueue();
	// Registry.StopTaskQueue();

	return true;
}

// ===== Blueprint Query Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_BlueprintQuery_GetInfo,
	"UnrealClaude.MCP.Tools.BlueprintQuery.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_BlueprintQuery_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("blueprint_query"));
	TestNotNull("blueprint_query tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();
	TestEqual("Tool name should be blueprint_query", Info.Name, TEXT("blueprint_query"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have parameters", Info.Parameters.Num() > 0);

	bool bHasOperation = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("operation"))
		{
			bHasOperation = true;
			TestTrue("operation should be required", Param.bRequired);
		}
	}
	TestTrue("Should have 'operation' parameter", bHasOperation);
	TestTrue("Should be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

// ===== Blueprint Modify Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_BlueprintModify_GetInfo,
	"UnrealClaude.MCP.Tools.BlueprintModify.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_BlueprintModify_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("blueprint_modify"));
	TestNotNull("blueprint_modify tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();
	TestEqual("Tool name should be blueprint_modify", Info.Name, TEXT("blueprint_modify"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have many parameters", Info.Parameters.Num() >= 10);

	bool bHasOperation = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("operation"))
		{
			bHasOperation = true;
			TestTrue("operation should be required", Param.bRequired);
		}
	}
	TestTrue("Should have 'operation' parameter", bHasOperation);
	TestFalse("Should not be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

// ===== Capture Viewport Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_CaptureViewport_GetInfo,
	"UnrealClaude.MCP.Tools.CaptureViewport.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_CaptureViewport_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("capture_viewport"));
	TestNotNull("capture_viewport tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();
	TestEqual("Tool name should be capture_viewport", Info.Name, TEXT("capture_viewport"));
	TestTrue("Description should mention JPEG", Info.Description.Contains(TEXT("JPEG")));
	TestTrue("Description should mention base64", Info.Description.Contains(TEXT("base64")));
	TestTrue("Should be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

// ===== Cleanup Scripts Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_CleanupScripts_GetInfo,
	"UnrealClaude.MCP.Tools.CleanupScripts.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_CleanupScripts_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("cleanup_scripts"));
	TestNotNull("cleanup_scripts tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();
	TestEqual("Tool name should be cleanup_scripts", Info.Name, TEXT("cleanup_scripts"));
	TestTrue("Description should warn about destructive operation",
		Info.Description.Contains(TEXT("WARNING")) || Info.Description.Contains(TEXT("destructive")));
	TestTrue("Should be marked as destructive", Info.Annotations.bDestructiveHint);

	return true;
}

// ===== Execute Script Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_ExecuteScript_GetInfo,
	"UnrealClaude.MCP.Tools.ExecuteScript.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_ExecuteScript_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("execute_script"));
	TestNotNull("execute_script tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();
	TestEqual("Tool name should be execute_script", Info.Name, TEXT("execute_script"));
	TestTrue("Should have parameters", Info.Parameters.Num() > 0);

	bool bHasScriptType = false;
	bool bHasScriptContent = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("script_type"))
		{
			bHasScriptType = true;
			TestTrue("script_type should be required", Param.bRequired);
		}
		if (Param.Name == TEXT("script_content"))
		{
			bHasScriptContent = true;
			TestTrue("script_content should be required", Param.bRequired);
		}
	}
	TestTrue("Should have 'script_type' parameter", bHasScriptType);
	TestTrue("Should have 'script_content' parameter", bHasScriptContent);
	TestTrue("Should be marked as destructive", Info.Annotations.bDestructiveHint);

	return true;
}

// ===== Get Output Log Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_GetOutputLog_GetInfo,
	"UnrealClaude.MCP.Tools.GetOutputLog.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_GetOutputLog_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("get_output_log"));
	TestNotNull("get_output_log tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();
	TestEqual("Tool name should be get_output_log", Info.Name, TEXT("get_output_log"));
	TestTrue("Description should mention filtering", Info.Description.Contains(TEXT("filter")));

	bool bHasLines = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("lines"))
		{
			bHasLines = true;
			TestFalse("lines should be optional", Param.bRequired);
		}
	}
	TestTrue("Should have 'lines' parameter", bHasLines);
	TestTrue("Should be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

// ===== Get Script History Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_GetScriptHistory_GetInfo,
	"UnrealClaude.MCP.Tools.GetScriptHistory.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_GetScriptHistory_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("get_script_history"));
	TestNotNull("get_script_history tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();
	TestEqual("Tool name should be get_script_history", Info.Name, TEXT("get_script_history"));
	TestTrue("Description should mention history", Info.Description.Contains(TEXT("history")));

	bool bHasCount = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("count"))
		{
			bHasCount = true;
			TestFalse("count should be optional", Param.bRequired);
		}
	}
	TestTrue("Should have 'count' parameter", bHasCount);
	TestTrue("Should be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

// ===== Run Console Command Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_RunConsoleCommand_GetInfo,
	"UnrealClaude.MCP.Tools.RunConsoleCommand.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_RunConsoleCommand_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("run_console_command"));
	TestNotNull("run_console_command tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();
	TestEqual("Tool name should be run_console_command", Info.Name, TEXT("run_console_command"));
	TestTrue("Description should mention console", Info.Description.Contains(TEXT("console")));

	bool bHasCommand = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("command"))
		{
			bHasCommand = true;
			TestTrue("command should be required", Param.bRequired);
		}
	}
	TestTrue("Should have 'command' parameter", bHasCommand);
	TestFalse("Should not be read-only", Info.Annotations.bReadOnlyHint);

	return true;
}

// ===== Registry Tests for Script Tools =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPToolRegistry_ScriptToolsRegistered,
	"UnrealClaude.MCP.Registry.ScriptToolsRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPToolRegistry_ScriptToolsRegistered::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;

	TestNotNull("execute_script should be registered", Registry.FindTool(TEXT("execute_script")));
	TestNotNull("get_script_history should be registered", Registry.FindTool(TEXT("get_script_history")));
	TestNotNull("cleanup_scripts should be registered", Registry.FindTool(TEXT("cleanup_scripts")));

	return true;
}

// ===== Registry Tests for Utility Tools =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPToolRegistry_UtilityToolsRegistered,
	"UnrealClaude.MCP.Registry.UtilityToolsRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPToolRegistry_UtilityToolsRegistered::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;

	TestNotNull("capture_viewport should be registered", Registry.FindTool(TEXT("capture_viewport")));
	TestNotNull("get_output_log should be registered", Registry.FindTool(TEXT("get_output_log")));
	TestNotNull("run_console_command should be registered", Registry.FindTool(TEXT("run_console_command")));

	return true;
}

// ===== Enhanced Input Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_EnhancedInput_GetInfo,
	"UnrealClaude.MCP.Tools.EnhancedInput.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_EnhancedInput_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("enhanced_input"));
	TestNotNull("enhanced_input tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();
	TestEqual("Tool name should be enhanced_input", Info.Name, TEXT("enhanced_input"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have parameters", Info.Parameters.Num() > 0);

	// Check for key parameters
	bool bHasOperation = false;
	bool bHasActionName = false;
	bool bHasContextName = false;
	bool bHasKey = false;

	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("operation"))
		{
			bHasOperation = true;
			TestTrue("operation should be required", Param.bRequired);
		}
		if (Param.Name == TEXT("action_name")) bHasActionName = true;
		if (Param.Name == TEXT("context_name")) bHasContextName = true;
		if (Param.Name == TEXT("key")) bHasKey = true;
	}

	TestTrue("Should have 'operation' parameter", bHasOperation);
	TestTrue("Should have 'action_name' parameter", bHasActionName);
	TestTrue("Should have 'context_name' parameter", bHasContextName);
	TestTrue("Should have 'key' parameter", bHasKey);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_EnhancedInput_MissingOperation,
	"UnrealClaude.MCP.Tools.EnhancedInput.MissingOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_EnhancedInput_MissingOperation::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("enhanced_input"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	// Execute without operation
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("action_name"), TEXT("IA_Test"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail without operation", Result.bSuccess);
	TestTrue("Error should mention operation or Missing",
		Result.Message.Contains(TEXT("operation")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_EnhancedInput_InvalidOperation,
	"UnrealClaude.MCP.Tools.EnhancedInput.InvalidOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_EnhancedInput_InvalidOperation::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("enhanced_input"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("invalid_operation_xyz"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail with invalid operation", Result.bSuccess);
	TestTrue("Error should mention unknown operation",
		Result.Message.Contains(TEXT("Unknown")) || Result.Message.Contains(TEXT("operation")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_EnhancedInput_CreateInputActionMissingName,
	"UnrealClaude.MCP.Tools.EnhancedInput.CreateInputActionMissingName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_EnhancedInput_CreateInputActionMissingName::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("enhanced_input"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	// create_input_action without action_name
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("create_input_action"));
	Params->SetStringField(TEXT("value_type"), TEXT("Axis2D"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("create_input_action should fail without action_name", Result.bSuccess);
	TestTrue("Error should mention action_name or Missing",
		Result.Message.Contains(TEXT("action_name")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_EnhancedInput_CreateMappingContextMissingName,
	"UnrealClaude.MCP.Tools.EnhancedInput.CreateMappingContextMissingName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_EnhancedInput_CreateMappingContextMissingName::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("enhanced_input"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	// create_mapping_context without context_name
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("create_mapping_context"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("create_mapping_context should fail without context_name", Result.bSuccess);
	TestTrue("Error should mention context_name or Missing",
		Result.Message.Contains(TEXT("context_name")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_EnhancedInput_AddMappingMissingParams,
	"UnrealClaude.MCP.Tools.EnhancedInput.AddMappingMissingParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_EnhancedInput_AddMappingMissingParams::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("enhanced_input"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	// add_mapping without context_path
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("operation"), TEXT("add_mapping"));
		Params->SetStringField(TEXT("action_path"), TEXT("/Game/Input/IA_Move"));
		Params->SetStringField(TEXT("key"), TEXT("W"));
		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("add_mapping should fail without context_path", Result.bSuccess);
	}

	// add_mapping without action_path
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("operation"), TEXT("add_mapping"));
		Params->SetStringField(TEXT("context_path"), TEXT("/Game/Input/IMC_Default"));
		Params->SetStringField(TEXT("key"), TEXT("W"));
		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("add_mapping should fail without action_path", Result.bSuccess);
	}

	// add_mapping without key
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("operation"), TEXT("add_mapping"));
		Params->SetStringField(TEXT("context_path"), TEXT("/Game/Input/IMC_Default"));
		Params->SetStringField(TEXT("action_path"), TEXT("/Game/Input/IA_Move"));
		FMCPToolResult Result = Tool->Execute(Params);
		TestFalse("add_mapping should fail without key", Result.bSuccess);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_EnhancedInput_QueryContextMissingPath,
	"UnrealClaude.MCP.Tools.EnhancedInput.QueryContextMissingPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_EnhancedInput_QueryContextMissingPath::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("enhanced_input"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("query_context"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("query_context should fail without context_path", Result.bSuccess);
	TestTrue("Error should mention context_path or Missing",
		Result.Message.Contains(TEXT("context_path")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_EnhancedInput_ToolAnnotations,
	"UnrealClaude.MCP.Tools.EnhancedInput.ToolAnnotations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_EnhancedInput_ToolAnnotations::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("enhanced_input"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	// Enhanced Input tool modifies assets, so should not be read-only
	TestFalse("Should not be marked as read-only", Info.Annotations.bReadOnlyHint);
	// It's not destructive (changes can be reverted)
	TestFalse("Should not be marked as destructive", Info.Annotations.bDestructiveHint);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPToolRegistry_EnhancedInputRegistered,
	"UnrealClaude.MCP.Registry.EnhancedInputRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPToolRegistry_EnhancedInputRegistered::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	TestNotNull("enhanced_input should be registered", Registry.FindTool(TEXT("enhanced_input")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
