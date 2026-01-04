// Copyright Your Name. All Rights Reserved.

/**
 * Unit tests for MCPParamValidator
 * Tests security-critical validation logic for MCP tool parameters
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MCP/MCPParamValidator.h"

#if WITH_DEV_AUTOMATION_TESTS

// ===== Actor Name Validation Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_ValidateActorName_ValidNames,
	"UnrealClaude.MCP.ParamValidator.ActorName.ValidNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_ValidateActorName_ValidNames::RunTest(const FString& Parameters)
{
	FString Error;

	// Valid simple names
	TestTrue("Simple name should be valid", FMCPParamValidator::ValidateActorName(TEXT("MyActor"), Error));
	TestTrue("Name with numbers should be valid", FMCPParamValidator::ValidateActorName(TEXT("Actor123"), Error));
	TestTrue("Name with underscore should be valid", FMCPParamValidator::ValidateActorName(TEXT("My_Actor"), Error));
	TestTrue("Name with dash should be valid", FMCPParamValidator::ValidateActorName(TEXT("My-Actor"), Error));
	TestTrue("Name with spaces should be valid", FMCPParamValidator::ValidateActorName(TEXT("My Actor"), Error));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_ValidateActorName_InvalidNames,
	"UnrealClaude.MCP.ParamValidator.ActorName.InvalidNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_ValidateActorName_InvalidNames::RunTest(const FString& Parameters)
{
	FString Error;

	// Empty name
	TestFalse("Empty name should be invalid", FMCPParamValidator::ValidateActorName(TEXT(""), Error));
	TestTrue("Error message for empty name", Error.Contains(TEXT("empty")));

	// Names with dangerous characters
	TestFalse("Name with < should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor<Script>"), Error));
	TestFalse("Name with > should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor>test"), Error));
	TestFalse("Name with | should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor|test"), Error));
	TestFalse("Name with & should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor&test"), Error));
	TestFalse("Name with ; should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor;drop"), Error));
	TestFalse("Name with ` should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor`cmd`"), Error));
	TestFalse("Name with $ should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor$var"), Error));
	TestFalse("Name with ( should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor(test)"), Error));
	TestFalse("Name with { should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor{test}"), Error));
	TestFalse("Name with [ should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor[0]"), Error));
	TestFalse("Name with ! should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor!"), Error));
	TestFalse("Name with * should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor*"), Error));
	TestFalse("Name with ? should be invalid", FMCPParamValidator::ValidateActorName(TEXT("Actor?"), Error));
	TestFalse("Name with ~ should be invalid", FMCPParamValidator::ValidateActorName(TEXT("~Actor"), Error));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_ValidateActorName_LengthLimits,
	"UnrealClaude.MCP.ParamValidator.ActorName.LengthLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_ValidateActorName_LengthLimits::RunTest(const FString& Parameters)
{
	FString Error;

	// Create a string that exceeds max length (256 chars)
	FString LongName;
	for (int32 i = 0; i < 300; i++)
	{
		LongName += TEXT("A");
	}

	TestFalse("Name exceeding max length should be invalid", FMCPParamValidator::ValidateActorName(LongName, Error));
	TestTrue("Error should mention length", Error.Contains(TEXT("length")) || Error.Contains(TEXT("256")));

	return true;
}

// ===== Console Command Validation Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_ValidateConsoleCommand_BlockedCommands,
	"UnrealClaude.MCP.ParamValidator.ConsoleCommand.BlockedCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_ValidateConsoleCommand_BlockedCommands::RunTest(const FString& Parameters)
{
	FString Error;

	// Test blocked commands
	TestFalse("quit should be blocked", FMCPParamValidator::ValidateConsoleCommand(TEXT("quit"), Error));
	TestFalse("exit should be blocked", FMCPParamValidator::ValidateConsoleCommand(TEXT("exit"), Error));
	TestFalse("crash should be blocked", FMCPParamValidator::ValidateConsoleCommand(TEXT("crash"), Error));
	TestFalse("forcecrash should be blocked", FMCPParamValidator::ValidateConsoleCommand(TEXT("forcecrash"), Error));
	TestFalse("shutdown should be blocked", FMCPParamValidator::ValidateConsoleCommand(TEXT("shutdown"), Error));

	// Test case insensitivity
	TestFalse("QUIT should be blocked", FMCPParamValidator::ValidateConsoleCommand(TEXT("QUIT"), Error));
	TestFalse("Quit should be blocked", FMCPParamValidator::ValidateConsoleCommand(TEXT("Quit"), Error));

	// Test prefix matching
	TestFalse("gc.CollectGarbage should be blocked", FMCPParamValidator::ValidateConsoleCommand(TEXT("gc.CollectGarbage"), Error));
	TestFalse("r.ScreenPercentage should be blocked", FMCPParamValidator::ValidateConsoleCommand(TEXT("r.ScreenPercentage 50"), Error));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_ValidateConsoleCommand_ChainAttempts,
	"UnrealClaude.MCP.ParamValidator.ConsoleCommand.ChainAttempts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_ValidateConsoleCommand_ChainAttempts::RunTest(const FString& Parameters)
{
	FString Error;

	// Command chaining attempts
	TestFalse("Semicolon chaining should be blocked",
		FMCPParamValidator::ValidateConsoleCommand(TEXT("stat fps; quit"), Error));
	TestFalse("Pipe chaining should be blocked",
		FMCPParamValidator::ValidateConsoleCommand(TEXT("stat fps | quit"), Error));
	TestFalse("AND chaining should be blocked",
		FMCPParamValidator::ValidateConsoleCommand(TEXT("stat fps && quit"), Error));

	// Shell escape attempts
	TestFalse("Backtick escape should be blocked",
		FMCPParamValidator::ValidateConsoleCommand(TEXT("stat `quit`"), Error));
	TestFalse("$() escape should be blocked",
		FMCPParamValidator::ValidateConsoleCommand(TEXT("stat $(quit)"), Error));
	TestFalse("${} escape should be blocked",
		FMCPParamValidator::ValidateConsoleCommand(TEXT("stat ${quit}"), Error));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_ValidateConsoleCommand_ValidCommands,
	"UnrealClaude.MCP.ParamValidator.ConsoleCommand.ValidCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_ValidateConsoleCommand_ValidCommands::RunTest(const FString& Parameters)
{
	FString Error;

	// Valid safe commands
	TestTrue("stat fps should be valid", FMCPParamValidator::ValidateConsoleCommand(TEXT("stat fps"), Error));
	TestTrue("stat unit should be valid", FMCPParamValidator::ValidateConsoleCommand(TEXT("stat unit"), Error));
	TestTrue("showlog should be valid", FMCPParamValidator::ValidateConsoleCommand(TEXT("showlog"), Error));
	TestTrue("show collision should be valid", FMCPParamValidator::ValidateConsoleCommand(TEXT("show collision"), Error));

	return true;
}

// ===== Blueprint Path Validation Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_ValidateBlueprintPath_Security,
	"UnrealClaude.MCP.ParamValidator.BlueprintPath.Security",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_ValidateBlueprintPath_Security::RunTest(const FString& Parameters)
{
	FString Error;

	// Block engine Blueprints
	TestFalse("/Engine/ paths should be blocked",
		FMCPParamValidator::ValidateBlueprintPath(TEXT("/Engine/EditorBlueprintResources/StandardMacros"), Error));
	TestFalse("/Script/ paths should be blocked",
		FMCPParamValidator::ValidateBlueprintPath(TEXT("/Script/Engine.Actor"), Error));

	// Path traversal
	TestFalse("Path traversal should be blocked",
		FMCPParamValidator::ValidateBlueprintPath(TEXT("/Game/../Engine/SomeBP"), Error));

	// Valid game paths
	TestTrue("/Game/ paths should be valid",
		FMCPParamValidator::ValidateBlueprintPath(TEXT("/Game/Blueprints/BP_MyActor"), Error));

	return true;
}

// ===== Property Path Validation Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_ValidatePropertyPath_Format,
	"UnrealClaude.MCP.ParamValidator.PropertyPath.Format",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_ValidatePropertyPath_Format::RunTest(const FString& Parameters)
{
	FString Error;

	// Valid property paths
	TestTrue("Simple property should be valid",
		FMCPParamValidator::ValidatePropertyPath(TEXT("MyProperty"), Error));
	TestTrue("Nested property should be valid",
		FMCPParamValidator::ValidatePropertyPath(TEXT("Component.SubProperty"), Error));
	TestTrue("Property with underscore should be valid",
		FMCPParamValidator::ValidatePropertyPath(TEXT("My_Property"), Error));

	// Invalid property paths
	TestFalse("Empty path should be invalid",
		FMCPParamValidator::ValidatePropertyPath(TEXT(""), Error));
	TestFalse("Path traversal should be blocked",
		FMCPParamValidator::ValidatePropertyPath(TEXT("..Parent.Prop"), Error));
	TestFalse("Leading dot should be invalid",
		FMCPParamValidator::ValidatePropertyPath(TEXT(".Property"), Error));
	TestFalse("Trailing dot should be invalid",
		FMCPParamValidator::ValidatePropertyPath(TEXT("Property."), Error));
	TestFalse("Special characters should be invalid",
		FMCPParamValidator::ValidatePropertyPath(TEXT("Property<T>"), Error));

	return true;
}

// ===== Numeric Value Validation Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_ValidateNumericValue_EdgeCases,
	"UnrealClaude.MCP.ParamValidator.NumericValue.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_ValidateNumericValue_EdgeCases::RunTest(const FString& Parameters)
{
	FString Error;

	// Valid values
	TestTrue("Zero should be valid",
		FMCPParamValidator::ValidateNumericValue(0.0, TEXT("test"), Error));
	TestTrue("Positive value should be valid",
		FMCPParamValidator::ValidateNumericValue(100.0, TEXT("test"), Error));
	TestTrue("Negative value should be valid",
		FMCPParamValidator::ValidateNumericValue(-100.0, TEXT("test"), Error));

	// Invalid values
	TestFalse("NaN should be invalid",
		FMCPParamValidator::ValidateNumericValue(std::numeric_limits<double>::quiet_NaN(), TEXT("test"), Error));
	TestFalse("Positive infinity should be invalid",
		FMCPParamValidator::ValidateNumericValue(std::numeric_limits<double>::infinity(), TEXT("test"), Error));
	TestFalse("Negative infinity should be invalid",
		FMCPParamValidator::ValidateNumericValue(-std::numeric_limits<double>::infinity(), TEXT("test"), Error));

	// Bounds checking with custom max
	TestFalse("Value exceeding max should be invalid",
		FMCPParamValidator::ValidateNumericValue(1e10, TEXT("test"), Error, 1e6));

	return true;
}

// ===== String Sanitization Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_SanitizeString_RemovesDangerousChars,
	"UnrealClaude.MCP.ParamValidator.SanitizeString.RemovesDangerousChars",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_SanitizeString_RemovesDangerousChars::RunTest(const FString& Parameters)
{
	// Test that dangerous characters are removed
	FString Sanitized = FMCPParamValidator::SanitizeString(TEXT("Hello<script>World</script>"));
	TestFalse("< should be removed", Sanitized.Contains(TEXT("<")));
	TestFalse("> should be removed", Sanitized.Contains(TEXT(">")));
	TestTrue("Normal text should remain", Sanitized.Contains(TEXT("Hello")));
	TestTrue("Normal text should remain", Sanitized.Contains(TEXT("World")));

	// Test shell escape removal
	Sanitized = FMCPParamValidator::SanitizeString(TEXT("Hello`rm -rf`World"));
	TestFalse("Backticks should be removed", Sanitized.Contains(TEXT("`")));

	// Test dollar sign removal
	Sanitized = FMCPParamValidator::SanitizeString(TEXT("Hello$(cmd)World"));
	TestFalse("$ should be removed", Sanitized.Contains(TEXT("$")));
	TestFalse("( should be removed", Sanitized.Contains(TEXT("(")));
	TestFalse(") should be removed", Sanitized.Contains(TEXT(")")));

	return true;
}

// ===== Blueprint Variable/Function Name Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMCPParamValidator_ValidateBlueprintNames,
	"UnrealClaude.MCP.ParamValidator.BlueprintNames.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMCPParamValidator_ValidateBlueprintNames::RunTest(const FString& Parameters)
{
	FString Error;

	// Valid variable names
	TestTrue("Simple variable should be valid",
		FMCPParamValidator::ValidateBlueprintVariableName(TEXT("MyVariable"), Error));
	TestTrue("Variable with underscore should be valid",
		FMCPParamValidator::ValidateBlueprintVariableName(TEXT("_MyVariable"), Error));
	TestTrue("Variable with numbers should be valid",
		FMCPParamValidator::ValidateBlueprintVariableName(TEXT("MyVariable123"), Error));

	// Invalid variable names
	TestFalse("Variable starting with number should be invalid",
		FMCPParamValidator::ValidateBlueprintVariableName(TEXT("123Variable"), Error));
	TestFalse("Variable with space should be invalid",
		FMCPParamValidator::ValidateBlueprintVariableName(TEXT("My Variable"), Error));
	TestFalse("Variable with special char should be invalid",
		FMCPParamValidator::ValidateBlueprintVariableName(TEXT("My-Variable"), Error));
	TestFalse("Empty variable name should be invalid",
		FMCPParamValidator::ValidateBlueprintVariableName(TEXT(""), Error));

	// Same rules apply to function names
	TestTrue("Simple function should be valid",
		FMCPParamValidator::ValidateBlueprintFunctionName(TEXT("MyFunction"), Error));
	TestFalse("Function starting with number should be invalid",
		FMCPParamValidator::ValidateBlueprintFunctionName(TEXT("123Function"), Error));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
