// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPParamValidator.h"
#include "UnrealClaudeConstants.h"

// Use constants from centralized header
using namespace UnrealClaudeConstants::MCPValidation;

const TArray<FString>& FMCPParamValidator::GetBlockedConsoleCommands()
{
	static TArray<FString> BlockedCommands = {
		// Dangerous commands that could crash or corrupt
		TEXT("quit"),
		TEXT("exit"),
		TEXT("crash"),
		TEXT("forcegc"),
		TEXT("forcecrash"),
		TEXT("debug crash"),

		// Memory manipulation commands
		TEXT("mem"),
		TEXT("memreport"),
		TEXT("obj"),

		// File system commands that could be dangerous
		TEXT("exec"),
		TEXT("savepackage"),
		TEXT("deletepackage"),

		// Network commands that could expose security issues
		TEXT("net"),
		TEXT("admin"),

		// Engine shutdown commands
		TEXT("shutdown"),
		TEXT("restartlevel"),
		TEXT("open"),  // Could open malicious maps
		TEXT("servertravel"),

		// Debug camera can cause issues in editor
		TEXT("toggledebugcamera"),
		TEXT("enablecheats"),

		// Potentially dangerous stat commands
		TEXT("stat slow"),

		// Commands that modify engine state dangerously
		TEXT("gc."),
		TEXT("r."),  // Block all rendering CVars as they can crash
	};

	return BlockedCommands;
}

bool FMCPParamValidator::ValidateActorName(const FString& Name, FString& OutError)
{
	if (Name.IsEmpty())
	{
		OutError = TEXT("Actor name cannot be empty");
		return false;
	}

	if (Name.Len() > MaxActorNameLength)
	{
		OutError = FString::Printf(TEXT("Actor name exceeds maximum length of %d characters"), MaxActorNameLength);
		return false;
	}

	// Check for dangerous characters (optimized: avoid FString allocation in loop)
	int32 FoundIndex;
	for (const TCHAR* c = DangerousChars; *c; ++c)
	{
		if (Name.FindChar(*c, FoundIndex))
		{
			OutError = FString::Printf(TEXT("Actor name contains invalid character: '%c'"), *c);
			return false;
		}
	}

	// Check for control characters
	for (TCHAR c : Name)
	{
		if (c < 32 && c != TEXT('\0'))
		{
			OutError = TEXT("Actor name contains control characters");
			return false;
		}
	}

	return true;
}

bool FMCPParamValidator::ValidatePropertyPath(const FString& PropertyPath, FString& OutError)
{
	if (PropertyPath.IsEmpty())
	{
		OutError = TEXT("Property path cannot be empty");
		return false;
	}

	if (PropertyPath.Len() > MaxPropertyPathLength)
	{
		OutError = FString::Printf(TEXT("Property path exceeds maximum length of %d characters"), MaxPropertyPathLength);
		return false;
	}

	// Property paths should only contain alphanumeric, underscore, and dot
	for (TCHAR c : PropertyPath)
	{
		if (!FChar::IsAlnum(c) && c != TEXT('_') && c != TEXT('.'))
		{
			OutError = FString::Printf(TEXT("Property path contains invalid character: '%c'. Only alphanumeric, underscore, and dot are allowed."), c);
			return false;
		}
	}

	// Check for double dots which could indicate path traversal attempts
	if (PropertyPath.Contains(TEXT("..")))
	{
		OutError = TEXT("Property path cannot contain consecutive dots");
		return false;
	}

	// Should not start or end with a dot
	if (PropertyPath.StartsWith(TEXT(".")) || PropertyPath.EndsWith(TEXT(".")))
	{
		OutError = TEXT("Property path cannot start or end with a dot");
		return false;
	}

	return true;
}

bool FMCPParamValidator::ValidateClassPath(const FString& ClassPath, FString& OutError)
{
	if (ClassPath.IsEmpty())
	{
		OutError = TEXT("Class path cannot be empty");
		return false;
	}

	if (ClassPath.Len() > MaxClassPathLength)
	{
		OutError = FString::Printf(TEXT("Class path exceeds maximum length of %d characters"), MaxClassPathLength);
		return false;
	}

	// Check for dangerous characters (excluding / and . which are valid in paths)
	// Optimized: avoid FString allocation in loop
	int32 FoundIndex;
	for (const TCHAR* c = DangerousChars; *c; ++c)
	{
		if (ClassPath.FindChar(*c, FoundIndex))
		{
			OutError = FString::Printf(TEXT("Class path contains invalid character: '%c'"), *c);
			return false;
		}
	}

	// Check for path traversal
	if (ClassPath.Contains(TEXT("..")))
	{
		OutError = TEXT("Class path cannot contain path traversal sequences");
		return false;
	}

	return true;
}

bool FMCPParamValidator::ValidateConsoleCommand(const FString& Command, FString& OutError)
{
	if (Command.IsEmpty())
	{
		OutError = TEXT("Command cannot be empty");
		return false;
	}

	if (Command.Len() > MaxCommandLength)
	{
		OutError = FString::Printf(TEXT("Command exceeds maximum length of %d characters"), MaxCommandLength);
		return false;
	}

	// Check against blocklist
	FString CommandLower = Command.ToLower().TrimStartAndEnd();

	for (const FString& Blocked : GetBlockedConsoleCommands())
	{
		if (CommandLower.StartsWith(Blocked.ToLower()))
		{
			OutError = FString::Printf(TEXT("Command '%s' is blocked for safety"), *Blocked);
			return false;
		}
	}

	// Check for command chaining attempts
	if (Command.Contains(TEXT(";")) || Command.Contains(TEXT("|")) || Command.Contains(TEXT("&&")))
	{
		OutError = TEXT("Command chaining is not allowed");
		return false;
	}

	// Check for shell escape attempts
	if (Command.Contains(TEXT("`")) || Command.Contains(TEXT("$(")) || Command.Contains(TEXT("${")))
	{
		OutError = TEXT("Shell escape sequences are not allowed");
		return false;
	}

	return true;
}

