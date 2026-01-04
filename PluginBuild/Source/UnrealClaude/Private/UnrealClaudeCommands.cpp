// Copyright Your Name. All Rights Reserved.

#include "UnrealClaudeCommands.h"

#define LOCTEXT_NAMESPACE "UnrealClaude"

void FUnrealClaudeCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenClaudePanel,
		"Claude Assistant",
		"Open the Claude AI Assistant panel for UE5.7 help",
		EUserInterfaceActionType::Button,
		FInputChord()
	);

	UI_COMMAND(
		QuickAsk,
		"Quick Ask Claude",
		"Quickly ask Claude a question",
		EUserInterfaceActionType::Button,
		FInputChord()
	);
}

#undef LOCTEXT_NAMESPACE
