// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Brushes/SlateDynamicImageBrush.h"

class SMultiLineEditableTextBox;
class SHorizontalBox;
class SScrollBox;

DECLARE_DELEGATE(FOnInputAction)
DECLARE_DELEGATE_OneParam(FOnTextChangedEvent, const FString&)
DECLARE_DELEGATE_OneParam(FOnImagesChanged, const TArray<FString>&)

/**
 * Input area widget for Claude Editor
 * Handles multi-line text input with paste, send/cancel buttons, and multi-image attachment
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
		SLATE_EVENT(FOnImagesChanged, OnImagesChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Set the input text */
	void SetText(const FString& NewText);

	/** Get the current input text */
	FString GetText() const;

	/** Clear the input */
	void ClearText();

	/** Check if any images are currently attached */
	bool HasAttachedImages() const;

	/** Get the number of attached images */
	int32 GetAttachedImageCount() const;

	/** Get all attached image file paths */
	TArray<FString> GetAttachedImagePaths() const;

	/** Clear all attached images */
	void ClearAttachedImages();

	/** Remove a specific attached image by index */
	void RemoveAttachedImage(int32 Index);

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

	/** Handle remove image button click for a specific index */
	FReply HandleRemoveImageClicked(int32 Index);

	/** Rebuild the horizontal thumbnail strip from current attached images */
	void RebuildImagePreviewStrip();

	/** Create a dynamic image brush from a PNG file on disk */
	TSharedPtr<FSlateDynamicImageBrush> CreateThumbnailBrush(const FString& FilePath) const;

private:
	TSharedPtr<SMultiLineEditableTextBox> InputTextBox;
	FString CurrentInputText;

	/** Attached image file paths (up to MaxImagesPerMessage) */
	TArray<FString> AttachedImagePaths;

	/** Horizontal thumbnail strip container */
	TSharedPtr<SHorizontalBox> ImagePreviewStrip;

	/** Dynamic brushes for thumbnails (must outlive the SImage widgets) */
	TArray<TSharedPtr<FSlateDynamicImageBrush>> ThumbnailBrushes;

	TAttribute<bool> bIsWaiting;
	FOnInputAction OnSend;
	FOnInputAction OnCancel;
	FOnTextChangedEvent OnTextChangedDelegate;
	FOnImagesChanged OnImagesChangedDelegate;
};