bool FMCPParamValidator::ValidateNumericValue(double Value, const FString& FieldName, FString& OutError, double MaxAbsValue)
{
	// Check for NaN
	if (FMath::IsNaN(Value))
	{
		OutError = FString::Printf(TEXT("%s: NaN is not a valid value"), *FieldName);
		return false;
	}

	// Check for infinity
	if (!FMath::IsFinite(Value))
	{
		OutError = FString::Printf(TEXT("%s: Infinite values are not allowed"), *FieldName);
		return false;
	}

	// Check bounds
	if (FMath::Abs(Value) > MaxAbsValue)
	{
		OutError = FString::Printf(TEXT("%s: Value %f exceeds maximum allowed magnitude of %f"), *FieldName, Value, MaxAbsValue);
		return false;
	}

	return true;
}

bool FMCPParamValidator::ValidateStringLength(const FString& Value, const FString& FieldName, int32 MaxLength, FString& OutError)
{
	if (Value.Len() > MaxLength)
	{
		OutError = FString::Printf(TEXT("%s: String length %d exceeds maximum of %d"), *FieldName, Value.Len(), MaxLength);
		return false;
	}
	return true;
}

FString FMCPParamValidator::SanitizeString(const FString& Input)
{
	// Build result without dangerous characters in single pass (optimized)
	FString Output;
	Output.Reserve(Input.Len());

	for (TCHAR InputChar : Input)
	{
		// Check if this character is in the dangerous chars list
		bool bIsDangerous = false;
		for (const TCHAR* c = DangerousChars; *c; ++c)
		{
			if (InputChar == *c)
			{
				bIsDangerous = true;
				break;
			}
		}

		// Only keep non-dangerous, non-control characters
		if (!bIsDangerous && (InputChar >= 32 || InputChar == TEXT('\0')))
		{
			Output.AppendChar(InputChar);
		}
	}

	return Output;
}

bool FMCPParamValidator::ValidateBlueprintPath(const FString& BlueprintPath, FString& OutError)
{
	if (BlueprintPath.IsEmpty())
	{
		OutError = TEXT("Blueprint path cannot be empty");
		return false;
	}

	if (BlueprintPath.Len() > 512)
	{
		OutError = TEXT("Blueprint path exceeds maximum length of 512 characters");
		return false;
	}

	// Block engine Blueprints
	if (BlueprintPath.StartsWith(TEXT("/Engine/")) || BlueprintPath.StartsWith(TEXT("/Script/")))
	{
		OutError = TEXT("Cannot access engine or script Blueprints");
		return false;
	}

	// Check for path traversal
	if (BlueprintPath.Contains(TEXT("..")))
	{
		OutError = TEXT("Blueprint path cannot contain path traversal sequences");
		return false;
	}

	// Check for dangerous characters
	// Optimized: avoid FString allocation in loop
	int32 FoundIndex;
	for (const TCHAR* c = DangerousChars; *c; ++c)
	{
		if (BlueprintPath.FindChar(*c, FoundIndex))
		{
			OutError = FString::Printf(TEXT("Blueprint path contains invalid character: '%c'"), *c);
			return false;
		}
	}

	return true;
}

bool FMCPParamValidator::ValidateBlueprintVariableName(const FString& VariableName, FString& OutError)
{
	if (VariableName.IsEmpty())
	{
		OutError = TEXT("Variable name cannot be empty");
		return false;
	}

	if (VariableName.Len() > 128)
	{
		OutError = TEXT("Variable name exceeds maximum length of 128 characters");
		return false;
	}

	// Must start with letter or underscore
	if (!FChar::IsAlpha(VariableName[0]) && VariableName[0] != TEXT('_'))
	{
		OutError = TEXT("Variable name must start with a letter or underscore");
		return false;
	}

	// Only alphanumeric and underscore
	for (TCHAR C : VariableName)
	{
		if (!FChar::IsAlnum(C) && C != TEXT('_'))
		{
			OutError = FString::Printf(TEXT("Variable name contains invalid character: '%c'"), C);
			return false;
		}
	}

	return true;
}

bool FMCPParamValidator::ValidateBlueprintFunctionName(const FString& FunctionName, FString& OutError)
{
	// Same validation rules as variable names
	if (FunctionName.IsEmpty())
	{
		OutError = TEXT("Function name cannot be empty");
		return false;
	}

	if (FunctionName.Len() > 128)
	{
		OutError = TEXT("Function name exceeds maximum length of 128 characters");
		return false;
	}

	// Must start with letter or underscore
	if (!FChar::IsAlpha(FunctionName[0]) && FunctionName[0] != TEXT('_'))
	{
		OutError = TEXT("Function name must start with a letter or underscore");
		return false;
	}

	// Only alphanumeric and underscore
	for (TCHAR C : FunctionName)
	{
		if (!FChar::IsAlnum(C) && C != TEXT('_'))
		{
			OutError = FString::Printf(TEXT("Function name contains invalid character: '%c'"), C);
			return false;
		}
	}

	return true;
}
