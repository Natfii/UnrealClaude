// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Capture a screenshot of the active viewport
 * Returns base64-encoded JPEG (1024x576, quality 70)
 * Captures PIE viewport if running, otherwise active editor viewport
 */
class FMCPTool_CaptureViewport : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("capture_viewport");
		Info.Description = TEXT("Capture screenshot of active viewport (PIE if running, else editor). Returns 1024x576 JPEG as base64.");
		Info.Parameters = {};
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
