// Copyright Natali Caggiano. All Rights Reserved.

/**
 * Unit tests for Animation Blueprint Bulk Operations (setup_transition_conditions)
 * Tests for the new bulk transition condition setup functionality
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MCP/Tools/MCPTool_AnimBlueprintModify.h"
#include "Dom/JsonObject.h"

#if WITH_DEV_AUTOMATION_TESTS

// ===== Tool Info Tests for Bulk Operations =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_BulkOps_HasRulesParam,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.BulkOps.HasRulesParam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_BulkOps_HasRulesParam::RunTest(const FString& Parameters)
{
	FMCPTool_AnimBlueprintModify Tool;
	FMCPToolInfo Info = Tool.GetInfo();

	// Check that rules parameter exists for bulk operations
	bool bHasRulesParam = false;
	for (const FMCPToolParameter& Param : Info.Parameters)
	{
		if (Param.Name == TEXT("rules"))
		{
			bHasRulesParam = true;
			TestFalse("rules parameter should be optional", Param.bRequired);
			TestEqual("rules parameter should be array type", Param.Type, TEXT("array"));
		}
	}

	TestTrue("Should have 'rules' parameter for bulk operations", bHasRulesParam);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_BulkOps_DescriptionMentionsBulkOps,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.BulkOps.DescriptionMentionsBulkOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_BulkOps_DescriptionMentionsBulkOps::RunTest(const FString& Parameters)
{
	FMCPTool_AnimBlueprintModify Tool;
	FMCPToolInfo Info = Tool.GetInfo();

	// Verify bulk operations are documented
	TestTrue("Description mentions setup_transition_conditions",
		Info.Description.Contains(TEXT("setup_transition_conditions")));
	TestTrue("Description has Bulk Operations section",
		Info.Description.Contains(TEXT("Bulk Operations")));

	return true;
}

// ===== Parameter Validation Tests for setup_transition_conditions =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_BulkOps_RequiresStateMachine,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.BulkOps.RequiresStateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_BulkOps_RequiresStateMachine::RunTest(const FString& Parameters)
{
	FMCPTool_AnimBlueprintModify Tool;

	// setup_transition_conditions without state_machine
	// Note: Blueprint loading happens first, so without a real test asset
	// we can only verify the operation fails. With a real ABP, error would mention state_machine.
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Test/ABP_Test"));
	Params->SetStringField(TEXT("operation"), TEXT("setup_transition_conditions"));
	// Missing state_machine

	FMCPToolResult Result = Tool.Execute(Params);
	TestFalse("Should fail without state_machine (or blueprint)", Result.bSuccess);
	// Either fails on blueprint load or state_machine validation
	TestTrue("Should have error message", !Result.Message.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_BulkOps_RequiresRulesArray,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.BulkOps.RequiresRulesArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_BulkOps_RequiresRulesArray::RunTest(const FString& Parameters)
{
	FMCPTool_AnimBlueprintModify Tool;

	// setup_transition_conditions without rules
	// Note: Blueprint loading happens first, so without a real test asset
	// we can only verify the operation fails. With a real ABP, error would mention rules.
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Test/ABP_Test"));
	Params->SetStringField(TEXT("operation"), TEXT("setup_transition_conditions"));
	Params->SetStringField(TEXT("state_machine"), TEXT("Locomotion"));
	// Missing rules array

	FMCPToolResult Result = Tool.Execute(Params);
	TestFalse("Should fail without rules array (or blueprint)", Result.bSuccess);
	// Either fails on blueprint load or rules validation
	TestTrue("Should have error message", !Result.Message.IsEmpty());

	return true;
}

// ===== Batch Operations Tests for add_comparison_chain =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_Batch_SupportsComparisonChain,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.Batch.SupportsComparisonChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_Batch_SupportsComparisonChain::RunTest(const FString& Parameters)
{
	FMCPTool_AnimBlueprintModify Tool;
	FMCPToolInfo Info = Tool.GetInfo();

	// Verify batch operation is mentioned in description
	TestTrue("Description mentions batch operation", Info.Description.Contains(TEXT("batch")));
	TestTrue("Description mentions add_comparison_chain", Info.Description.Contains(TEXT("add_comparison_chain")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_Batch_RequiresOperationsArray,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.Batch.RequiresOperationsArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_Batch_RequiresOperationsArray::RunTest(const FString& Parameters)
{
	FMCPTool_AnimBlueprintModify Tool;

	// batch operation without operations array
	// Note: Blueprint loading happens first, so without a real test asset
	// we can only verify the operation fails. With a real ABP, error would mention operations.
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/Test/ABP_Test"));
	Params->SetStringField(TEXT("operation"), TEXT("batch"));
	// Missing operations array

	FMCPToolResult Result = Tool.Execute(Params);
	TestFalse("Should fail without operations array (or blueprint)", Result.bSuccess);
	// Either fails on blueprint load or operations validation
	TestTrue("Should have error message", !Result.Message.IsEmpty());

	return true;
}

// ===== Security Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_BulkOps_Security_BlocksEnginePaths,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.BulkOps.Security.BlocksEnginePaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_BulkOps_Security_BlocksEnginePaths::RunTest(const FString& Parameters)
{
	FMCPTool_AnimBlueprintModify Tool;

	// Attempt to access engine path with bulk operation
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("blueprint_path"), TEXT("/Engine/AnimBlueprints/ABP_Default"));
	Params->SetStringField(TEXT("operation"), TEXT("setup_transition_conditions"));
	Params->SetStringField(TEXT("state_machine"), TEXT("Locomotion"));

	TArray<TSharedPtr<FJsonValue>> RulesArray;
	Params->SetArrayField(TEXT("rules"), RulesArray);

	FMCPToolResult Result = Tool.Execute(Params);
	TestFalse("Should block engine paths", Result.bSuccess);
	TestTrue("Error should mention engine paths blocked",
		Result.Message.Contains(TEXT("Engine")) || Result.Message.Contains(TEXT("blocked")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPTool_AnimBlueprintModify_BulkOps_Security_BlocksPathTraversal,
	"UnrealClaude.MCP.Tools.AnimBlueprintModify.BulkOps.Security.BlocksPathTraversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPTool_AnimBlueprintModify_BulkOps_Security_BlocksPathTraversal::RunTest(const FString& Parameters)
{
	FMCPTool_AnimBlueprintModify Tool;

	// Attempt path traversal with bulk operation
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("blueprint_path"), TEXT("/Game/../../../etc/passwd"));
	Params->SetStringField(TEXT("operation"), TEXT("setup_transition_conditions"));
	Params->SetStringField(TEXT("state_machine"), TEXT("Locomotion"));

	TArray<TSharedPtr<FJsonValue>> RulesArray;
	Params->SetArrayField(TEXT("rules"), RulesArray);

	FMCPToolResult Result = Tool.Execute(Params);
	TestFalse("Should block path traversal", Result.bSuccess);
	TestTrue("Error should mention path traversal",
		Result.Message.Contains(TEXT("traversal")) || Result.Message.Contains(TEXT("../")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
