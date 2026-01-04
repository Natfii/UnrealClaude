// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Run an Unreal console command
 */
class FMCPTool_RunConsoleCommand : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("run_console_command");
		Info.Description = TEXT("Execute an Unreal Engine console command");
		Info.Parameters = {
			FMCPToolParameter(TEXT("command"), TEXT("string"), TEXT("The console command to execute (e.g., 'stat fps', 'show collision')"), true)
		};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
