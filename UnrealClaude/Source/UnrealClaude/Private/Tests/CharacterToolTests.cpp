// Copyright Natali Caggiano. All Rights Reserved.

/**
 * Unit tests for Character MCP Tools
 * Tests tool info, parameter validation, and error handling
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MCP/MCPToolRegistry.h"
#include "Dom/JsonObject.h"

#if WITH_DEV_AUTOMATION_TESTS

// ===== Character Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_Character_GetInfo,
	"UnrealClaude.MCP.Tools.Character.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_Character_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be character", Info.Name, TEXT("character"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have parameters", Info.Parameters.Num() > 0);

	bool bHasOperation = false;
	bool bHasCharacterName = false;

	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("operation"))
		{
			bHasOperation = true;
			TestTrue("operation parameter should be required", Param.bRequired);
		}
		if (Param.Name == TEXT("character_name")) bHasCharacterName = true;
	}

	TestTrue("Should have 'operation' parameter", bHasOperation);
	TestTrue("Should have 'character_name' parameter", bHasCharacterName);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_Character_MissingOperation,
	"UnrealClaude.MCP.Tools.Character.MissingOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_Character_MissingOperation::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail without operation", Result.bSuccess);
	TestTrue("Error should mention operation", Result.Message.Contains(TEXT("operation")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_Character_InvalidOperation,
	"UnrealClaude.MCP.Tools.Character.InvalidOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_Character_InvalidOperation::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("invalid_op"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail with invalid operation", Result.bSuccess);
	TestTrue("Error should mention unknown operation",
		Result.Message.Contains(TEXT("Unknown")) || Result.Message.Contains(TEXT("invalid")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_Character_GetCharacterInfoMissingName,
	"UnrealClaude.MCP.Tools.Character.GetCharacterInfoMissingName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_Character_GetCharacterInfoMissingName::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("get_character_info"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("get_character_info should fail without character_name", Result.bSuccess);
	TestTrue("Error should mention character_name",
		Result.Message.Contains(TEXT("character_name")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_Character_GetMovementParamsMissingName,
	"UnrealClaude.MCP.Tools.Character.GetMovementParamsMissingName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_Character_GetMovementParamsMissingName::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("get_movement_params"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("get_movement_params should fail without character_name", Result.bSuccess);
	TestTrue("Error should mention character_name",
		Result.Message.Contains(TEXT("character_name")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_Character_SetMovementParamsMissingName,
	"UnrealClaude.MCP.Tools.Character.SetMovementParamsMissingName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_Character_SetMovementParamsMissingName::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("set_movement_params"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("set_movement_params should fail without character_name", Result.bSuccess);
	TestTrue("Error should mention character_name",
		Result.Message.Contains(TEXT("character_name")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_Character_ToolAnnotations,
	"UnrealClaude.MCP.Tools.Character.ToolAnnotations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_Character_ToolAnnotations::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestFalse("Should not be marked as read-only", Info.Annotations.bReadOnlyHint);
	TestFalse("Should not be marked as destructive", Info.Annotations.bDestructiveHint);

	return true;
}

// ===== CharacterData Tool Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_CharacterData_GetInfo,
	"UnrealClaude.MCP.Tools.CharacterData.GetInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_CharacterData_GetInfo::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character_data"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestEqual("Tool name should be character_data", Info.Name, TEXT("character_data"));
	TestTrue("Description should not be empty", !Info.Description.IsEmpty());
	TestTrue("Should have parameters", Info.Parameters.Num() > 0);

	bool bHasOperation = false;
	bool bHasAssetName = false;
	bool bHasPackagePath = false;

	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("operation"))
		{
			bHasOperation = true;
			TestTrue("operation parameter should be required", Param.bRequired);
		}
		if (Param.Name == TEXT("asset_name")) bHasAssetName = true;
		if (Param.Name == TEXT("package_path")) bHasPackagePath = true;
	}

	TestTrue("Should have 'operation' parameter", bHasOperation);
	TestTrue("Should have 'asset_name' parameter", bHasAssetName);
	TestTrue("Should have 'package_path' parameter", bHasPackagePath);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_CharacterData_MissingOperation,
	"UnrealClaude.MCP.Tools.CharacterData.MissingOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_CharacterData_MissingOperation::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character_data"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail without operation", Result.bSuccess);
	TestTrue("Error should mention operation", Result.Message.Contains(TEXT("operation")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_CharacterData_InvalidOperation,
	"UnrealClaude.MCP.Tools.CharacterData.InvalidOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_CharacterData_InvalidOperation::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character_data"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("invalid_operation"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("Should fail with invalid operation", Result.bSuccess);
	TestTrue("Error should mention unknown operation",
		Result.Message.Contains(TEXT("Unknown")) || Result.Message.Contains(TEXT("invalid")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_CharacterData_CreateMissingAssetName,
	"UnrealClaude.MCP.Tools.CharacterData.CreateMissingAssetName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_CharacterData_CreateMissingAssetName::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character_data"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("create_character_data"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("create_character_data should fail without asset_name", Result.bSuccess);
	TestTrue("Error should mention asset_name",
		Result.Message.Contains(TEXT("asset_name")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_CharacterData_CreateStatsTableMissingName,
	"UnrealClaude.MCP.Tools.CharacterData.CreateStatsTableMissingName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_CharacterData_CreateStatsTableMissingName::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character_data"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("create_stats_table"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("create_stats_table should fail without asset_name", Result.bSuccess);
	TestTrue("Error should mention asset_name",
		Result.Message.Contains(TEXT("asset_name")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_CharacterData_QueryMissingTablePath,
	"UnrealClaude.MCP.Tools.CharacterData.QueryMissingTablePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_CharacterData_QueryMissingTablePath::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character_data"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("query_stats_table"));
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("query_stats_table should fail without table_path", Result.bSuccess);
	TestTrue("Error should mention table_path",
		Result.Message.Contains(TEXT("table_path")) || Result.Message.Contains(TEXT("Missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_CharacterData_AddRowMissingParams,
	"UnrealClaude.MCP.Tools.CharacterData.AddRowMissingParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_CharacterData_AddRowMissingParams::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character_data"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("operation"), TEXT("add_stats_row"));
	// Missing both table_path and row_name
	FMCPToolResult Result = Tool->Execute(Params);

	TestFalse("add_stats_row should fail without required params", Result.bSuccess);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_CharacterData_ToolAnnotations,
	"UnrealClaude.MCP.Tools.CharacterData.ToolAnnotations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_CharacterData_ToolAnnotations::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	IMCPTool* Tool = Registry.FindTool(TEXT("character_data"));
	TestNotNull("Tool should exist", Tool);
	if (!Tool) return false;

	FMCPToolInfo Info = Tool->GetInfo();

	TestFalse("Should not be marked as read-only", Info.Annotations.bReadOnlyHint);
	TestFalse("Should not be marked as destructive", Info.Annotations.bDestructiveHint);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPToolRegistry_CharacterToolsRegistered,
	"UnrealClaude.MCP.Registry.CharacterToolsRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPToolRegistry_CharacterToolsRegistered::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	TestNotNull("character should be registered", Registry.FindTool(TEXT("character")));
	TestNotNull("character_data should be registered", Registry.FindTool(TEXT("character_data")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
