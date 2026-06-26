// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_RunConsoleCommand.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "ScriptExecutionManager.h"
#include "ScriptTypes.h"

// SECURITY: this tool intentionally does NOT call GEditor->Exec(World, *Command, ...) directly.
// A raw console Exec bypasses user approval, and because the Python Editor Script Plugin registers
// the `py` command, an un-gated console command can launch arbitrary Python (= OS-level RCE). Instead
// we (1) auto-reject denylisted commands without prompting, then (2) route the rest through
// FScriptExecutionManager (EScriptType::Console) so every command passes the same deny-by-default
// permission dialog as execute_script. run_console_command is hidden from the model by default; this
// guard is defense-in-depth for any caller that reaches the tool directly (e.g. a raw POST to :3000).
FMCPToolResult FMCPTool_RunConsoleCommand::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Command;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("command"), Command, ParamError))
	{
		return ParamError.GetValue();
	}

	// Layer 1: auto-block catastrophic commands (denylist) WITHOUT prompting the user.
	if (!ValidateConsoleCommandParam(Command, ParamError))
	{
		return ParamError.GetValue();
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("run_console_command requesting approval: %s"), *Command);

	// Layer 2: deny-by-default permission dialog + execution, shared with execute_script.
	const FScriptExecutionResult Result = FScriptExecutionManager::Get().ExecuteScript(
		EScriptType::Console, Command, TEXT("run_console_command"));

	if (!Result.bSuccess)
	{
		return FMCPToolResult::Error(Result.Message);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("command"), Command);
	ResultData->SetStringField(TEXT("output"), Result.Output);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Executed command: %s"), *Command),
		ResultData
	);
}
