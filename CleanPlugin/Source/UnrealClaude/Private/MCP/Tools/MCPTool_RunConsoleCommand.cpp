// Copyright Your Name. All Rights Reserved.

#include "MCPTool_RunConsoleCommand.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeUtils.h"
#include "Editor.h"
#include "Engine/World.h"

FMCPToolResult FMCPTool_RunConsoleCommand::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Validate editor context using base class
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	// Get command
	FString Command;
	if (!Params->TryGetStringField(TEXT("command"), Command))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: command"));
	}

	// Validate command using centralized validator
	FString ValidationError;
	if (!FMCPParamValidator::ValidateConsoleCommand(Command, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Executing console command: %s"), *Command);

	// Create output capture device
	FUnrealClaudeOutputDevice OutputDevice;

	// Execute the command
	GEditor->Exec(World, *Command, OutputDevice);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("command"), Command);
	ResultData->SetStringField(TEXT("output"), OutputDevice.GetTrimmedOutput());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Executed command: %s"), *Command),
		ResultData
	);
}
