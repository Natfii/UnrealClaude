// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_EditorExit.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"

FMCPToolInfo FMCPTool_EditorExit::GetInfo() const
{
	FMCPToolInfo Info;
	Info.Name = TEXT("editor_exit");
	Info.Description = TEXT("Request editor to close. Shows save dialog for unsaved changes - user decides whether to save.");

	// No parameters needed - always shows save dialog

	Info.Annotations = FMCPToolAnnotations::Destructive(TEXT("Closes the editor"));

	return Info;
}

FMCPToolResult FMCPTool_EditorExit::Execute(const TSharedRef<FJsonObject>& Params)
{
	UE_LOG(LogUnrealClaude, Log, TEXT("Requesting editor exit - save dialog will appear if there are unsaved changes"));

	// Request the main window to close - this triggers the standard exit flow
	// which includes prompting to save dirty packages
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWindow> MainWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (MainWindow.IsValid())
		{
			MainWindow->RequestDestroyWindow();
		}
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("exit_requested"), true);
	ResultData->SetStringField(TEXT("note"), TEXT("Save dialog will appear if there are unsaved changes. User must confirm."));

	return FMCPToolResult::Success(
		TEXT("Editor exit requested - save dialog will appear for unsaved changes"),
		ResultData
	);
}
