// Copyright Your Name. All Rights Reserved.

#include "ScriptPermissionDialog.h"
#include "UnrealClaudeModule.h"

#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Framework/Application/SlateApplication.h"

bool FScriptPermissionDialog::Show(
	const FString& ScriptPreview,
	EScriptType Type,
	const FString& Description)
{
	// Must be on game thread for Slate
	if (!IsInGameThread())
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Permission dialog must be shown on game thread"));
		return false;
	}

	// Use shared pointer for approval state to avoid stack variable capture issues
	TSharedPtr<bool> bApprovedPtr = MakeShared<bool>(false);

	// Truncate preview if too long
	FString DisplayPreview = ScriptPreview;
	if (DisplayPreview.Len() > MaxPreviewLength)
	{
		DisplayPreview = DisplayPreview.Left(MaxPreviewLength) + TEXT("\n\n... (truncated)");
	}

	FString TypeStr = ScriptTypeToString(Type);

	// Create the modal window
	TSharedPtr<SWindow> PermissionWindow = SNew(SWindow)
		.Title(FText::FromString(FString::Printf(TEXT("Execute %s Script?"), *TypeStr.ToUpper())))
		.ClientSize(FVector2D(DialogWidth, DialogHeight))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	// Build and set content
	PermissionWindow->SetContent(
		BuildContent(DisplayPreview, TypeStr, Description, bApprovedPtr, PermissionWindow)
	);

	// Show as modal and wait for user response
	FSlateApplication::Get().AddModalWindow(PermissionWindow.ToSharedRef(), nullptr);

	return *bApprovedPtr;
}

TSharedRef<SWidget> FScriptPermissionDialog::BuildContent(
	const FString& DisplayPreview,
	const FString& TypeStr,
	const FString& Description,
	TSharedPtr<bool> bApprovedPtr,
	TSharedPtr<SWindow> Window)
{
	return SNew(SVerticalBox)
		// Header with warning and description
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildHeaderSection(TypeStr, Description)
		]
		// Script preview
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(10)
		[
			BuildPreviewSection(DisplayPreview)
		]
		// Buttons
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(10)
		[
			BuildButtonBar(bApprovedPtr, Window)
		];
}

TSharedRef<SVerticalBox> FScriptPermissionDialog::BuildHeaderSection(
	const FString& TypeStr,
	const FString& Description)
{
	return SNew(SVerticalBox)
		// Warning header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("⚠️ Claude wants to execute a script. Review the code below:")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
		]
		// Description
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("Description: %s"), *Description)))
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> FScriptPermissionDialog::BuildPreviewSection(const FString& DisplayPreview)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(DisplayPreview))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.AutoWrapText(false)
			]
		];
}

TSharedRef<SWidget> FScriptPermissionDialog::BuildButtonBar(
	TSharedPtr<bool> bApprovedPtr,
	TSharedPtr<SWindow> Window)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 10, 0)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Allow")))
			.OnClicked_Lambda([bApprovedPtr, Window]() {
				*bApprovedPtr = true;
				if (Window.IsValid())
				{
					Window->RequestDestroyWindow();
				}
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Deny")))
			.OnClicked_Lambda([bApprovedPtr, Window]() {
				*bApprovedPtr = false;
				if (Window.IsValid())
				{
					Window->RequestDestroyWindow();
				}
				return FReply::Handled();
			})
		];
}
