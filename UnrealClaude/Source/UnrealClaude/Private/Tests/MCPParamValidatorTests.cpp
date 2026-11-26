// Copyright Natali Caggiano. All Rights Reserved.

/**
 * Unit tests for MCPParamValidator.
 * BDD-style spec — see UE Automation Spec docs for Describe/It structure.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeTestFlags.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(
	FMCPParamValidatorSpec,
	"UnrealClaude.MCP.ParamValidator",
	UnrealClaudeTest::DefaultTestFlags)
END_DEFINE_SPEC(FMCPParamValidatorSpec)

void FMCPParamValidatorSpec::Define()
{
	Describe(TEXT("ValidateActorName"), [this]()
	{
		Describe(TEXT("accepts"), [this]()
		{
			It("a simple alphabetic name", [this]()
			{
				FString Error;
				TestTrue(TEXT("MyActor"), FMCPParamValidator::ValidateActorName(TEXT("MyActor"), Error));
			});

			It("a name with digits", [this]()
			{
				FString Error;
				TestTrue(TEXT("Actor123"), FMCPParamValidator::ValidateActorName(TEXT("Actor123"), Error));
			});

			It("a name with an underscore", [this]()
			{
				FString Error;
				TestTrue(TEXT("My_Actor"), FMCPParamValidator::ValidateActorName(TEXT("My_Actor"), Error));
			});

			It("a name with a dash", [this]()
			{
				FString Error;
				TestTrue(TEXT("My-Actor"), FMCPParamValidator::ValidateActorName(TEXT("My-Actor"), Error));
			});

			It("a name with spaces", [this]()
			{
				FString Error;
				TestTrue(TEXT("My Actor"), FMCPParamValidator::ValidateActorName(TEXT("My Actor"), Error));
			});
		});

		Describe(TEXT("rejects"), [this]()
		{
			It("an empty name with a length-related error message", [this]()
			{
				FString Error;
				TestFalse(TEXT("empty string"), FMCPParamValidator::ValidateActorName(TEXT(""), Error));
				TestTrue(TEXT("error mentions emptiness"), Error.Contains(TEXT("empty")));
			});

			It("a name containing '<'", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor<Script>"), FMCPParamValidator::ValidateActorName(TEXT("Actor<Script>"), Error));
			});

			It("a name containing '>'", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor>test"), FMCPParamValidator::ValidateActorName(TEXT("Actor>test"), Error));
			});

			It("a name containing '|'", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor|test"), FMCPParamValidator::ValidateActorName(TEXT("Actor|test"), Error));
			});

			It("a name containing '&'", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor&test"), FMCPParamValidator::ValidateActorName(TEXT("Actor&test"), Error));
			});

			It("a name containing ';'", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor;drop"), FMCPParamValidator::ValidateActorName(TEXT("Actor;drop"), Error));
			});

			It("a name containing a backtick", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor`cmd`"), FMCPParamValidator::ValidateActorName(TEXT("Actor`cmd`"), Error));
			});

			It("a name containing '$'", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor$var"), FMCPParamValidator::ValidateActorName(TEXT("Actor$var"), Error));
			});

			It("a name containing parentheses", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor(test)"), FMCPParamValidator::ValidateActorName(TEXT("Actor(test)"), Error));
			});

			It("a name containing braces", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor{test}"), FMCPParamValidator::ValidateActorName(TEXT("Actor{test}"), Error));
			});

			It("a name containing brackets", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor[0]"), FMCPParamValidator::ValidateActorName(TEXT("Actor[0]"), Error));
			});

			It("a name containing '!'", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor!"), FMCPParamValidator::ValidateActorName(TEXT("Actor!"), Error));
			});

			It("a name containing '*'", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor*"), FMCPParamValidator::ValidateActorName(TEXT("Actor*"), Error));
			});

			It("a name containing '?'", [this]()
			{
				FString Error;
				TestFalse(TEXT("Actor?"), FMCPParamValidator::ValidateActorName(TEXT("Actor?"), Error));
			});

			It("a name containing '~'", [this]()
			{
				FString Error;
				TestFalse(TEXT("~Actor"), FMCPParamValidator::ValidateActorName(TEXT("~Actor"), Error));
			});

			It("a name exceeding the max length with a length-related error message", [this]()
			{
				FString LongName;
				for (int32 i = 0; i < 300; i++)
				{
					LongName += TEXT("A");
				}
				FString Error;
				TestFalse(TEXT("300-char name"), FMCPParamValidator::ValidateActorName(LongName, Error));
				TestTrue(TEXT("error mentions length or 256"),
					Error.Contains(TEXT("length")) || Error.Contains(TEXT("256")));
			});
		});
	});

	Describe(TEXT("ValidateConsoleCommand"), [this]()
	{
		Describe(TEXT("blocks dangerous commands"), [this]()
		{
			It("blocks 'quit'", [this]()
			{
				FString Error;
				TestFalse(TEXT("quit"), FMCPParamValidator::ValidateConsoleCommand(TEXT("quit"), Error));
			});

			It("blocks 'exit'", [this]()
			{
				FString Error;
				TestFalse(TEXT("exit"), FMCPParamValidator::ValidateConsoleCommand(TEXT("exit"), Error));
			});

			It("blocks 'crash'", [this]()
			{
				FString Error;
				TestFalse(TEXT("crash"), FMCPParamValidator::ValidateConsoleCommand(TEXT("crash"), Error));
			});

			It("blocks 'forcecrash'", [this]()
			{
				FString Error;
				TestFalse(TEXT("forcecrash"), FMCPParamValidator::ValidateConsoleCommand(TEXT("forcecrash"), Error));
			});

			It("blocks 'shutdown'", [this]()
			{
				FString Error;
				TestFalse(TEXT("shutdown"), FMCPParamValidator::ValidateConsoleCommand(TEXT("shutdown"), Error));
			});

			It("is case-insensitive for blocked commands", [this]()
			{
				FString Error;
				TestFalse(TEXT("QUIT"), FMCPParamValidator::ValidateConsoleCommand(TEXT("QUIT"), Error));
				TestFalse(TEXT("Quit"), FMCPParamValidator::ValidateConsoleCommand(TEXT("Quit"), Error));
			});

			It("blocks commands by prefix", [this]()
			{
				FString Error;
				TestFalse(TEXT("gc.CollectGarbage"),
					FMCPParamValidator::ValidateConsoleCommand(TEXT("gc.CollectGarbage"), Error));
				TestFalse(TEXT("r.ScreenPercentage 50"),
					FMCPParamValidator::ValidateConsoleCommand(TEXT("r.ScreenPercentage 50"), Error));
			});
		});

		Describe(TEXT("blocks command chaining and shell escapes"), [this]()
		{
			It("blocks semicolon chaining", [this]()
			{
				FString Error;
				TestFalse(TEXT("stat fps; quit"),
					FMCPParamValidator::ValidateConsoleCommand(TEXT("stat fps; quit"), Error));
			});

			It("blocks pipe chaining", [this]()
			{
				FString Error;
				TestFalse(TEXT("stat fps | quit"),
					FMCPParamValidator::ValidateConsoleCommand(TEXT("stat fps | quit"), Error));
			});

			It("blocks '&&' chaining", [this]()
			{
				FString Error;
				TestFalse(TEXT("stat fps && quit"),
					FMCPParamValidator::ValidateConsoleCommand(TEXT("stat fps && quit"), Error));
			});

			It("blocks backtick escapes", [this]()
			{
				FString Error;
				TestFalse(TEXT("stat `quit`"),
					FMCPParamValidator::ValidateConsoleCommand(TEXT("stat `quit`"), Error));
			});

			It("blocks $() escapes", [this]()
			{
				FString Error;
				TestFalse(TEXT("stat $(quit)"),
					FMCPParamValidator::ValidateConsoleCommand(TEXT("stat $(quit)"), Error));
			});

			It("blocks ${} escapes", [this]()
			{
				FString Error;
				TestFalse(TEXT("stat ${quit}"),
					FMCPParamValidator::ValidateConsoleCommand(TEXT("stat ${quit}"), Error));
			});
		});

		Describe(TEXT("blocks interpreter escapes"), [this]()
		{
			It("blocks py / python / keybind / setbind", [this]()
			{
				FString Error;
				TestFalse(TEXT("py"), FMCPParamValidator::ValidateConsoleCommand(TEXT("py print(1)"), Error));
				TestFalse(TEXT("python"), FMCPParamValidator::ValidateConsoleCommand(TEXT("python -c \"print(1)\""), Error));
				TestFalse(TEXT("keybind"), FMCPParamValidator::ValidateConsoleCommand(TEXT("keybind f1 quit"), Error));
				TestFalse(TEXT("setbind"), FMCPParamValidator::ValidateConsoleCommand(TEXT("setbind f1 quit"), Error));
			});
		});

		Describe(TEXT("accepts safe commands"), [this]()
		{
			It("accepts 'stat fps'", [this]()
			{
				FString Error;
				TestTrue(TEXT("stat fps"), FMCPParamValidator::ValidateConsoleCommand(TEXT("stat fps"), Error));
			});

			It("accepts 'stat unit'", [this]()
			{
				FString Error;
				TestTrue(TEXT("stat unit"), FMCPParamValidator::ValidateConsoleCommand(TEXT("stat unit"), Error));
			});

			It("accepts 'showlog'", [this]()
			{
				FString Error;
				TestTrue(TEXT("showlog"), FMCPParamValidator::ValidateConsoleCommand(TEXT("showlog"), Error));
			});

			It("accepts 'show collision'", [this]()
			{
				FString Error;
				TestTrue(TEXT("show collision"),
					FMCPParamValidator::ValidateConsoleCommand(TEXT("show collision"), Error));
			});
		});
	});

	Describe(TEXT("ValidateBlueprintPath"), [this]()
	{
		It("blocks /Engine/ paths", [this]()
		{
			FString Error;
			TestFalse(TEXT("/Engine/..."),
				FMCPParamValidator::ValidateBlueprintPath(TEXT("/Engine/EditorBlueprintResources/StandardMacros"), Error));
		});

		It("blocks /Script/ paths", [this]()
		{
			FString Error;
			TestFalse(TEXT("/Script/Engine.Actor"),
				FMCPParamValidator::ValidateBlueprintPath(TEXT("/Script/Engine.Actor"), Error));
		});

		It("blocks path traversal via '..'", [this]()
		{
			FString Error;
			TestFalse(TEXT("/Game/../Engine/SomeBP"),
				FMCPParamValidator::ValidateBlueprintPath(TEXT("/Game/../Engine/SomeBP"), Error));
		});

		It("accepts /Game/ paths", [this]()
		{
			FString Error;
			TestTrue(TEXT("/Game/Blueprints/BP_MyActor"),
				FMCPParamValidator::ValidateBlueprintPath(TEXT("/Game/Blueprints/BP_MyActor"), Error));
		});
	});

	Describe(TEXT("ValidatePropertyPath"), [this]()
	{
		Describe(TEXT("accepts"), [this]()
		{
			It("a simple property name", [this]()
			{
				FString Error;
				TestTrue(TEXT("MyProperty"),
					FMCPParamValidator::ValidatePropertyPath(TEXT("MyProperty"), Error));
			});

			It("a nested property path", [this]()
			{
				FString Error;
				TestTrue(TEXT("Component.SubProperty"),
					FMCPParamValidator::ValidatePropertyPath(TEXT("Component.SubProperty"), Error));
			});

			It("a property with an underscore", [this]()
			{
				FString Error;
				TestTrue(TEXT("My_Property"),
					FMCPParamValidator::ValidatePropertyPath(TEXT("My_Property"), Error));
			});
		});

		Describe(TEXT("rejects"), [this]()
		{
			It("an empty path", [this]()
			{
				FString Error;
				TestFalse(TEXT("empty"), FMCPParamValidator::ValidatePropertyPath(TEXT(""), Error));
			});

			It("a path starting with '..'", [this]()
			{
				FString Error;
				TestFalse(TEXT("..Parent.Prop"),
					FMCPParamValidator::ValidatePropertyPath(TEXT("..Parent.Prop"), Error));
			});

			It("a path with a leading dot", [this]()
			{
				FString Error;
				TestFalse(TEXT(".Property"),
					FMCPParamValidator::ValidatePropertyPath(TEXT(".Property"), Error));
			});

			It("a path with a trailing dot", [this]()
			{
				FString Error;
				TestFalse(TEXT("Property."),
					FMCPParamValidator::ValidatePropertyPath(TEXT("Property."), Error));
			});

			It("a path with special characters", [this]()
			{
				FString Error;
				TestFalse(TEXT("Property<T>"),
					FMCPParamValidator::ValidatePropertyPath(TEXT("Property<T>"), Error));
			});
		});
	});

	Describe(TEXT("ValidateNumericValue"), [this]()
	{
		It("accepts zero", [this]()
		{
			FString Error;
			TestTrue(TEXT("0.0"), FMCPParamValidator::ValidateNumericValue(0.0, TEXT("test"), Error));
		});

		It("accepts a positive finite value", [this]()
		{
			FString Error;
			TestTrue(TEXT("100.0"), FMCPParamValidator::ValidateNumericValue(100.0, TEXT("test"), Error));
		});

		It("accepts a negative finite value", [this]()
		{
			FString Error;
			TestTrue(TEXT("-100.0"), FMCPParamValidator::ValidateNumericValue(-100.0, TEXT("test"), Error));
		});

		It("rejects NaN", [this]()
		{
			FString Error;
			TestFalse(TEXT("NaN"), FMCPParamValidator::ValidateNumericValue(
				std::numeric_limits<double>::quiet_NaN(), TEXT("test"), Error));
		});

		It("rejects positive infinity", [this]()
		{
			FString Error;
			TestFalse(TEXT("+Inf"), FMCPParamValidator::ValidateNumericValue(
				std::numeric_limits<double>::infinity(), TEXT("test"), Error));
		});

		It("rejects negative infinity", [this]()
		{
			FString Error;
			TestFalse(TEXT("-Inf"), FMCPParamValidator::ValidateNumericValue(
				-std::numeric_limits<double>::infinity(), TEXT("test"), Error));
		});

		It("rejects values exceeding the custom max bound", [this]()
		{
			FString Error;
			TestFalse(TEXT("1e10 with max 1e6"),
				FMCPParamValidator::ValidateNumericValue(1e10, TEXT("test"), Error, 1e6));
		});
	});

	Describe(TEXT("SanitizeString"), [this]()
	{
		It("strips '<' and '>' but preserves surrounding text", [this]()
		{
			const FString Sanitized = FMCPParamValidator::SanitizeString(TEXT("Hello<script>World</script>"));
			TestFalse(TEXT("contains '<'"), Sanitized.Contains(TEXT("<")));
			TestFalse(TEXT("contains '>'"), Sanitized.Contains(TEXT(">")));
			TestTrue(TEXT("preserves 'Hello'"), Sanitized.Contains(TEXT("Hello")));
			TestTrue(TEXT("preserves 'World'"), Sanitized.Contains(TEXT("World")));
		});

		It("strips backticks", [this]()
		{
			const FString Sanitized = FMCPParamValidator::SanitizeString(TEXT("Hello`rm -rf`World"));
			TestFalse(TEXT("contains backtick"), Sanitized.Contains(TEXT("`")));
		});

		It("strips $, (, and )", [this]()
		{
			const FString Sanitized = FMCPParamValidator::SanitizeString(TEXT("Hello$(cmd)World"));
			TestFalse(TEXT("contains '$'"), Sanitized.Contains(TEXT("$")));
			TestFalse(TEXT("contains '('"), Sanitized.Contains(TEXT("(")));
			TestFalse(TEXT("contains ')'"), Sanitized.Contains(TEXT(")")));
		});
	});

	Describe(TEXT("ValidateBlueprintVariableName"), [this]()
	{
		It("accepts a simple identifier", [this]()
		{
			FString Error;
			TestTrue(TEXT("MyVariable"),
				FMCPParamValidator::ValidateBlueprintVariableName(TEXT("MyVariable"), Error));
		});

		It("accepts a leading underscore", [this]()
		{
			FString Error;
			TestTrue(TEXT("_MyVariable"),
				FMCPParamValidator::ValidateBlueprintVariableName(TEXT("_MyVariable"), Error));
		});

		It("accepts trailing digits", [this]()
		{
			FString Error;
			TestTrue(TEXT("MyVariable123"),
				FMCPParamValidator::ValidateBlueprintVariableName(TEXT("MyVariable123"), Error));
		});

		It("rejects a name starting with a digit", [this]()
		{
			FString Error;
			TestFalse(TEXT("123Variable"),
				FMCPParamValidator::ValidateBlueprintVariableName(TEXT("123Variable"), Error));
		});

		It("rejects a name with a space", [this]()
		{
			FString Error;
			TestFalse(TEXT("My Variable"),
				FMCPParamValidator::ValidateBlueprintVariableName(TEXT("My Variable"), Error));
		});

		It("rejects a name with a dash", [this]()
		{
			FString Error;
			TestFalse(TEXT("My-Variable"),
				FMCPParamValidator::ValidateBlueprintVariableName(TEXT("My-Variable"), Error));
		});

		It("rejects an empty name", [this]()
		{
			FString Error;
			TestFalse(TEXT("empty"),
				FMCPParamValidator::ValidateBlueprintVariableName(TEXT(""), Error));
		});
	});

	Describe(TEXT("ValidateBlueprintFunctionName"), [this]()
	{
		It("accepts a simple identifier", [this]()
		{
			FString Error;
			TestTrue(TEXT("MyFunction"),
				FMCPParamValidator::ValidateBlueprintFunctionName(TEXT("MyFunction"), Error));
		});

		It("rejects a name starting with a digit", [this]()
		{
			FString Error;
			TestFalse(TEXT("123Function"),
				FMCPParamValidator::ValidateBlueprintFunctionName(TEXT("123Function"), Error));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
