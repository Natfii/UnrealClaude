// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UnrealClaudeConstants.h"
#include "UnrealClaudeSettings.generated.h"

/**
 * Project-level settings for the UnrealClaude plugin.
 *
 * Edit in: Project Settings -> Plugins -> Unreal Claude.
 * Stored in: Config/DefaultEditor.ini under [/Script/UnrealClaude.UnrealClaudeSettings].
 *
 * The plugin loads and works with all defaults even if this page is never opened.
 */
UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Unreal Claude"))
class UNREALCLAUDE_API UUnrealClaudeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUnrealClaudeSettings() = default;

	// ---- Server ----

	/**
	 * TCP port the built-in MCP HTTP server listens on.
	 *
	 * Default is 3000. Change this only if another process is already using 3000.
	 * After changing the value you must restart the Unreal Editor for the new port to take effect.
	 * The MCP config written for Claude Code will automatically use the same port.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Server",
		meta=(DisplayName="MCP Server Port",
			  ClampMin="1024", ClampMax="65535",
			  ToolTip="Port for the MCP HTTP server (default 3000). Restart the editor after changing."))
	int32 MCPServerPort = UnrealClaudeConstants::MCPServer::DefaultPort;

	// ---- Security ----

	/**
	 * Auto-approve all script execution requests instead of showing the per-call permission dialog.
	 *
	 * When OFF (default), every Python / C++ / Console / Editor Utility script triggered through the
	 * MCP bridge or the in-editor Claude Assistant chat must be approved by the user via a modal dialog.
	 * That is the safest default and matches the existing behaviour.
	 *
	 * Turn ON to opt into a "trusted" workflow where the host (Claude Code session, an MCP-driven agent,
	 * or an automated test harness) drives many scripts in sequence and the per-script confirmation is
	 * the dominant friction. Approvals are skipped silently and an INFO log line is written instead so
	 * the audit trail is preserved.
	 *
	 * WARNING: enabling this means every script the connected agent submits runs immediately without
	 * human confirmation. Only enable on machines and projects where the connected agent is trusted.
	 * The denylist (py/python, keybind, setbind, etc.) is still enforced even when this is on.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Security",
		meta=(DisplayName="Auto-approve script execution",
			  ToolTip="When ON, scripts run without a permission dialog (still logged). Denylist is always enforced. Only enable for trusted agents."))
	bool bAutoApproveScripts = false;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override { return NSLOCTEXT("UnrealClaude", "SettingsSectionText", "Unreal Claude"); }
	virtual FText GetSectionDescription() const override
	{
		return NSLOCTEXT("UnrealClaude", "SettingsSectionDesc",
			"Configure the Unreal Claude plugin (MCP server port, script security). Defaults work without opening this page.");
	}
#endif
};
