// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScriptExecutionTypes.h"

/**
 * Interface for script executors
 * Each script type (Cpp, Python, Console, EditorUtility) has its own executor
 */
class IScriptExecutor
{
public:
	virtual ~IScriptExecutor() = default;

	/** Get the script type this executor handles */
	virtual EScriptType GetScriptType() const = 0;

	/** Execute the script and return result */
	virtual FScriptExecutionResult Execute(const FString& ScriptContent, const FString& Description) = 0;

	/** Get the file extension for this script type */
	virtual FString GetFileExtension() const = 0;
};
