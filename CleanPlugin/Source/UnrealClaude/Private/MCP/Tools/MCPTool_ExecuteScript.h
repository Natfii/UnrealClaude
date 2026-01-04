// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolRegistry.h"

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
class FMCPTool_ExecuteScript : public IMCPTool
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("execute_script");
		Info.Description = TEXT(
			"Execute a script with user permission. Supports C++ (via Live Coding), Python, "
			"Console commands, and Editor Utility. C++ scripts auto-retry on compilation failure. "
			"IMPORTANT: All scripts MUST include a header comment with @Description for history tracking."
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
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
