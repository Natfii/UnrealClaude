// Copyright Your Name. All Rights Reserved.

#include "ClaudeEditorWidget.h"
#include "ClaudeCodeRunner.h"
#include "ClaudeSubsystem.h"
#include "UnrealClaudeModule.h"
#include "ProjectContext.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "UnrealClaude"

// ============================================================================
// SChatMessage
// ============================================================================

void SChatMessage::Construct(const FArguments& InArgs)
{
	bool bIsUser = InArgs._IsUser;
	FString Message = InArgs._Message;
	
	// Different colors for user vs assistant
	FLinearColor BackgroundColor = bIsUser 
		? FLinearColor(0.15f, 0.15f, 0.2f, 1.0f)  // Dark blue for user
		: FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);   // Dark gray for assistant
	
	FLinearColor TextColor = FLinearColor::White;
	
	FString RoleLabel = bIsUser ? TEXT("You") : TEXT("Claude");
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.BorderBackgroundColor(BackgroundColor)
		.Padding(FMargin(10.0f, 8.0f))
		[
			SNew(SVerticalBox)
			
			// Role label
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(FText::FromString(RoleLabel))
				.TextStyle(FAppStyle::Get(), "SmallText")
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
			
			// Message content
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Message))
				.TextStyle(FAppStyle::Get(), "NormalText")
				.ColorAndOpacity(FSlateColor(TextColor))
				.AutoWrapText(true)
			]
		]
	];
}

// ============================================================================
// SClaudeEditorWidget
// ============================================================================

void SClaudeEditorWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		
		// Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildToolbar()
		]
		
		// Separator
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]
		
		// Chat area (fills remaining space)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			BuildChatArea()
		]
		
		// Separator
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]
		
		// Input area
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			BuildInputArea()
		]
		
		// Status bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildStatusBar()
		]
	];
	
	// Check Claude availability on startup
	if (!IsClaudeAvailable())
	{
		AddMessage(TEXT("⚠️ Claude CLI not found.\n\nPlease install Claude Code:\n  npm install -g @anthropic-ai/claude-code\n\nThen authenticate:\n  claude auth login"), false);
	}
	else
	{
		AddMessage(TEXT("👋 Welcome to Unreal Claude!\n\nI'm ready to help with your UE5.7 project. Ask me about:\n• C++ code patterns and best practices\n• Blueprint integration\n• Engine systems (Nanite, Lumen, GAS, etc.)\n• Debugging and optimization\n\nType your question below and press Enter or click Send."), false);
	}
}

SClaudeEditorWidget::~SClaudeEditorWidget()
{
	// Cancel any pending requests
	FClaudeCodeSubsystem::Get().CancelCurrentRequest();
}

TSharedRef<SWidget> SClaudeEditorWidget::BuildToolbar()
{
	return SNew(SBorder)
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
				.IsChecked_Lambda([this]() { return bIncludeUE57Context ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bIncludeUE57Context = (NewState == ECheckBoxState::Checked); })
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
				.IsChecked_Lambda([this]() { return bIncludeProjectContext ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bIncludeProjectContext = (NewState == ECheckBoxState::Checked); })
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
				.OnClicked_Lambda([this]() { RefreshProjectContext(); return FReply::Handled(); })
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
				.OnClicked_Lambda([this]() { RestoreSession(); return FReply::Handled(); })
				.ToolTipText(LOCTEXT("RestoreContextTip", "Restore previous session context from disk"))
				.IsEnabled_Lambda([this]() { return FClaudeCodeSubsystem::Get().HasSavedSession(); })
			]

			// New Session button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("NewSession", "New Session"))
				.OnClicked_Lambda([this]() { NewSession(); return FReply::Handled(); })
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
				.OnClicked_Lambda([this]() { ClearChat(); return FReply::Handled(); })
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
				.OnClicked_Lambda([this]() { CopyToClipboard(); return FReply::Handled(); })
				.ToolTipText(LOCTEXT("CopyTip", "Copy last response to clipboard"))
			]
		];
}

TSharedRef<SWidget> SClaudeEditorWidget::BuildChatArea()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(4.0f)
		[
			SAssignNew(ChatScrollBox, SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(ChatMessagesBox, SVerticalBox)
			]
		];
}

TSharedRef<SWidget> SClaudeEditorWidget::BuildInputArea()
{
	return SNew(SVerticalBox)

		// Input row
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300.0f) // Allow up to 300px for large pastes
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
						.OnTextChanged(this, &SClaudeEditorWidget::OnInputTextChanged)
						.OnTextCommitted(this, &SClaudeEditorWidget::OnInputTextCommitted)
						.OnKeyDownHandler(this, &SClaudeEditorWidget::OnInputKeyDown)
						.IsEnabled_Lambda([this]() { return !bIsWaitingForResponse; })
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
					.OnClicked_Lambda([this]()
					{
						FString ClipboardText;
						FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
						if (!ClipboardText.IsEmpty() && InputTextBox.IsValid())
						{
							// Append to existing text
							FString NewText = CurrentInputText + ClipboardText;
							CurrentInputText = NewText;
							InputTextBox->SetText(FText::FromString(NewText));
						}
						return FReply::Handled();
					})
					.ToolTipText(LOCTEXT("PasteTip", "Paste text from clipboard (supports large text)"))
					.IsEnabled_Lambda([this]() { return !bIsWaitingForResponse; })
				]

				// Send/Cancel button
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.Text_Lambda([this]() { return bIsWaitingForResponse ? LOCTEXT("Cancel", "Cancel") : LOCTEXT("Send", "Send"); })
					.OnClicked_Lambda([this]()
					{
						if (bIsWaitingForResponse)
						{
							CancelRequest();
						}
						else
						{
							SendMessage();
						}
						return FReply::Handled();
					})
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
		];
}

