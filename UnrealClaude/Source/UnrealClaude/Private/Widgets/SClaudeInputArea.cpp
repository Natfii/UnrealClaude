// Copyright Natali Caggiano. All Rights Reserved.

#include "SClaudeInputArea.h"
#include "ClipboardImageUtils.h"
#include "UnrealClaudeConstants.h"
#include "UnrealClaudeModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Styling/AppStyle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/FileHelper.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"

#define LOCTEXT_NAMESPACE "UnrealClaude"

void SClaudeInputArea::Construct(const FArguments& InArgs)
{
	bIsWaiting = InArgs._bIsWaiting;
	OnSend = InArgs._OnSend;
	OnCancel = InArgs._OnCancel;
	OnTextChangedDelegate = InArgs._OnTextChanged;
	OnImagesChangedDelegate = InArgs._OnImagesChanged;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Image preview strip (starts collapsed, horizontal scroll)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SAssignNew(ImagePreviewStrip, SHorizontalBox)
			.Visibility(EVisibility::Collapsed)
		]

		// Input row
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			// Input text box with scroll support
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBox)
				.MinDesiredHeight(60.0f)
				.MaxDesiredHeight(300.0f)
				[
					SNew(SScrollBox)
					.Orientation(Orient_Vertical)
					+ SScrollBox::Slot()
					[
						SAssignNew(InputTextBox, SMultiLineEditableTextBox)
						.HintText(LOCTEXT("InputHint", "Ask Claude about Unreal Engine 5.7... (Shift+Enter for newline)"))
						.AutoWrapText(true)
						.AllowMultiLine(true)
						.OnTextChanged(this, &SClaudeInputArea::HandleTextChanged)
						.OnTextCommitted(this, &SClaudeInputArea::HandleTextCommitted)
						.OnKeyDownHandler(this, &SClaudeInputArea::OnInputKeyDown)
						.IsEnabled_Lambda([this]() { return !bIsWaiting.Get(); })
					]
				]
			]

			// Buttons column
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Bottom)
			[
				SNew(SVerticalBox)

				// Paste from clipboard button
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Paste", "Paste"))
					.OnClicked(this, &SClaudeInputArea::HandlePasteClicked)
					.ToolTipText(LOCTEXT("PasteTip", "Paste text or image from clipboard"))
					.IsEnabled_Lambda([this]() { return !bIsWaiting.Get(); })
				]

				// Send/Cancel button
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.Text_Lambda([this]() { return bIsWaiting.Get() ? LOCTEXT("Cancel", "Cancel") : LOCTEXT("Send", "Send"); })
					.OnClicked(this, &SClaudeInputArea::HandleSendCancelClicked)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				]
			]
		]

		// Character count indicator
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				int32 CharCount = CurrentInputText.Len();
				if (CharCount > 0)
				{
					return FText::Format(LOCTEXT("CharCount", "{0} chars"), FText::AsNumber(CharCount));
				}
				return FText::GetEmpty();
			})
			.TextStyle(FAppStyle::Get(), "SmallText")
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
		]
	];
}

void SClaudeInputArea::SetText(const FString& NewText)
{
	CurrentInputText = NewText;
	if (InputTextBox.IsValid())
	{
		InputTextBox->SetText(FText::FromString(NewText));
	}
}

FString SClaudeInputArea::GetText() const
{
	return CurrentInputText;
}

void SClaudeInputArea::ClearText()
{
	CurrentInputText.Empty();
	if (InputTextBox.IsValid())
	{
		InputTextBox->SetText(FText::GetEmpty());
	}
	ClearAttachedImages();
}

bool SClaudeInputArea::HasAttachedImages() const
{
	return AttachedImagePaths.Num() > 0;
}

int32 SClaudeInputArea::GetAttachedImageCount() const
{
	return AttachedImagePaths.Num();
}

TArray<FString> SClaudeInputArea::GetAttachedImagePaths() const
{
	return AttachedImagePaths;
}

void SClaudeInputArea::ClearAttachedImages()
{
	AttachedImagePaths.Empty();
	ThumbnailBrushes.Empty();
	RebuildImagePreviewStrip();
}

void SClaudeInputArea::RemoveAttachedImage(int32 Index)
{
	if (AttachedImagePaths.IsValidIndex(Index))
	{
		AttachedImagePaths.RemoveAt(Index);
		ThumbnailBrushes.RemoveAt(Index);
		RebuildImagePreviewStrip();
		OnImagesChangedDelegate.ExecuteIfBound(AttachedImagePaths);
	}
}

FReply SClaudeInputArea::OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Ctrl+V: try image paste first, fall back to text paste
	if (InKeyEvent.GetKey() == EKeys::V && InKeyEvent.IsControlDown())
	{
		if (TryPasteImageFromClipboard())
		{
			return FReply::Handled();
		}
		// Return unhandled to let default text paste proceed
		return FReply::Unhandled();
	}

	// Enter (without Shift) to send
	// Shift+Enter allows newline
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		if (!InKeyEvent.IsShiftDown())
		{
			OnSend.ExecuteIfBound();
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SClaudeInputArea::HandleTextChanged(const FText& NewText)
{
	CurrentInputText = NewText.ToString();
	OnTextChangedDelegate.ExecuteIfBound(CurrentInputText);
}

void SClaudeInputArea::HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	// Don't send on commit - use explicit Enter key handling
}

