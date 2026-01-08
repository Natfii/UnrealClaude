// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool for executing scripts with user permission
 *
 * Supports: C++, Python, Console commands, Editor Utility
 *
 * IMPORTANT: Scripts MUST include a header comment with @Description
 * This description is stored in history for context restoration.
 *
 * C++ Header Format:
 * /**
 *  * @UnrealClaude Script
 *  * @Name: MyScript
 *  * @Description: Brief description of what this script does
 *  * @Created: 2026-01-03T10:30:00Z
 *  * /
 *
 * Python Header Format:
 * """
 * @UnrealClaude Script
 * @Name: MyScript
 * @Description: Brief description of what this script does
 * @Created: 2026-01-03T10:30:00Z
 * """
 */
class FMCPTaskQueue;

class FMCPTool_ExecuteScript : public FMCPToolBase
{
public:
	/** Set the task queue for async execution */
	void SetTaskQueue(TSharedPtr<FMCPTaskQueue> InTaskQueue) { TaskQueue = InTaskQueue; }

	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("execute_script");
		Info.Description = TEXT(
			"Execute custom scripts in Unreal Engine with user permission.\n\n"
			"POWERFUL: This tool enables complex operations not possible with other tools. "
			"Scripts run with full engine access. User approval required before execution.\n\n"
			"Script types:\n"
			"- 'cpp': C++ via Live Coding (auto-retries on compile failure)\n"
			"- 'python': Python script (requires Python plugin)\n"
			"- 'console': Console command batch\n"
			"- 'editor_utility': Editor Utility Widget/Blueprint\n\n"
			"IMPORTANT: Include @Description in script header for history tracking:\n"
			"/** @UnrealClaude Script\\n * @Description: What this script does */\n\n"
			"Returns: Script execution result, output, and any errors."
		);
		Info.Parameters = {
			FMCPToolParameter(
				TEXT("script_type"),
				TEXT("string"),
				TEXT("Type: 'cpp', 'python', 'console', or 'editor_utility'"),
				true
			),
			FMCPToolParameter(
				TEXT("script_content"),
				TEXT("string"),
				TEXT("The script code. MUST include @Description in header comment."),
				true
			),
			FMCPToolParameter(
				TEXT("description"),
				TEXT("string"),
				TEXT("Brief description (optional if @Description in header)"),
				false
			)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		Info.Annotations.bDestructiveHint = true; // Scripts can do anything
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	/** Task queue for async execution */
	TSharedPtr<FMCPTaskQueue> TaskQueue;

	/** Execute script synchronously (called by task queue) */
	FMCPToolResult ExecuteSync(const TSharedRef<FJsonObject>& Params);
};
