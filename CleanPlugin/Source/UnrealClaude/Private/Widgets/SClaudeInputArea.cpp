// Copyright Your Name. All Rights Reserved.

#include "SClaudeInputArea.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "UnrealClaude"

void SClaudeInputArea::Construct(const FArguments& InArgs)
{
	bIsWaiting = InArgs._bIsWaiting;
	OnSend = InArgs._OnSend;
	OnCancel = InArgs._OnCancel;
	OnTextChangedDelegate = InArgs._OnTextChanged;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Input row
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300.0f)
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
					.ToolTipText(LOCTEXT("PasteTip", "Paste text from clipboard (supports large text)"))
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
}

FReply SClaudeInputArea::OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
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

#undef LOCTEXT_NAMESPACE
