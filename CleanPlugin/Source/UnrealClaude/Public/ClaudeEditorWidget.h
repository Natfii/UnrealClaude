// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SMultiLineEditableTextBox;
class SScrollBox;
class SVerticalBox;

/**
 * Chat message display widget
 */
class SChatMessage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChatMessage)
		: _IsUser(true)
	{}
		SLATE_ARGUMENT(FString, Message)
		SLATE_ARGUMENT(bool, IsUser)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

/**
 * Main Claude chat widget for the editor
 */
class UNREALCLAUDE_API SClaudeEditorWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClaudeEditorWidget)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SClaudeEditorWidget();

private:
	/** UI Construction */
	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildChatArea();
	TSharedRef<SWidget> BuildInputArea();
	TSharedRef<SWidget> BuildStatusBar();
	
	/** Add a message to the chat display */
	void AddMessage(const FString& Message, bool bIsUser);
	
	/** Add streaming response (appends to last assistant message) */
	void AppendToLastResponse(const FString& Text);
	
	/** Send the current input to Claude */
	void SendMessage();
	
	/** Clear chat history */
	void ClearChat();

	/** Cancel current request */
	void CancelRequest();

	/** Copy selected text or last response */
	void CopyToClipboard();

	/** Restore previous session context */
	void RestoreSession();

	/** Start a new session (clear history and saved session) */
	void NewSession();
	
	/** Handle response from Claude */
	void OnClaudeResponse(const FString& Response, bool bSuccess);
	
	/** Check if Claude CLI is available */
	bool IsClaudeAvailable() const;
	
	/** Get status text */
	FText GetStatusText() const;
	
	/** Get status color */
	FSlateColor GetStatusColor() const;
	
	/** Input text changed */
	void OnInputTextChanged(const FText& NewText);
	
	/** Input text committed (Enter pressed) */
	void OnInputTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
	
	/** Handle key down in input box */
	FReply OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

private:
	/** Chat message container */
	TSharedPtr<SVerticalBox> ChatMessagesBox;
	
	/** Scroll box for chat */
	TSharedPtr<SScrollBox> ChatScrollBox;
	
	/** Input text box */
	TSharedPtr<SMultiLineEditableTextBox> InputTextBox;
	
	/** Current input text */
	FString CurrentInputText;
	
	/** Is currently waiting for response */
	bool bIsWaitingForResponse = false;
	
	/** Last response for copying */
	FString LastResponse;

	/** Accumulated streaming response */
	FString StreamingResponse;

	/** Current streaming message widget (for updating in place) */
	TSharedPtr<STextBlock> StreamingTextBlock;

	/** Include UE5.7 context in prompts */
	bool bIncludeUE57Context = true;

	/** Include project context in prompts */
	bool bIncludeProjectContext = true;

	/** Handle streaming progress from Claude */
	void OnClaudeProgress(const FString& PartialOutput);

	/** Start a new streaming response message */
	void StartStreamingResponse();

	/** Finalize streaming response */
	void FinalizeStreamingResponse();

	/** Refresh project context */
	void RefreshProjectContext();

	/** Get project context summary for status bar */
	FText GetProjectContextSummary() const;
};
