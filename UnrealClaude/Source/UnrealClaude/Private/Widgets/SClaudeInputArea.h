// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Brushes/SlateDynamicImageBrush.h"

class SMultiLineEditableTextBox;
class SHorizontalBox;
class STextBlock;
class SImage;

DECLARE_DELEGATE(FOnInputAction)
DECLARE_DELEGATE_OneParam(FOnTextChangedEvent, const FString&)
DECLARE_DELEGATE_OneParam(FOnImageAttached, const FString&)

/**
 * Input area widget for Claude Editor
 * Handles multi-line text input with paste, send/cancel buttons, and image attachment
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
		SLATE_EVENT(FOnImageAttached, OnImageAttached)
		SLATE_EVENT(FOnInputAction, OnImageRemoved)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Set the input text */
	void SetText(const FString& NewText);

	/** Get the current input text */
	FString GetText() const;

	/** Clear the input */
	void ClearText();

	/** Check if an image is currently attached */
	bool HasAttachedImage() const;

	/** Get the attached image file path */
	FString GetAttachedImagePath() const;

	/** Clear the attached image */
	void ClearAttachedImage();

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

	/** Try to paste an image from clipboard. Returns true if image was found and attached. */
	bool TryPasteImageFromClipboard();

	/** Handle remove image button click */
	FReply HandleRemoveImageClicked();

	/** Show the image preview row with thumbnail loaded from the saved PNG */
	void ShowImagePreview(const FString& FilePath);

	/** Hide the image preview row and release thumbnail brush */
	void HideImagePreview();

	/** Create a dynamic image brush from a PNG file on disk */
	TSharedPtr<FSlateDynamicImageBrush> CreateThumbnailBrush(const FString& FilePath) const;

private:
	TSharedPtr<SMultiLineEditableTextBox> InputTextBox;
	FString CurrentInputText;

	/** Attached image file path */
	FString AttachedImagePath;

	/** Image preview row (collapsed when no image) */
	TSharedPtr<SHorizontalBox> ImagePreviewRow;

	/** Image file name display */
	TSharedPtr<STextBlock> ImageFileNameText;

	/** Thumbnail image widget */
	TSharedPtr<SImage> ThumbnailImage;

	/** Dynamic brush for the thumbnail (must outlive the SImage) */
	TSharedPtr<FSlateDynamicImageBrush> ThumbnailBrush;

	TAttribute<bool> bIsWaiting;
	FOnInputAction OnSend;
	FOnInputAction OnCancel;
	FOnTextChangedEvent OnTextChangedDelegate;
	FOnImageAttached OnImageAttachedDelegate;
	FOnInputAction OnImageRemovedDelegate;
};
