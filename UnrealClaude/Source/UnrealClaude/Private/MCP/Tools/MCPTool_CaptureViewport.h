// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

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
		Info.Description = TEXT(
			"Capture a screenshot of the active viewport.\n\n"
			"Captures the current view from either Play-In-Editor (if running) or the active editor viewport. "
			"Useful for visual verification of scene changes.\n\n"
			"Output: 1024x576 JPEG image encoded as base64 string.\n\n"
			"Use cases:\n"
			"- Verify actor placement after spawning/moving\n"
			"- Check lighting changes\n"
			"- Document scene state\n"
			"- Debug visual issues\n\n"
			"Returns: Base64-encoded JPEG image data."
		);
		Info.Parameters = {};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
