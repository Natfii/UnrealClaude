// Copyright Natali Caggiano. All Rights Reserved.

/**
 * Unit tests for MCP bearer-token auth helpers and hidden-tool policy.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UnrealClaudeConstants.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealClaude_HiddenTools_IsHidden,
	"UnrealClaude.MCP.Auth.HiddenTools.IsHidden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FUnrealClaude_HiddenTools_IsHidden::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("task_submit is hidden"), UnrealClaudeConstants::HiddenTools::IsHidden(TEXT("task_submit")));
	TestTrue(TEXT("execute_script is hidden"), UnrealClaudeConstants::HiddenTools::IsHidden(TEXT("execute_script")));
	TestTrue(TEXT("run_console_command is hidden"), UnrealClaudeConstants::HiddenTools::IsHidden(TEXT("run_console_command")));
	TestFalse(TEXT("spawn_actor is not hidden"), UnrealClaudeConstants::HiddenTools::IsHidden(TEXT("spawn_actor")));
	TestFalse(TEXT("asset_search is not hidden"), UnrealClaudeConstants::HiddenTools::IsHidden(TEXT("asset_search")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealClaude_HiddenTools_BlockedFromTaskSubmit,
	"UnrealClaude.MCP.Auth.HiddenTools.BlockedFromTaskSubmit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FUnrealClaude_HiddenTools_BlockedFromTaskSubmit::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("execute_script blocked from task_submit"),
		UnrealClaudeConstants::HiddenTools::IsBlockedFromTaskSubmit(TEXT("execute_script")));
	TestTrue(TEXT("run_console_command blocked from task_submit"),
		UnrealClaudeConstants::HiddenTools::IsBlockedFromTaskSubmit(TEXT("run_console_command")));
	TestFalse(TEXT("task_status is not in the task_submit blocklist"),
		UnrealClaudeConstants::HiddenTools::IsBlockedFromTaskSubmit(TEXT("task_status")));
	TestFalse(TEXT("spawn_actor is not blocked"),
		UnrealClaudeConstants::HiddenTools::IsBlockedFromTaskSubmit(TEXT("spawn_actor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealClaude_Process_OutputCapPositive,
	"UnrealClaude.MCP.Auth.Process.OutputCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FUnrealClaude_Process_OutputCapPositive::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("MaxProcessOutputBytes is positive and at least 1 MiB"),
		UnrealClaudeConstants::Process::MaxProcessOutputBytes >= 1024 * 1024);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
