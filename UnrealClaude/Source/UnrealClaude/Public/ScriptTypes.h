// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Script type enumeration
 */
enum class EScriptType : uint8
{
	Cpp,
	Python,
	Console,
	EditorUtility
};

/**
 * Get string representation of script type
 */
inline FString ScriptTypeToString(EScriptType Type)
{
	switch (Type)
	{
		case EScriptType::Cpp: return TEXT("cpp");
		case EScriptType::Python: return TEXT("python");
		case EScriptType::Console: return TEXT("console");
		case EScriptType::EditorUtility: return TEXT("editor_utility");
		default: return TEXT("unknown");
	}
}

/**
 * Parse script type from string
 */
inline EScriptType StringToScriptType(const FString& TypeStr)
{
	if (TypeStr.Equals(TEXT("cpp"), ESearchCase::IgnoreCase)) return EScriptType::Cpp;
	if (TypeStr.Equals(TEXT("python"), ESearchCase::IgnoreCase)) return EScriptType::Python;
	if (TypeStr.Equals(TEXT("console"), ESearchCase::IgnoreCase)) return EScriptType::Console;
	if (TypeStr.Equals(TEXT("editor_utility"), ESearchCase::IgnoreCase)) return EScriptType::EditorUtility;
	return EScriptType::Console; // Default
}

/**
 * Get file extension for script type
 */
inline FString GetScriptExtension(EScriptType Type)
{
	switch (Type)
	{
		case EScriptType::Cpp: return TEXT(".cpp");
		case EScriptType::Python: return TEXT(".py");
		case EScriptType::Console: return TEXT(".txt");
		case EScriptType::EditorUtility: return TEXT(".uasset");
		default: return TEXT(".txt");
	}
}

/**
 * Script header comment format for each type
 * These headers are parsed to extract description for history
 */
namespace ScriptHeader
{
	// C++ header format
	inline FString FormatCppHeader(const FString& Description, const FString& ScriptName)
	{
		return FString::Printf(TEXT(
			"/**\n"
			" * @UnrealClaude Script\n"
			" * @Name: %s\n"
			" * @Description: %s\n"
			" * @Created: %s\n"
			" */\n\n"
		), *ScriptName, *Description, *FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")));
	}

	// Python header format
	inline FString FormatPythonHeader(const FString& Description, const FString& ScriptName)
	{
		return FString::Printf(TEXT(
			"\"\"\"\n"
			"@UnrealClaude Script\n"
			"@Name: %s\n"
			"@Description: %s\n"
			"@Created: %s\n"
			"\"\"\"\n\n"
		), *ScriptName, *Description, *FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")));
	}

	// Console commands header (as comment)
	inline FString FormatConsoleHeader(const FString& Description, const FString& ScriptName)
	{
		return FString::Printf(TEXT(
			"# @UnrealClaude Script\n"
			"# @Name: %s\n"
			"# @Description: %s\n"
			"# @Created: %s\n\n"
		), *ScriptName, *Description, *FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")));
	}

	/**
	 * Parse description from script header comment
	 * Looks for @Description: line in header
	 */
	inline FString ParseDescription(const FString& ScriptContent)
	{
		// Look for @Description: pattern
		int32 DescStart = ScriptContent.Find(TEXT("@Description:"));
		if (DescStart == INDEX_NONE)
		{
			return TEXT("No description provided");
		}

		// Find end of line
		int32 LineEnd = ScriptContent.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, DescStart);
		if (LineEnd == INDEX_NONE)
		{
			LineEnd = ScriptContent.Len();
		}

		// Extract description text
		FString Description = ScriptContent.Mid(DescStart + 13, LineEnd - DescStart - 13);
		Description.TrimStartAndEndInline();

		return Description;
	}

	/**
	 * Parse script name from header
	 */
	inline FString ParseName(const FString& ScriptContent)
	{
		int32 NameStart = ScriptContent.Find(TEXT("@Name:"));
		if (NameStart == INDEX_NONE)
		{
			return TEXT("");
		}

		int32 LineEnd = ScriptContent.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, NameStart);
		if (LineEnd == INDEX_NONE)
		{
			LineEnd = ScriptContent.Len();
		}

		FString Name = ScriptContent.Mid(NameStart + 6, LineEnd - NameStart - 6);
		Name.TrimStartAndEndInline();

		return Name;
	}
}

/**
 * Script history entry - stored in JSON log
 * Only stores description (parsed from header), not full code
 */
struct FScriptHistoryEntry
{
	/** Unique ID for this script */
	FGuid ScriptId;

