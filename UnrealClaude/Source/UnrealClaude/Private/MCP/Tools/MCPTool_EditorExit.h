// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "MCP/MCPToolBase.h"

/**
 * MCP Tool for requesting editor exit with save dialog.
 */
class FMCPTool_EditorExit : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override;
	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
