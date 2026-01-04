// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

/**
 * Editor commands for the Unreal Claude plugin
 */
class FUnrealClaudeCommands : public TCommands<FUnrealClaudeCommands>
{
public:
	FUnrealClaudeCommands()
		: TCommands<FUnrealClaudeCommands>(
			TEXT("UnrealClaude"),
			NSLOCTEXT("Contexts", "UnrealClaude", "Unreal Claude"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	// TCommands interface
	virtual void RegisterCommands() override;

public:
	/** Open the Claude Assistant panel */
	TSharedPtr<FUICommandInfo> OpenClaudePanel;
	
	/** Quick ask - opens a small dialog for quick questions */
	TSharedPtr<FUICommandInfo> QuickAsk;
};