	/** Script type */
	EScriptType ScriptType;

	/** Script filename */
	FString Filename;

	/** Description from header comment (NOT full code) */
	FString Description;

	/** Whether execution succeeded */
	bool bSuccess;

	/** Result message */
	FString ResultMessage;

	/** Execution timestamp */
	FDateTime Timestamp;

	/** Full file path (for cleanup) */
	FString FilePath;

	FScriptHistoryEntry()
		: ScriptType(EScriptType::Console)
		, bSuccess(false)
		, Timestamp(FDateTime::UtcNow())
	{
		ScriptId = FGuid::NewGuid();
	}

	/** Serialize to JSON */
	TSharedPtr<FJsonObject> ToJson() const
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("id"), ScriptId.ToString());
		Json->SetStringField(TEXT("type"), ScriptTypeToString(ScriptType));
		Json->SetStringField(TEXT("filename"), Filename);
		Json->SetStringField(TEXT("description"), Description);
		Json->SetBoolField(TEXT("success"), bSuccess);
		Json->SetStringField(TEXT("result"), ResultMessage);
		Json->SetStringField(TEXT("timestamp"), Timestamp.ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")));
		Json->SetStringField(TEXT("filepath"), FilePath);
		return Json;
	}

	/** Deserialize from JSON */
	static FScriptHistoryEntry FromJson(const TSharedPtr<FJsonObject>& Json)
	{
		FScriptHistoryEntry Entry;

		FString IdStr;
		if (Json->TryGetStringField(TEXT("id"), IdStr))
		{
			FGuid::Parse(IdStr, Entry.ScriptId);
		}

		FString TypeStr;
		if (Json->TryGetStringField(TEXT("type"), TypeStr))
		{
			Entry.ScriptType = StringToScriptType(TypeStr);
		}

		Json->TryGetStringField(TEXT("filename"), Entry.Filename);
		Json->TryGetStringField(TEXT("description"), Entry.Description);
		Json->TryGetBoolField(TEXT("success"), Entry.bSuccess);
		Json->TryGetStringField(TEXT("result"), Entry.ResultMessage);
		Json->TryGetStringField(TEXT("filepath"), Entry.FilePath);

		FString TimestampStr;
		if (Json->TryGetStringField(TEXT("timestamp"), TimestampStr))
		{
			FDateTime::ParseIso8601(*TimestampStr, Entry.Timestamp);
		}

		return Entry;
	}
};

/**
 * Result of script execution
 */
struct FScriptExecutionResult
{
	/** Whether execution succeeded */
	bool bSuccess;

	/** Human-readable message */
	FString Message;

	/** Output from execution (stdout, console output) */
	FString Output;

	/** Error output if failed */
	FString ErrorOutput;

	/** Number of retry attempts made */
	int32 RetryCount;

	FScriptExecutionResult()
		: bSuccess(false)
		, RetryCount(0)
	{}

	static FScriptExecutionResult Success(const FString& InMessage, const FString& InOutput = TEXT(""))
	{
		FScriptExecutionResult Result;
		Result.bSuccess = true;
		Result.Message = InMessage;
		Result.Output = InOutput;
		return Result;
	}

	static FScriptExecutionResult Error(const FString& InMessage, const FString& InErrorOutput = TEXT(""))
	{
		FScriptExecutionResult Result;
		Result.bSuccess = false;
		Result.Message = InMessage;
		Result.ErrorOutput = InErrorOutput;
		return Result;
	}
};
