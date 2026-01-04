// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SMultiLineEditableTextBox;

DECLARE_DELEGATE(FOnInputAction)
DECLARE_DELEGATE_OneParam(FOnTextChangedEvent, const FString&)

/**
 * Input area widget for Claude Editor
 * Handles multi-line text input with paste, send/cancel buttons
 */
class SClaudeInputArea : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClaudeInputArea)
		: _bIsWaiting(false)
	{}
		SLATE_ATTRIBUTE(bool, bIsWaiting)
		SLATE_EVENT(FOnInputAction, OnSend)
		SLATE_EVENT(FOnInputAction, OnCancel)
		SLATE_EVENT(FOnTextChangedEvent, OnTextChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Set the input text */
	void SetText(const FString& NewText);

	/** Get the current input text */
	FString GetText() const;

	/** Clear the input */
	void ClearText();

private:
	/** Handle key down in input box */
	FReply OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/** Handle text change */
	void HandleTextChanged(const FText& NewText);

	/** Handle text committed */
	void HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

	/** Handle paste button click */
	FReply HandlePasteClicked();

	/** Handle send/cancel button click */
	FReply HandleSendCancelClicked();

private:
	TSharedPtr<SMultiLineEditableTextBox> InputTextBox;
	FString CurrentInputText;

	TAttribute<bool> bIsWaiting;
	FOnInputAction OnSend;
	FOnInputAction OnCancel;
	FOnTextChangedEvent OnTextChangedDelegate;
};