TSharedRef<SWidget> SClaudeEditorWidget::BuildStatusBar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			
			// Status indicator
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SClaudeEditorWidget::GetStatusText)
				.ColorAndOpacity(this, &SClaudeEditorWidget::GetStatusColor)
			]
			
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			
			// Project path
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FPaths::GetProjectFilePath()))
				.TextStyle(FAppStyle::Get(), "SmallText")
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			]
		];
}

void SClaudeEditorWidget::AddMessage(const FString& Message, bool bIsUser)
{
	if (ChatMessagesBox.IsValid())
	{
		ChatMessagesBox->AddSlot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SChatMessage)
			.Message(Message)
			.IsUser(bIsUser)
		];
		
		// Scroll to bottom
		if (ChatScrollBox.IsValid())
		{
			ChatScrollBox->ScrollToEnd();
		}
	}
}

void SClaudeEditorWidget::SendMessage()
{
	if (CurrentInputText.IsEmpty() || bIsWaitingForResponse)
	{
		return;
	}

	if (!IsClaudeAvailable())
	{
		AddMessage(TEXT("Claude CLI is not available. Please install it first."), false);
		return;
	}

	// Add user message to chat
	AddMessage(CurrentInputText, true);

	// Save and clear input
	FString Prompt = CurrentInputText;
	CurrentInputText.Empty();
	if (InputTextBox.IsValid())
	{
		InputTextBox->SetText(FText::GetEmpty());
	}

	// Set waiting state
	bIsWaitingForResponse = true;

	// Start streaming response display
	StartStreamingResponse();

	// Send to Claude with progress callback
	FOnClaudeResponse OnComplete;
	OnComplete.BindSP(this, &SClaudeEditorWidget::OnClaudeResponse);

	FOnClaudeProgress OnProgress;
	OnProgress.BindSP(this, &SClaudeEditorWidget::OnClaudeProgress);

	FClaudeCodeSubsystem::Get().SendPrompt(Prompt, OnComplete, bIncludeUE57Context, OnProgress, bIncludeProjectContext);
}

void SClaudeEditorWidget::OnClaudeResponse(const FString& Response, bool bSuccess)
{
	bIsWaitingForResponse = false;

	// Finalize the streaming response
	FinalizeStreamingResponse();

	if (bSuccess)
	{
		// Use the streamed response if we have one, otherwise use the final response
		LastResponse = StreamingResponse.IsEmpty() ? Response : StreamingResponse;

		// If streaming didn't show anything, add the final response
		if (StreamingResponse.IsEmpty())
		{
			AddMessage(Response, false);
		}
	}
	else
	{
		AddMessage(FString::Printf(TEXT("Error: %s"), *Response), false);
	}

	// Clear streaming state
	StreamingResponse.Empty();
}

void SClaudeEditorWidget::ClearChat()
{
	if (ChatMessagesBox.IsValid())
	{
		ChatMessagesBox->ClearChildren();
	}

	FClaudeCodeSubsystem::Get().ClearHistory();
	LastResponse.Empty();
	StreamingResponse.Empty();

	// Add welcome message again
	AddMessage(TEXT("Chat cleared. Ready for new questions!"), false);
}

void SClaudeEditorWidget::CancelRequest()
{
	FClaudeCodeSubsystem::Get().CancelCurrentRequest();
	bIsWaitingForResponse = false;
	AddMessage(TEXT("Request cancelled."), false);
}

void SClaudeEditorWidget::CopyToClipboard()
{
	if (!LastResponse.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*LastResponse);
		UE_LOG(LogUnrealClaude, Log, TEXT("Copied response to clipboard"));
	}
}

void SClaudeEditorWidget::RestoreSession()
{
	FClaudeCodeSubsystem& Subsystem = FClaudeCodeSubsystem::Get();

	if (Subsystem.LoadSession())
	{
		// Clear current chat display
		if (ChatMessagesBox.IsValid())
		{
			ChatMessagesBox->ClearChildren();
		}

		// Restore messages to chat display
		const TArray<TPair<FString, FString>>& History = Subsystem.GetHistory();

		if (History.Num() > 0)
		{
			AddMessage(TEXT("Previous session restored. Context has been loaded."), false);

			for (const TPair<FString, FString>& Exchange : History)
			{
				AddMessage(Exchange.Key, true);   // User message
				AddMessage(Exchange.Value, false); // Assistant response
			}

			AddMessage(FString::Printf(TEXT("Restored %d previous exchanges. Continue the conversation below."), History.Num()), false);
		}
		else
		{
			AddMessage(TEXT("Session file loaded but contained no messages."), false);
		}
	}
	else
	{
		AddMessage(TEXT("Failed to restore previous session. The file may be corrupted or inaccessible."), false);
	}
}

