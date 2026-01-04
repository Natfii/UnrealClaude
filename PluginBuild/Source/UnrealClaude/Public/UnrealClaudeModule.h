// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUnrealClaude, Log, All);

class FUnrealClaudeMCPServer;

class FUnrealClaudeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the singleton instance */
	static FUnrealClaudeModule& Get();

	/** Check if module is available */
	static bool IsAvailable();

	/** Get the MCP server instance */
	TSharedPtr<FUnrealClaudeMCPServer> GetMCPServer() const { return MCPServer; }

	/** Get MCP server port */
	static constexpr uint32 GetMCPServerPort() { return 3000; }

private:
	void RegisterMenus();
	void UnregisterMenus();
	void StartMCPServer();
	void StopMCPServer();

	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedPtr<class SDockTab> ClaudeTab;
	TSharedPtr<FUnrealClaudeMCPServer> MCPServer;
};
