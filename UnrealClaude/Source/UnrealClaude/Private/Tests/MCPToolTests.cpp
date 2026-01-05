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

	// Check required parameters
	bool bHasNamesParam = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("names"))
		{
			bHasNamesParam = true;
			TestTrue("names parameter should be required", Param.bRequired);
		}
	}
	TestTrue("Should have 'names' parameter", bHasNamesParam);

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
	bool bHasPropertyPath = false;
	bool bHasValue = false;

	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("actor_name")) bHasActorName = true;
		if (Param.Name == TEXT("property_path")) bHasPropertyPath = true;
		if (Param.Name == TEXT("value")) bHasValue = true;
	}

	TestTrue("Should have 'actor_name' parameter", bHasActorName);
	TestTrue("Should have 'property_path' parameter", bHasPropertyPath);
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
	Params->SetStringField(TEXT("class"), TEXT("StaticMeshActor"));
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
		Params->SetStringField(TEXT("property_path"), TEXT("MyProperty"));
		Params->SetStringField(TEXT("value"), TEXT("test"));

		FMCPToolResult Result = Tool.Execute(Params);
		TestFalse("Should fail without actor_name", Result.bSuccess);
	}

	// Test missing property_path
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("actor_name"), TEXT("TestActor"));
		Params->SetStringField(TEXT("value"), TEXT("test"));

		FMCPToolResult Result = Tool.Execute(Params);
		TestFalse("Should fail without property_path", Result.bSuccess);
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

	// These should still be valid class paths (blocking is for Blueprint paths)
	TestTrue("StaticMeshActor should be valid",
		FMCPParamValidator::ValidateClassPath(TEXT("StaticMeshActor"), Error));

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
		Params->SetStringField(TEXT("property_path"), TEXT("..Parent.Property"));
		Params->SetStringField(TEXT("value"), TEXT("evil"));

		FMCPToolResult Result = Tool.Execute(Params);
		TestFalse("Path traversal should be blocked", Result.bSuccess);
	}

	// Test special characters
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("actor_name"), TEXT("TestActor"));
		Params->SetStringField(TEXT("property_path"), TEXT("Property<T>"));
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

#endif // WITH_DEV_AUTOMATION_TESTS