void SClaudeEditorWidget::NewSession()
{
	// Clear the chat display
	if (ChatMessagesBox.IsValid())
	{
		ChatMessagesBox->ClearChildren();
	}

	// Clear the subsystem history
	FClaudeCodeSubsystem::Get().ClearHistory();

	// Clear local state
	LastResponse.Empty();
	StreamingResponse.Empty();

	// Add welcome message
	AddMessage(TEXT("New session started. Previous context has been cleared."), false);
	AddMessage(TEXT("Ready for new questions!"), false);
}

bool SClaudeEditorWidget::IsClaudeAvailable() const
{
	return FClaudeCodeRunner::IsClaudeAvailable();
}

FText SClaudeEditorWidget::GetStatusText() const
{
	if (bIsWaitingForResponse)
	{
		return LOCTEXT("StatusThinking", "● Claude is thinking...");
	}
	
	if (!IsClaudeAvailable())
	{
		return LOCTEXT("StatusUnavailable", "● Claude CLI not found");
	}
	
	return LOCTEXT("StatusReady", "● Ready");
}

FSlateColor SClaudeEditorWidget::GetStatusColor() const
{
	if (bIsWaitingForResponse)
	{
		return FSlateColor(FLinearColor(1.0f, 0.8f, 0.0f)); // Yellow
	}
	
	if (!IsClaudeAvailable())
	{
		return FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)); // Red
	}
	
	return FSlateColor(FLinearColor(0.3f, 1.0f, 0.3f)); // Green
}

void SClaudeEditorWidget::OnInputTextChanged(const FText& NewText)
{
	CurrentInputText = NewText.ToString();
}

void SClaudeEditorWidget::OnInputTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	// Don't send on commit - use explicit Enter key handling
}

FReply SClaudeEditorWidget::OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Enter (without Shift) or Ctrl+Enter to send
	// Shift+Enter allows newline
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		if (!InKeyEvent.IsShiftDown())
		{
			SendMessage();
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SClaudeEditorWidget::StartStreamingResponse()
{
	StreamingResponse.Empty();

	if (ChatMessagesBox.IsValid())
	{
		// Create a streaming message container with live-updating text
		ChatMessagesBox->AddSlot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))
			.Padding(FMargin(10.0f, 8.0f))
			[
				SNew(SVerticalBox)

				// Role label
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Claude")))
					.TextStyle(FAppStyle::Get(), "SmallText")
					.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
				]

				// Streaming message content
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(StreamingTextBlock, STextBlock)
					.Text(FText::FromString(TEXT("Thinking...")))
					.TextStyle(FAppStyle::Get(), "NormalText")
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
					.AutoWrapText(true)
				]
			]
		];

		// Scroll to bottom
		if (ChatScrollBox.IsValid())
		{
			ChatScrollBox->ScrollToEnd();
		}
	}
}

void SClaudeEditorWidget::OnClaudeProgress(const FString& PartialOutput)
{
	// Append to streaming response
	StreamingResponse += PartialOutput;

	// Update the streaming text block
	if (StreamingTextBlock.IsValid())
	{
		StreamingTextBlock->SetText(FText::FromString(StreamingResponse));
	}

	// Auto-scroll to bottom as content streams in
	if (ChatScrollBox.IsValid())
	{
		ChatScrollBox->ScrollToEnd();
	}
}

void SClaudeEditorWidget::FinalizeStreamingResponse()
{
	// The streaming text block remains as the final response
	// Just update it with the complete text if we have streaming content
	if (StreamingTextBlock.IsValid() && !StreamingResponse.IsEmpty())
	{
		StreamingTextBlock->SetText(FText::FromString(StreamingResponse));
		LastResponse = StreamingResponse;
	}

	// Clear the reference (we don't need to update it anymore)
	StreamingTextBlock.Reset();
}

void SClaudeEditorWidget::AppendToLastResponse(const FString& Text)
{
	// Delegate to OnClaudeProgress for streaming updates
	OnClaudeProgress(Text);
}

void SClaudeEditorWidget::RefreshProjectContext()
{
	AddMessage(TEXT("Refreshing project context..."), false);

	FProjectContextManager::Get().RefreshContext();

	FString Summary = FProjectContextManager::Get().GetContextSummary();
	AddMessage(FString::Printf(TEXT("Project context updated: %s"), *Summary), false);
}

FText SClaudeEditorWidget::GetProjectContextSummary() const
{
	if (FProjectContextManager::Get().HasContext())
	{
		return FText::FromString(FProjectContextManager::Get().GetContextSummary());
	}
	return LOCTEXT("NoContext", "No context gathered");
}

#undef LOCTEXT_NAMESPACE
