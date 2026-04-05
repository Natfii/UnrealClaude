// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "MCP/MCPToolBase.h"

/**
 * MCP Tool for capturing material preview images.
 * Renders a material to a thumbnail and returns as base64.
 */
class FMCPTool_MaterialPreview : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override;
	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