FReply SClaudeInputArea::HandlePasteClicked()
{
	// Try image paste first
	if (TryPasteImageFromClipboard())
	{
		return FReply::Handled();
	}

	// Fall back to text paste
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	if (!ClipboardText.IsEmpty() && InputTextBox.IsValid())
	{
		// Append to existing text
		FString NewText = CurrentInputText + ClipboardText;
		SetText(NewText);
	}
	return FReply::Handled();
}

FReply SClaudeInputArea::HandleSendCancelClicked()
{
	if (bIsWaiting.Get())
	{
		OnCancel.ExecuteIfBound();
	}
	else
	{
		OnSend.ExecuteIfBound();
	}
	return FReply::Handled();
}

bool SClaudeInputArea::TryPasteImageFromClipboard()
{
	using namespace UnrealClaudeConstants::ClipboardImage;

	if (!FClipboardImageUtils::ClipboardHasImage())
	{
		return false;
	}

	// Reject if at max image count
	if (AttachedImagePaths.Num() >= MaxImagesPerMessage)
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("Image paste rejected: already at max (%d images)"), MaxImagesPerMessage);
		return false;
	}

	// Clean up old screenshots before saving a new one
	FString ScreenshotDir = FClipboardImageUtils::GetScreenshotDirectory();
	FClipboardImageUtils::CleanupOldScreenshots(
		ScreenshotDir,
		UnrealClaudeConstants::ClipboardImage::MaxScreenshotAgeSeconds);

	FString SavedPath;
	if (!FClipboardImageUtils::SaveClipboardImageToFile(SavedPath, ScreenshotDir))
	{
		return false;
	}

	AttachedImagePaths.Add(SavedPath);
	ThumbnailBrushes.Add(CreateThumbnailBrush(SavedPath));
	RebuildImagePreviewStrip();

	OnImagesChangedDelegate.ExecuteIfBound(AttachedImagePaths);
	return true;
}

FReply SClaudeInputArea::HandleRemoveImageClicked(int32 Index)
{
	RemoveAttachedImage(Index);
	return FReply::Handled();
}

void SClaudeInputArea::RebuildImagePreviewStrip()
{
	using namespace UnrealClaudeConstants::ClipboardImage;

	if (!ImagePreviewStrip.IsValid())
	{
		return;
	}

	ImagePreviewStrip->ClearChildren();

	if (AttachedImagePaths.Num() == 0)
	{
		ImagePreviewStrip->SetVisibility(EVisibility::Collapsed);
		return;
	}

	ImagePreviewStrip->SetVisibility(EVisibility::Visible);

	// Add a thumbnail slot for each attached image
	for (int32 i = 0; i < AttachedImagePaths.Num(); ++i)
	{
		const FString& ImagePath = AttachedImagePaths[i];
		FString FileName = FPaths::GetCleanFilename(ImagePath);

		ImagePreviewStrip->AddSlot()
		.AutoWidth()
		.Padding(i > 0 ? ThumbnailSpacing : 0.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(ThumbnailSize)
			.HeightOverride(ThumbnailSize)
			.ToolTipText(FText::FromString(FileName))
			[
				SNew(SOverlay)

				// Layer 0: thumbnail image
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew(SImage)
						.Image(ThumbnailBrushes.IsValidIndex(i) && ThumbnailBrushes[i].IsValid()
							? ThumbnailBrushes[i].Get() : nullptr)
					]
				]

				// Layer 1: remove button (top-right)
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				[
					SNew(SButton)
					.Text(LOCTEXT("RemoveImageX", "X"))
					.OnClicked_Lambda([this, Index = i]()
					{
						return HandleRemoveImageClicked(Index);
					})
					.ToolTipText(LOCTEXT("RemoveImageTip", "Remove this image"))
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				]
			]
		];
	}

	// Add count label after thumbnails
	ImagePreviewStrip->AddSlot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.Padding(ThumbnailSpacing, 0.0f, 0.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("ImageCount", "{0}/{1}"),
			FText::AsNumber(AttachedImagePaths.Num()),
			FText::AsNumber(MaxImagesPerMessage)))
		.TextStyle(FAppStyle::Get(), "SmallText")
		.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
	];
}

TSharedPtr<FSlateDynamicImageBrush> SClaudeInputArea::CreateThumbnailBrush(const FString& FilePath) const
{
	// Load PNG file from disk
	TArray<uint8> PngData;
	if (!FFileHelper::LoadFileToArray(PngData, *FilePath))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to load image for thumbnail: %s"), *FilePath);
		return nullptr;
	}

	// Decompress PNG to raw pixels
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (!ImageWrapper.IsValid())
	{
		return nullptr;
	}

	if (!ImageWrapper->SetCompressed(PngData.GetData(), PngData.Num()))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to decompress PNG for thumbnail"));
		return nullptr;
	}

	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to get raw pixel data for thumbnail"));
		return nullptr;
	}

	const int32 Width = ImageWrapper->GetWidth();
	const int32 Height = ImageWrapper->GetHeight();

	if (Width <= 0 || Height <= 0)
	{
		return nullptr;
	}

	// Create a dynamic brush from the raw pixel data
	FName BrushName = FName(*FString::Printf(TEXT("ClipboardThumb_%s"), *FPaths::GetBaseFilename(FilePath)));
	TSharedPtr<FSlateDynamicImageBrush> Brush = FSlateDynamicImageBrush::CreateWithImageData(
		BrushName,
		FVector2D(Width, Height),
		TArray<uint8>(RawData));

	return Brush;
}

#undef LOCTEXT_NAMESPACE
