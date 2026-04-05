// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "MCP/MCPToolBase.h"

/**
 * MCP Tool for triggering Live Coding (hot reload) compilation.
 */
class FMCPTool_LiveCoding : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override;
	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
