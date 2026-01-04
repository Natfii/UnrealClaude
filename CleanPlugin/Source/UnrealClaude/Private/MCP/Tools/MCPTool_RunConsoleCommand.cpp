// Copyright Your Name. All Rights Reserved.

#include "MCPTool_RunConsoleCommand.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/OutputDeviceRedirector.h"

// Custom output device to capture console command output
class FStringOutputDevice : public FOutputDevice
{
public:
	FString Output;

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		Output += V;
		Output += TEXT("\n");
	}
};

FMCPToolResult FMCPTool_RunConsoleCommand::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Validate we're in editor
	if (!GEditor)
	{
		return FMCPToolResult::Error(TEXT("Editor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FMCPToolResult::Error(TEXT("No active world"));
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
	FStringOutputDevice OutputDevice;

	// Execute the command
	GEditor->Exec(World, *Command, OutputDevice);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("command"), Command);
	ResultData->SetStringField(TEXT("output"), OutputDevice.Output.TrimEnd());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Executed command: %s"), *Command),
		ResultData
	);
}
