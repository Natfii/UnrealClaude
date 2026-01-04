// Copyright Your Name. All Rights Reserved.

#include "SClaudeToolbar.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "UnrealClaude"

void SClaudeToolbar::Construct(const FArguments& InArgs)
{
	bUE57ContextEnabled = InArgs._bUE57ContextEnabled;
	bProjectContextEnabled = InArgs._bProjectContextEnabled;
	bRestoreEnabled = InArgs._bRestoreEnabled;
	OnUE57ContextChanged = InArgs._OnUE57ContextChanged;
	OnProjectContextChanged = InArgs._OnProjectContextChanged;
	OnRefreshContext = InArgs._OnRefreshContext;
	OnRestoreSession = InArgs._OnRestoreSession;
	OnNewSession = InArgs._OnNewSession;
	OnClear = InArgs._OnClear;
	OnCopyLast = InArgs._OnCopyLast;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.0f, 4.0f))
		[
			SNew(SHorizontalBox)

			// Title
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "Claude Assistant"))
				.TextStyle(FAppStyle::Get(), "LargeText")
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]

			// UE5.7 Context checkbox
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bUE57ContextEnabled.Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { OnUE57ContextChanged.ExecuteIfBound(NewState == ECheckBoxState::Checked); })
				.ToolTipText(LOCTEXT("UE57ContextTip", "Include Unreal Engine 5.7 context in prompts"))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("UE57Context", "UE5.7 Context"))
				]
			]

			// Project Context checkbox
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bProjectContextEnabled.Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { OnProjectContextChanged.ExecuteIfBound(NewState == ECheckBoxState::Checked); })
				.ToolTipText(LOCTEXT("ProjectContextTip", "Include project source files and level actors in prompts"))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ProjectContext", "Project Context"))
				]
			]

			// Refresh Context button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshContext", "Refresh Context"))
				.OnClicked_Lambda([this]() { OnRefreshContext.ExecuteIfBound(); return FReply::Handled(); })
				.ToolTipText(LOCTEXT("RefreshContextTip", "Refresh project context (source files, classes, level actors)"))
			]

			// Restore Context button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RestoreContext", "Restore Context"))
				.OnClicked_Lambda([this]() { OnRestoreSession.ExecuteIfBound(); return FReply::Handled(); })
				.ToolTipText(LOCTEXT("RestoreContextTip", "Restore previous session context from disk"))
				.IsEnabled_Lambda([this]() { return bRestoreEnabled.Get(); })
			]

			// New Session button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("NewSession", "New Session"))
				.OnClicked_Lambda([this]() { OnNewSession.ExecuteIfBound(); return FReply::Handled(); })
				.ToolTipText(LOCTEXT("NewSessionTip", "Start a new session (clears history)"))
			]

			// Clear button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Clear", "Clear"))
				.OnClicked_Lambda([this]() { OnClear.ExecuteIfBound(); return FReply::Handled(); })
				.ToolTipText(LOCTEXT("ClearTip", "Clear chat display"))
			]

			// Copy button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Copy", "Copy Last"))
				.OnClicked_Lambda([this]() { OnCopyLast.ExecuteIfBound(); return FReply::Handled(); })
				.ToolTipText(LOCTEXT("CopyTip", "Copy last response to clipboard"))
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
