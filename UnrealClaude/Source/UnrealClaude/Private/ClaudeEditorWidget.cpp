// Copyright Natali Caggiano. All Rights Reserved.

#include "ClaudeEditorWidget.h"
#include "ClaudeCodeRunner.h"
#include "ClaudeSubsystem.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"
#include "ProjectContext.h"
#include "MCP/UnrealClaudeMCPServer.h"
#include "MCP/MCPToolRegistry.h"
#include "Widgets/SClaudeToolbar.h"
#include "Widgets/SClaudeInputArea.h"
#include "Widgets/SMarkdownWidget.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "UnrealClaude"

// ============================================================================
// SRightClickDragBox
// ============================================================================

void SRightClickDragBox::Construct(const FArguments& InArgs)
{
	TargetScrollBox = InArgs._ScrollBox;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

FReply SRightClickDragBox::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bIsDragging = true;
		DragStartMousePos = MouseEvent.GetScreenSpacePosition();

		if (TargetScrollBox.IsValid())
		{
			DragStartScrollOffset = TargetScrollBox->GetScrollOffset();
		}

		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

FReply SRightClickDragBox::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bIsDragging)
	{
		bIsDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply SRightClickDragBox::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsDragging && TargetScrollBox.IsValid())
	{
		FVector2D CurrentMousePos = MouseEvent.GetScreenSpacePosition();
		float DeltaY = DragStartMousePos.Y - CurrentMousePos.Y;

		// Apply delta to scroll offset (moving mouse up scrolls content down, moving down scrolls content up)
		float NewScrollOffset = DragStartScrollOffset + DeltaY;
		TargetScrollBox->SetScrollOffset(NewScrollOffset);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TOptional<EMouseCursor::Type> SRightClickDragBox::GetCursor() const
{
	if (bIsDragging)
	{
		return EMouseCursor::GrabHandClosed;
	}
	return TOptional<EMouseCursor::Type>();
}

FReply SRightClickDragBox::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (TargetScrollBox.IsValid())
	{
		float CurrentOffset = TargetScrollBox->GetScrollOffset();
		float WheelDelta = MouseEvent.GetWheelDelta();
		float ScrollSpeed = 50.0f; // Pixels per wheel tick
		float NewOffset = CurrentOffset - (WheelDelta * ScrollSpeed);
		TargetScrollBox->SetScrollOffset(NewOffset);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// ============================================================================
// SChatMessage
// ============================================================================

void SChatMessage::Construct(const FArguments& InArgs)
{
	bool bIsUser = InArgs._IsUser;
	FString Message = InArgs._Message;
	bUseMarkdown = InArgs._bRenderMarkdown && !bIsUser;  // Only render Markdown for Claude messages
	bSelectionModeAttr = InArgs._bSelectionMode;

	FLinearColor BackgroundColor = bIsUser
		? FLinearColor(0.13f, 0.13f, 0.18f, 1.0f)  // Dark blue-gray for user
		: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);  // Near-black for assistant

	FLinearColor AccentColor = bIsUser
		? FLinearColor(0.3f, 0.5f, 0.9f, 1.0f)   // Blue accent for user
		: FLinearColor(0.6f, 0.4f, 0.2f, 1.0f);   // Warm orange accent for Claude

	FLinearColor TextColor = FLinearColor::White;
	FLinearColor RoleLabelColor = bIsUser
		? FLinearColor(0.4f, 0.6f, 1.0f)   // Light blue
		: FLinearColor(0.9f, 0.6f, 0.3f);  // Warm orange

	FString RoleLabel = bIsUser ? TEXT("> You") : TEXT("Claude");

	// Claude messages render through SMarkdownWidget; user messages stay plain text
	TSharedPtr<SWidget> ContentWidget;
	if (bUseMarkdown)
	{
		SAssignNew(MarkdownWidget, SMarkdownWidget)
			.Text(Message)
			.bDarkTheme(true)
			.bSelectionMode_Lambda([this]() { return bSelectionModeAttr.Get(); });
		ContentWidget = MarkdownWidget;
	}
	else
	{
		// Allow selecting user messages in Plain Text mode
		ContentWidget = SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() { return bSelectionModeAttr.Get() ? 1 : 0; })

			+ SWidgetSwitcher::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Message))
				.TextStyle(FAppStyle::Get(), "NormalText")
				.ColorAndOpacity(FSlateColor(TextColor))
				.AutoWrapText(true)
			]

			+ SWidgetSwitcher::Slot()
			[
				SNew(SMultiLineEditableText)
				.Text(FText::FromString(Message))
				.TextStyle(FAppStyle::Get(), "NormalText")
				.AutoWrapText(true)
				.IsReadOnly(true)
			];
	}

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(AccentColor)
			.Padding(FMargin(1.5f, 0.0f))
			[
				SNullWidget::NullWidget
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.BorderBackgroundColor(BackgroundColor)
			.Padding(FMargin(12.0f, 8.0f, 10.0f, 8.0f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(RoleLabel))
						.TextStyle(FAppStyle::Get(), "SmallText")
						.ColorAndOpacity(FSlateColor(RoleLabelColor))
					]

					// Per-message copy button (on both user and Claude messages)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SCopyButton)
						.OnGetText_Lambda([Message]() { return Message; })
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					ContentWidget.ToSharedRef()
				]
			]
		]
	];
}

// ============================================================================
// SCopyButton
// ============================================================================

void SCopyButton::Construct(const FArguments& InArgs)
{
	OnGetTextDelegate = InArgs._OnGetText;

	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(4.0f, 1.0f))
		.ToolTipText(LOCTEXT("CopyMessageTooltip", "Copy this message to the clipboard"))
		.OnClicked(this, &SCopyButton::OnCopyClicked)
		[
			SAssignNew(Label, STextBlock)
			.Text(LOCTEXT("CopyMessage", "Copy"))
			.TextStyle(FAppStyle::Get(), "SmallText")
			.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.65f)))
		]
	];
}

FReply SCopyButton::OnCopyClicked()
{
	const FString TextToCopy = OnGetTextDelegate.IsBound() ? OnGetTextDelegate.Execute() : FString();
	FPlatformApplicationMisc::ClipboardCopy(*TextToCopy);

	if (Label.IsValid())
	{
		Label->SetText(LOCTEXT("CopiedMessage", "Copied!"));
	}

	// Show the "Copied!" feedback for a short window. Re-clicking extends the deadline
	// rather than stacking timers.
	FeedbackDeadline = FSlateApplication::Get().GetCurrentTime() + 1.5;
	if (!bShowingFeedback)
	{
		bShowingFeedback = true;
		RegisterActiveTimer(0.25f, FWidgetActiveTimerDelegate::CreateSP(this, &SCopyButton::TickFeedback));
	}

	return FReply::Handled();
}

EActiveTimerReturnType SCopyButton::TickFeedback(double InCurrentTime, float InDeltaTime)
{
	if (InCurrentTime < FeedbackDeadline)
	{
		return EActiveTimerReturnType::Continue;
	}

	if (Label.IsValid())
	{
		Label->SetText(LOCTEXT("CopyMessage", "Copy"));
	}
	bShowingFeedback = false;
	return EActiveTimerReturnType::Stop;
}

// ============================================================================
// SClaudeEditorWidget
// ============================================================================

void SClaudeEditorWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildToolbar()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			BuildChatArea()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			BuildInputArea()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildStatusBar()
		]
	];

	if (!IsClaudeAvailable())
	{
		AddMessage(TEXT("⚠️ Claude CLI not found.\n\nPlease install Claude Code:\n  npm install -g @anthropic-ai/claude-code\n\nThen authenticate:\n  claude auth login"), false);
	}
	else
	{
		FString WelcomeMessage = TEXT("👋 Welcome to Unreal Claude!\n\nI'm ready to help with your UE5.7 project. Ask me about:\n• C++ code patterns and best practices\n• Blueprint integration\n• Engine systems (Nanite, Lumen, GAS, etc.)\n• Debugging and optimization\n\n");

		WelcomeMessage += GenerateMCPStatusMessage();

		WelcomeMessage += TEXT("\nType your question below and press Enter or click Send.");
		AddMessage(WelcomeMessage, false);
	}
}

SClaudeEditorWidget::~SClaudeEditorWidget()
{
	// Cancel any in-flight request so the worker thread can't fire callbacks into a destroyed widget
	FClaudeCodeSubsystem::Get().CancelCurrentRequest();
}

TSharedRef<SWidget> SClaudeEditorWidget::BuildToolbar()
{
	return SNew(SClaudeToolbar)
		.bUE57ContextEnabled_Lambda([this]() { return bIncludeUE57Context; })
		.bProjectContextEnabled_Lambda([this]() { return bIncludeProjectContext; })
		.bRestoreEnabled_Lambda([this]() { return FClaudeCodeSubsystem::Get().HasSavedSession(); })
		.bSelectionMode_Lambda([this]() { return bSelectionMode; })
		.OnUE57ContextChanged_Lambda([this](bool bEnabled) { bIncludeUE57Context = bEnabled; })
		.OnProjectContextChanged_Lambda([this](bool bEnabled) { bIncludeProjectContext = bEnabled; })
		.OnSelectionModeChanged_Lambda([this](bool bEnabled) {
				bSelectionMode = bEnabled;
				// Force re-evaluation of dynamic _Lambda attributes that depend on bSelectionMode
				Invalidate(EInvalidateWidgetReason::Layout);
			})
		.OnRefreshContext_Lambda([this]() { RefreshProjectContext(); })
		.OnRestoreSession_Lambda([this]() { RestoreSession(); })
		.OnNewSession_Lambda([this]() { NewSession(); })
		.OnClear_Lambda([this]() { ClearChat(); });
}

TSharedRef<SWidget> SClaudeEditorWidget::BuildChatArea()
{
	TSharedRef<SScrollBox> ScrollBoxRef = SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SAssignNew(ChatMessagesBox, SVerticalBox)
		];
	ChatScrollBox = ScrollBoxRef;

	// SRightClickDragBox sits above the scroll box so it can capture right-drag scroll input
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(4.0f)
		[
			SNew(SRightClickDragBox)
			.ScrollBox(ChatScrollBox)
			[
				ScrollBoxRef
			]
		];
}

TSharedRef<SWidget> SClaudeEditorWidget::BuildInputArea()
{
	SAssignNew(InputArea, SClaudeInputArea)
		.bIsWaiting_Lambda([this]() { return bIsWaitingForResponse; })
		.OnSend_Lambda([this]() { SendMessage(); })
		.OnCancel_Lambda([this]() { CancelRequest(); })
		.OnTextChanged_Lambda([this](const FString& Text) { CurrentInputText = Text; });

	return InputArea.ToSharedRef();
}

TSharedRef<SWidget> SClaudeEditorWidget::BuildStatusBar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.0f, 4.0f))
		[
			SNew(SHorizontalBox)

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

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([]() -> FText {
					FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
					FString HomeDir = FPlatformProcess::UserHomeDir();
					// Strip trailing slashes so the prefix match works regardless of UserHomeDir() format
					while (HomeDir.EndsWith(TEXT("/")))
					{
						HomeDir.LeftChopInline(1);
					}
					if (ProjectPath.StartsWith(HomeDir))
					{
						ProjectPath = TEXT("~") + ProjectPath.RightChop(HomeDir.Len());
					}
					return FText::FromString(ProjectPath);
				})
				.TextStyle(FAppStyle::Get(), "SmallText")
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			]
		];
}

void SClaudeEditorWidget::AddMessage(const FString& Message, bool bIsUser)
{
	// MessageHistory drives RebuildChatDisplay when selection mode toggles
	MessageHistory.Add(TPair<FString, bool>(Message, bIsUser));

	if (ChatMessagesBox.IsValid())
	{
		// Capture scroll position before mutation so we can preserve it (or auto-scroll) after
		bool bWasAtBottom = IsScrolledToBottom();

		if (ChatMessagesBox->NumSlots() > 0)
		{
			ChatMessagesBox->AddSlot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f))
			[
				SNew(SSeparator)
				.ColorAndOpacity(FLinearColor(0.15f, 0.15f, 0.15f, 0.5f))
			];
		}

		ChatMessagesBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(4.0f, 6.0f, 4.0f, 6.0f))
		[
			SNew(SChatMessage)
			.Message(Message)
			.IsUser(bIsUser)
			.bRenderMarkdown(!bIsUser)  // Enable Markdown rendering for Claude messages
			.bSelectionMode_Lambda([this]() { return bSelectionMode; })  // Dynamic: applies to both user and Claude messages
		];

		// Only scroll to bottom if we were already there
		if (bWasAtBottom)
		{
			ScrollToEnd();
		}
	}
}

void SClaudeEditorWidget::RebuildChatMessages()
{
	if (!ChatMessagesBox.IsValid())
	{
		return;
	}

	bool bWasAtBottom = IsScrolledToBottom();

	ChatMessagesBox->ClearChildren();

	for (int32 i = 0; i < MessageHistory.Num(); ++i)
	{
		const FString& Message = MessageHistory[i].Key;
		bool bIsUser = MessageHistory[i].Value;

		if (i > 0)
		{
			ChatMessagesBox->AddSlot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f))
			[
				SNew(SSeparator)
				.ColorAndOpacity(FLinearColor(0.15f, 0.15f, 0.15f, 0.5f))
			];
		}

		ChatMessagesBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(4.0f, 6.0f, 4.0f, 6.0f))
		[
			SNew(SChatMessage)
			.Message(Message)
			.IsUser(bIsUser)
			.bRenderMarkdown(!bIsUser)
			.bSelectionMode_Lambda([this, bIsUser]() { return bSelectionMode && !bIsUser; })
		];
	}

	if (bWasAtBottom)
	{
		ScrollToEnd();
	}
}

void SClaudeEditorWidget::SendMessage()
{
	TArray<FString> ImagePaths;
	if (InputArea.IsValid())
	{
		ImagePaths = InputArea->GetAttachedImagePaths();
	}

	bool bHasText = !CurrentInputText.IsEmpty();
	bool bHasImage = ImagePaths.Num() > 0;

	if ((!bHasText && !bHasImage) || bIsWaitingForResponse)
	{
		return;
	}

	if (!IsClaudeAvailable())
	{
		AddMessage(TEXT("Claude CLI is not available. Please install it first."), false);
		return;
	}

	FString DisplayMessage = bHasText ? CurrentInputText : FString();
	if (bHasImage)
	{
		FString ImageLabel;
		if (ImagePaths.Num() == 1)
		{
			ImageLabel = FString::Printf(TEXT("[Attached image: %s]"), *FPaths::GetCleanFilename(ImagePaths[0]));
		}
		else
		{
			TArray<FString> FileNames;
			for (const FString& Path : ImagePaths)
			{
				FileNames.Add(FPaths::GetCleanFilename(Path));
			}
			ImageLabel = FString::Printf(TEXT("[Attached %d images: %s]"), ImagePaths.Num(), *FString::Join(FileNames, TEXT(" ")));
		}

		if (bHasText)
		{
			DisplayMessage += TEXT("\n") + ImageLabel;
		}
		else
		{
			DisplayMessage = ImageLabel;
		}
	}

	AddMessage(DisplayMessage, true);

	// Image-only sends still need a prompt or the API rejects the request
	FString Prompt = bHasText ? CurrentInputText : TEXT("Please analyze this image.");

	CurrentInputText.Empty();
	if (InputArea.IsValid())
	{
		InputArea->ClearText();
	}

	bIsWaitingForResponse = true;

	StartStreamingResponse();

	FOnClaudeResponse OnComplete;
	OnComplete.BindSP(this, &SClaudeEditorWidget::OnClaudeResponse);

	FClaudePromptOptions Options;
	Options.bIncludeEngineContext = bIncludeUE57Context;
	Options.bIncludeProjectContext = bIncludeProjectContext;
	Options.OnProgress.BindSP(this, &SClaudeEditorWidget::OnClaudeProgress);
	Options.OnStreamEvent.BindSP(this, &SClaudeEditorWidget::OnClaudeStreamEvent);
	Options.AttachedImagePaths = ImagePaths;

	FClaudeCodeSubsystem::Get().SendPrompt(Prompt, OnComplete, Options);
}

void SClaudeEditorWidget::OnClaudeResponse(const FString& Response, bool bSuccess)
{
	bIsWaitingForResponse = false;

	if (bSuccess)
	{
		// If we have a streaming bubble but no progress was received (e.g., image mode
		// suppresses progress callbacks), populate it with the final response so
		// FinalizeStreamingResponse updates the existing bubble instead of leaving "Thinking..."
		if (StreamingResponse.IsEmpty() && StreamingTextBlock.IsValid())
		{
			StreamingResponse = Response;
			CurrentSegmentText = Response;
			if (StreamingTextBlock.IsValid())
			{
				StreamingTextBlock->SetText(FText::FromString(Response));
			}
		}

		FinalizeStreamingResponse();

		// Only add a new bubble if we had no streaming bubble at all
		if (StreamingResponse.IsEmpty())
		{
			AddMessage(Response, false);
		}
	}
	else
	{
		FinalizeStreamingResponse();
		// Only show error message if there's actual error text to display.
		// Refusals produce an empty response (AccumulatedResponseText was cleared)
		// and already show a banner via HandleRefusalEvent, so skip the redundant message.
		if (!Response.IsEmpty())
		{
			AddMessage(FString::Printf(TEXT("Error: %s"), *Response), false);
		}
	}

	StreamingResponse.Empty();
}

void SClaudeEditorWidget::ClearChat()
{
	if (ChatMessagesBox.IsValid())
	{
		ChatMessagesBox->ClearChildren();
	}

	MessageHistory.Empty();
	FClaudeCodeSubsystem::Get().ClearHistory();
	ResetStreamingState();

	AddMessage(TEXT("Chat cleared. Ready for new questions!"), false);
}

void SClaudeEditorWidget::CancelRequest()
{
	FClaudeCodeSubsystem::Get().CancelCurrentRequest();
	bIsWaitingForResponse = false;
	AddMessage(TEXT("Request cancelled."), false);
}

void SClaudeEditorWidget::RestoreSession()
{
	FClaudeCodeSubsystem& Subsystem = FClaudeCodeSubsystem::Get();

	if (Subsystem.LoadSession())
	{
		if (ChatMessagesBox.IsValid())
		{
			ChatMessagesBox->ClearChildren();
		}

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
	if (ChatMessagesBox.IsValid())
	{
		ChatMessagesBox->ClearChildren();
	}

	MessageHistory.Empty();

	FClaudeCodeSubsystem::Get().ClearHistory();

	ResetStreamingState();

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
		const IClaudeRunner* Runner = FClaudeCodeSubsystem::Get().GetRunner();
		if (Runner && Runner->IsSilenceWarningActive())
		{
			const int32 SilenceSec = FMath::FloorToInt(Runner->GetSilenceSeconds());
			return FText::FromString(FString::Printf(
				TEXT("⚠ No output from Claude for %ds — may be stuck in Agent SDK. Cancel and retry if this persists."),
				SilenceSec));
		}

		const double ElapsedSec = FPlatformTime::Seconds() - StreamingStartTime;
		FString StatusStr = FString::Printf(TEXT("● Claude is thinking... %.1fs"), ElapsedSec);

		if (StreamingToolCallCount > 0)
		{
			StatusStr += FString::Printf(TEXT(" | %d tool%s"),
				StreamingToolCallCount, StreamingToolCallCount != 1 ? TEXT("s") : TEXT(""));
		}

		return FText::FromString(StatusStr);
	}

	if (!IsClaudeAvailable())
	{
		return LOCTEXT("StatusUnavailable", "● Claude CLI not found");
	}

	if (!LastResultStats.IsEmpty())
	{
		return FText::FromString(FString::Printf(TEXT("● %s"), *LastResultStats));
	}

	return LOCTEXT("StatusReady", "● Ready");
}

FSlateColor SClaudeEditorWidget::GetStatusColor() const
{
	if (bIsWaitingForResponse)
	{
		const IClaudeRunner* Runner = FClaudeCodeSubsystem::Get().GetRunner();
		if (Runner && Runner->IsSilenceWarningActive())
		{
			return FSlateColor(FLinearColor(1.0f, 0.5f, 0.0f)); // Orange warning
		}
		return FSlateColor(FLinearColor(1.0f, 0.8f, 0.0f)); // Yellow
	}

	if (!IsClaudeAvailable())
	{
		return FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)); // Red
	}

	if (!LastResultStats.IsEmpty())
	{
		return FSlateColor(FLinearColor(0.5f, 0.5f, 0.55f)); // Muted gray for stats
	}

	return FSlateColor(FLinearColor(0.3f, 1.0f, 0.3f)); // Green
}

void SClaudeEditorWidget::ResetStreamingState()
{
	StreamingResponse.Empty();
	CurrentSegmentText.Empty();
	StreamingTextBlock.Reset();
	StreamingContentBox.Reset();
	ToolCallStatusLabels.Empty();
	ToolCallResultTexts.Empty();
	ToolCallExpandables.Empty();
	ToolCallNames.Empty();
	AllTextSegments.Empty();
	TextSegmentBlocks.Empty();
	TextSegmentContainers.Empty();
	ToolGroupExpandArea.Reset();
	ToolGroupInnerBox.Reset();
	ToolGroupSummaryText.Reset();
	ToolGroupCount = 0;
	ToolGroupDoneCount = 0;
	ToolGroupCallIds.Empty();
	StreamingToolCallCount = 0;
	LastResultStats.Empty();
}

void SClaudeEditorWidget::StartStreamingResponse()
{
	ResetStreamingState();
	StreamingStartTime = FPlatformTime::Seconds();

	if (ChatMessagesBox.IsValid())
	{
		if (ChatMessagesBox->NumSlots() > 0)
		{
			ChatMessagesBox->AddSlot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f))
			[
				SNew(SSeparator)
				.ColorAndOpacity(FLinearColor(0.15f, 0.15f, 0.15f, 0.5f))
			];
		}

		// First text segment is wrapped in its own container so it can be swapped for Markdown / code blocks on finalize
		TSharedPtr<SVerticalBox> FirstSegmentContainer;

		// Per-bubble copy text: the button reads this ref so it always copies this
		// response, even after later requests overwrite the streaming members.
		StreamingCopyTextRef = MakeShared<FString>();
		TSharedPtr<FString> CopyTextRef = StreamingCopyTextRef;

		SAssignNew(StreamingContentBox, SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Claude")))
				.TextStyle(FAppStyle::Get(), "SmallText")
				.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.6f, 0.3f)))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SCopyButton)
				.OnGetText_Lambda([CopyTextRef]() { return CopyTextRef.IsValid() ? *CopyTextRef : FString(); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(FirstSegmentContainer, SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(StreamingTextBlock, STextBlock)
				.Text(FText::FromString(TEXT("Thinking...")))
				.TextStyle(FAppStyle::Get(), "NormalText")
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
				.AutoWrapText(true)
			]
		];

		TextSegmentBlocks.Add(StreamingTextBlock);
		TextSegmentContainers.Add(FirstSegmentContainer);

		// Wrap content box in accent bar + border (matches SChatMessage style)
		ChatMessagesBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(4.0f, 6.0f, 4.0f, 6.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.6f, 0.4f, 0.2f, 1.0f))
				.Padding(FMargin(1.5f, 0.0f))
				[
					SNullWidget::NullWidget
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f))
				.Padding(FMargin(12.0f, 8.0f, 10.0f, 8.0f))
				[
					StreamingContentBox.ToSharedRef()
				]
			]
		];
		}

		ScrollToEnd();
	}

	void SClaudeEditorWidget::OnClaudeProgress(const FString& PartialOutput)
	{
		bool bWasAtBottom = IsScrolledToBottom();

		StreamingResponse += PartialOutput;
		CurrentSegmentText += PartialOutput;

		if (StreamingTextBlock.IsValid())
		{
			StreamingTextBlock->SetText(FText::FromString(CurrentSegmentText));
		}

		// Scroll-stickiness: only auto-scroll if the user was already pinned to bottom
		if (bWasAtBottom)
		{
			ScrollToEnd();
		}
	}
void SClaudeEditorWidget::OnClaudeStreamEvent(const FClaudeStreamEvent& Event)
{
	switch (Event.Type)
	{
	case EClaudeStreamEventType::SessionInit:
		UE_LOG(LogUnrealClaude, Log, TEXT("[StreamEvent] SessionInit: session_id=%s"), *Event.SessionId);
		break;

	case EClaudeStreamEventType::TextContent:
		UE_LOG(LogUnrealClaude, Log, TEXT("[StreamEvent] TextContent: %d chars"), Event.Text.Len());
		break;

	case EClaudeStreamEventType::ToolUse:
		UE_LOG(LogUnrealClaude, Log, TEXT("[StreamEvent] ToolUse: %s (id=%s)"), *Event.ToolName, *Event.ToolCallId);
		HandleToolUseEvent(Event);
		break;

	case EClaudeStreamEventType::ToolResult:
		UE_LOG(LogUnrealClaude, Log, TEXT("[StreamEvent] ToolResult: tool_id=%s, %d chars"),
			*Event.ToolCallId, Event.ToolResultContent.Len());
		HandleToolResultEvent(Event);
		break;

	case EClaudeStreamEventType::Result:
		UE_LOG(LogUnrealClaude, Log, TEXT("[StreamEvent] Result: error=%d, duration=%dms, turns=%d, cost=$%.4f"),
			Event.bIsError, Event.DurationMs, Event.NumTurns, Event.TotalCostUsd);
		HandleResultEvent(Event);
		break;

	case EClaudeStreamEventType::Refusal:
		UE_LOG(LogUnrealClaude, Warning, TEXT("[StreamEvent] Refusal: stop_reason=%s"), *Event.StopReason);
		HandleRefusalEvent(Event);
		break;

	default:
		UE_LOG(LogUnrealClaude, Log, TEXT("[StreamEvent] Unknown type: %d"), static_cast<int32>(Event.Type));
		break;
	}
}

void SClaudeEditorWidget::FinalizeStreamingResponse()
{
	AllTextSegments.Add(CurrentSegmentText);

	// Stitch all segments back together so the per-message copy button gets the full response
	FString Rebuilt;
	for (const FString& Segment : AllTextSegments)
	{
		Rebuilt += Segment;
	}
	if (!Rebuilt.IsEmpty())
	{
		StreamingResponse = Rebuilt;
	}

	// Freeze this bubble's copy text so its copy button keeps copying this response
	if (StreamingCopyTextRef.IsValid())
	{
		*StreamingCopyTextRef = StreamingResponse.IsEmpty() ? CurrentSegmentText : StreamingResponse;
	}
	StreamingCopyTextRef.Reset();

	// Render each segment in its own container so text appears before/after tool blocks in the right order
	for (int32 i = 0; i < TextSegmentContainers.Num() && i < AllTextSegments.Num(); ++i)
	{
		if (TextSegmentContainers[i].IsValid() && !AllTextSegments[i].IsEmpty())
		{
			TextSegmentContainers[i]->ClearChildren();
			TextSegmentContainers[i]->AddSlot()
			.AutoHeight()
			[
				SNew(SMarkdownWidget)
				.Text(AllTextSegments[i])
				.bDarkTheme(true)
				.bSelectionMode_Lambda([this]() { return bSelectionMode; })
			];
		}
		else if (TextSegmentContainers[i].IsValid() && AllTextSegments[i].IsEmpty())
		{
			TextSegmentContainers[i]->SetVisibility(EVisibility::Collapsed);
		}
	}

	// StreamingResponse intentionally NOT reset here — OnClaudeResponse reads it after this returns
	StreamingTextBlock.Reset();
	StreamingContentBox.Reset();
	CurrentSegmentText.Empty();
	ToolCallStatusLabels.Empty();
	ToolCallResultTexts.Empty();
	ToolCallExpandables.Empty();
	ToolCallNames.Empty();
	AllTextSegments.Empty();
	TextSegmentBlocks.Empty();
	TextSegmentContainers.Empty();
	ToolGroupExpandArea.Reset();
	ToolGroupInnerBox.Reset();
	ToolGroupSummaryText.Reset();
	ToolGroupCount = 0;
	ToolGroupDoneCount = 0;
	ToolGroupCallIds.Empty();
}

void SClaudeEditorWidget::HandleToolUseEvent(const FClaudeStreamEvent& Event)
{
	if (!StreamingContentBox.IsValid())
	{
		return;
	}

	StreamingToolCallCount++;

	// Tool name is stored by call id so the matching tool_result event can look it back up
	ToolCallNames.Add(Event.ToolCallId, Event.ToolName);

	FString DisplayName = GetDisplayToolName(Event.ToolName);

	// Consecutive tool calls (no intervening text) merge into the same expandable group
	bool bIsConsecutive = CurrentSegmentText.IsEmpty() && ToolGroupInnerBox.IsValid();

	if (!bIsConsecutive)
	{
		// Freeze the current text segment before starting a new tool group
		AllTextSegments.Add(CurrentSegmentText);
		CurrentSegmentText.Empty();

		if (AllTextSegments.Last().IsEmpty() && TextSegmentContainers.Num() > 0)
		{
			TextSegmentContainers.Last()->SetVisibility(EVisibility::Collapsed);
		}

		ToolGroupCount = 0;
		ToolGroupDoneCount = 0;
		ToolGroupCallIds.Empty();

		StreamingContentBox->AddSlot()
		.AutoHeight()
		.Padding(0, 3, 0, 3)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.10f, 0.10f, 0.13f, 1.0f))
			.Padding(FMargin(4.0f, 2.0f))
			[
				SAssignNew(ToolGroupExpandArea, SExpandableArea)
				.InitiallyCollapsed(false)
				.HeaderPadding(FMargin(4.0f, 2.0f))
				.HeaderContent()
				[
					SAssignNew(ToolGroupSummaryText, STextBlock)
					.TextStyle(FAppStyle::Get(), "SmallText")
					.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.55f)))
				]
				.BodyContent()
				[
					SAssignNew(ToolGroupInnerBox, SVerticalBox)
				]
			]
		];

		// New text segment container catches any text that arrives after this tool group
		TSharedPtr<SVerticalBox> NewSegmentContainer;

		StreamingContentBox->AddSlot()
		.AutoHeight()
		[
			SAssignNew(NewSegmentContainer, SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(StreamingTextBlock, STextBlock)
				.Text(FText::GetEmpty())
				.TextStyle(FAppStyle::Get(), "NormalText")
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
				.AutoWrapText(true)
			]
		];

		TextSegmentBlocks.Add(StreamingTextBlock);
		TextSegmentContainers.Add(NewSegmentContainer);
	}
	else
	{
		// Consecutive tool - transition from single to grouped display
		if (ToolGroupCount == 1)
		{
			// Collapse the group (was expanded for single tool)
			if (ToolGroupExpandArea.IsValid())
			{
				ToolGroupExpandArea->SetExpanded(false);
			}

			// Show the first tool's inner status label (was hidden for single-tool display)
			if (ToolGroupCallIds.Num() > 0)
			{
				TSharedPtr<STextBlock>* FirstStatusPtr = ToolCallStatusLabels.Find(ToolGroupCallIds[0]);
				if (FirstStatusPtr && FirstStatusPtr->IsValid())
				{
					(*FirstStatusPtr)->SetVisibility(EVisibility::Visible);
				}
			}
		}
	}

	ToolGroupCount++;
	ToolGroupCallIds.Add(Event.ToolCallId);

	TSharedPtr<STextBlock> StatusLabel;
	TSharedPtr<STextBlock> ResultText;
	TSharedPtr<SExpandableArea> ExpandArea;

	ToolGroupInnerBox->AddSlot()
	.AutoHeight()
	.Padding(FMargin(4.0f, 1.0f, 0.0f, 1.0f))
	[
		SNew(SVerticalBox)

		// Tool status line (hidden when single tool - header shows name instead)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(StatusLabel, STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("> %s..."), *DisplayName)))
			.TextStyle(FAppStyle::Get(), "SmallText")
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.55f)))
			.Visibility(ToolGroupCount == 1 ? EVisibility::Collapsed : EVisibility::Visible)
		]

		// Result expandable (hidden until result arrives)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ExpandArea, SExpandableArea)
			.InitiallyCollapsed(true)
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Result")))
				.TextStyle(FAppStyle::Get(), "SmallText")
				.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)))
			]
			.BodyContent()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.BorderBackgroundColor(FLinearColor(0.06f, 0.06f, 0.06f, 1.0f))
				.Padding(FMargin(8.0f, 6.0f))
				[
					SAssignNew(ResultText, STextBlock)
					.Text(FText::GetEmpty())
					.TextStyle(FAppStyle::Get(), "SmallText")
					.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
					.AutoWrapText(true)
				]
			]
			.Visibility(EVisibility::Collapsed)
		]
	];

	ToolCallStatusLabels.Add(Event.ToolCallId, StatusLabel);
	ToolCallResultTexts.Add(Event.ToolCallId, ResultText);
	ToolCallExpandables.Add(Event.ToolCallId, ExpandArea);

	UpdateToolGroupSummary();

}
void SClaudeEditorWidget::HandleToolResultEvent(const FClaudeStreamEvent& Event)
{
	const FString* ToolNamePtr = ToolCallNames.Find(Event.ToolCallId);
	FString ToolName = ToolNamePtr ? GetDisplayToolName(*ToolNamePtr) : TEXT("Tool");

	TSharedPtr<STextBlock>* StatusLabelPtr = ToolCallStatusLabels.Find(Event.ToolCallId);
	if (StatusLabelPtr && StatusLabelPtr->IsValid())
	{
		(*StatusLabelPtr)->SetText(FText::FromString(FString::Printf(TEXT("✓ %s completed"), *ToolName)));
		(*StatusLabelPtr)->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.75f, 0.3f)));
	}

	// Truncate large tool outputs so the expandable doesn't blow up the widget
	TSharedPtr<STextBlock>* ResultTextPtr = ToolCallResultTexts.Find(Event.ToolCallId);
	if (ResultTextPtr && ResultTextPtr->IsValid())
	{
		FString ResultContent = Event.ToolResultContent;
		if (ResultContent.Len() > 2000)
		{
			ResultContent = ResultContent.Left(2000) + TEXT("\n... (truncated)");
		}
		(*ResultTextPtr)->SetText(FText::FromString(ResultContent));
	}

	TSharedPtr<SExpandableArea>* ExpandPtr = ToolCallExpandables.Find(Event.ToolCallId);
	if (ExpandPtr && ExpandPtr->IsValid())
	{
		(*ExpandPtr)->SetVisibility(EVisibility::Visible);
	}

	ToolGroupDoneCount++;
	UpdateToolGroupSummary();
	}


void SClaudeEditorWidget::HandleResultEvent(const FClaudeStreamEvent& Event)
{
	if (!StreamingContentBox.IsValid())
	{
		return;
	}

	// Collapse the trailing text block if no text was emitted after the last tool group
	if (CurrentSegmentText.IsEmpty() && TextSegmentContainers.Num() > 0)
	{
		TextSegmentContainers.Last()->SetVisibility(EVisibility::Collapsed);
	}

	float DurationSec = Event.DurationMs / 1000.0f;
	FString StatsText = FString::Printf(TEXT("Done in %.1fs"), DurationSec);

	if (Event.NumTurns > 0)
	{
		StatsText += FString::Printf(TEXT(" | %d turn%s"),
			Event.NumTurns, Event.NumTurns != 1 ? TEXT("s") : TEXT(""));
	}

	if (Event.TotalCostUsd > 0.0f)
	{
		StatsText += FString::Printf(TEXT(" | $%.4f"), Event.TotalCostUsd);
	}

	LastResultStats = StatsText;

	StreamingContentBox->AddSlot()
	.AutoHeight()
	.Padding(0, 8, 0, 0)
	[
		SNew(STextBlock)
		.Text(FText::FromString(StatsText))
		.TextStyle(FAppStyle::Get(), "SmallText")
		.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.45f)))
	];
}

void SClaudeEditorWidget::HandleRefusalEvent(const FClaudeStreamEvent& Event)
{
	if (!StreamingContentBox.IsValid())
	{
		return;
	}

	FString RefusalMessage = TEXT("Response refused by content safety filter. Please rephrase your request.");

	StreamingContentBox->AddSlot()
	.AutoHeight()
	.Padding(0, 8, 0, 4)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.BorderBackgroundColor(FLinearColor(0.3f, 0.08f, 0.08f, 1.0f))
		.Padding(FMargin(10.0f, 8.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(RefusalMessage))
			.TextStyle(FAppStyle::Get(), "SmallText")
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.5f, 0.5f)))
			.AutoWrapText(true)
		]
	];
}

FString SClaudeEditorWidget::GetDisplayToolName(const FString& FullToolName)
{
	FString Name = FullToolName;
	// Bridge prefixes every tool with mcp__unrealclaude__unreal_; trim it for the chat label
	Name.RemoveFromStart(TEXT("mcp__unrealclaude__unreal_"));
	return Name;
}

void SClaudeEditorWidget::UpdateToolGroupSummary()
{
	if (!ToolGroupSummaryText.IsValid())
	{
		return;
	}

	if (ToolGroupCount == 1)
	{
		FString DisplayName = TEXT("Tool");
		if (ToolGroupCallIds.Num() > 0)
		{
			const FString* NamePtr = ToolCallNames.Find(ToolGroupCallIds[0]);
			if (NamePtr)
			{
				DisplayName = GetDisplayToolName(*NamePtr);
			}
		}

		if (ToolGroupDoneCount >= 1)
		{
			ToolGroupSummaryText->SetText(FText::FromString(
				FString::Printf(TEXT("✓ %s completed"), *DisplayName)));
			ToolGroupSummaryText->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.3f, 0.75f, 0.3f)));
		}
		else
		{
			ToolGroupSummaryText->SetText(FText::FromString(
				FString::Printf(TEXT("> Using %s..."), *DisplayName)));
		}
	}
	else
	{
		if (ToolGroupDoneCount >= ToolGroupCount)
		{
			ToolGroupSummaryText->SetText(FText::FromString(
				FString::Printf(TEXT("✓ %d tools completed"), ToolGroupCount)));
			ToolGroupSummaryText->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.3f, 0.75f, 0.3f)));
		}
		else
		{
			ToolGroupSummaryText->SetText(FText::FromString(
				FString::Printf(TEXT("> %d tools (%d/%d done)"),
					ToolGroupCount, ToolGroupDoneCount, ToolGroupCount)));
		}
	}
}

void SClaudeEditorWidget::ParseCodeFences(const FString& Input, TArray<TPair<FString, bool>>& OutSections)
{
	OutSections.Empty();

	int32 SearchFrom = 0;
	bool bInCodeBlock = false;
	int32 LastSplitPos = 0;

	while (SearchFrom < Input.Len())
	{
		int32 FencePos = Input.Find(TEXT("```"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
		if (FencePos == INDEX_NONE)
		{
			break;
		}

		if (!bInCodeBlock)
		{
			// Opening fence - text before it is plain text
			FString PlainText = Input.Mid(LastSplitPos, FencePos - LastSplitPos);
			if (!PlainText.IsEmpty())
			{
				OutSections.Add(TPair<FString, bool>(PlainText, false));
			}

			// Skip past the opening fence line (including language tag)
			int32 LineEnd = Input.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FencePos + 3);
			if (LineEnd == INDEX_NONE)
			{
				LineEnd = Input.Len();
			}

			LastSplitPos = LineEnd + 1;
			SearchFrom = LastSplitPos;
			bInCodeBlock = true;
		}
		else
		{
			// Closing fence - text before it is code
			FString CodeText = Input.Mid(LastSplitPos, FencePos - LastSplitPos);
			CodeText.TrimEndInline();
			if (!CodeText.IsEmpty())
			{
				OutSections.Add(TPair<FString, bool>(CodeText, true));
			}

			// Skip past the closing fence
			int32 LineEnd = Input.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FencePos + 3);
			if (LineEnd == INDEX_NONE)
			{
				LastSplitPos = FencePos + 3;
			}
			else
			{
				LastSplitPos = LineEnd + 1;
			}

			SearchFrom = LastSplitPos;
			bInCodeBlock = false;
		}
	}

	// Remaining text after last fence
	if (LastSplitPos < Input.Len())
	{
		FString Remaining = Input.Mid(LastSplitPos);
		if (!Remaining.IsEmpty())
		{
			OutSections.Add(TPair<FString, bool>(Remaining, bInCodeBlock));
		}
	}
}

void SClaudeEditorWidget::ParseAndRenderCodeBlocks()
{
	for (int32 i = 0; i < TextSegmentBlocks.Num() && i < TextSegmentContainers.Num(); ++i)
	{
		TSharedPtr<STextBlock> Block = TextSegmentBlocks[i];
		TSharedPtr<SVerticalBox> Container = TextSegmentContainers[i];

		if (!Block.IsValid() || !Container.IsValid())
		{
			continue;
		}

		FString SegmentText;
		if (i < AllTextSegments.Num())
		{
			SegmentText = AllTextSegments[i];
		}
		else
		{
			SegmentText = Block->GetText().ToString();
		}

		if (!SegmentText.Contains(TEXT("```")))
		{
			continue;
		}

		TArray<TPair<FString, bool>> Sections;
		ParseCodeFences(SegmentText, Sections);

		if (Sections.Num() <= 1)
		{
			continue;
		}

		Container->ClearChildren();

		for (const TPair<FString, bool>& Section : Sections)
		{
			if (Section.Value)
			{
				// Code block: dark background + monospace font
				Container->AddSlot()
				.AutoHeight()
				.Padding(0, 4, 0, 4)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					.BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.06f, 1.0f))
					.Padding(FMargin(10.0f, 8.0f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(Section.Key))
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.85f, 0.75f)))
						.AutoWrapText(true)
					]
				];
			}
			else
			{
				Container->AddSlot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Section.Key))
					.TextStyle(FAppStyle::Get(), "NormalText")
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
					.AutoWrapText(true)
				];
			}
		}
	}
}

void SClaudeEditorWidget::AppendToLastResponse(const FString& Text)
{
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

FString SClaudeEditorWidget::GenerateMCPStatusMessage() const
{
	FString StatusMessage = TEXT("─────────────────────────────────\n");
	StatusMessage += TEXT("MCP Tool Status:\n");

	// Check module availability first to avoid race conditions during startup
	if (!FUnrealClaudeModule::IsAvailable())
	{
		StatusMessage += TEXT("❌ MCP Server: MODULE NOT LOADED\n");
		StatusMessage += TEXT("─────────────────────────────────");
		return StatusMessage;
	}

	TSharedPtr<FUnrealClaudeMCPServer> MCPServer = FUnrealClaudeModule::Get().GetMCPServer();

	if (!MCPServer.IsValid() || !MCPServer->IsRunning())
	{
		StatusMessage += TEXT("❌ MCP Server: NOT RUNNING\n\n");
		StatusMessage += TEXT("⚠️ MCP tools are unavailable.\n\n");
		StatusMessage += TEXT("Troubleshooting:\n");
		StatusMessage += TEXT("  • Check Output Log for MCP errors\n");
		StatusMessage += TEXT("  • Run: npm install in Resources/mcp-bridge\n");
		StatusMessage += FString::Printf(TEXT("  • Verify port %d is available\n"), UnrealClaudeConstants::MCPServer::DefaultPort);
		StatusMessage += TEXT("─────────────────────────────────");
		return StatusMessage;
	}

	TSharedPtr<FMCPToolRegistry> ToolRegistry = MCPServer->GetToolRegistry();
	if (!ToolRegistry.IsValid())
	{
		StatusMessage += TEXT("❌ Tool Registry: NOT INITIALIZED\n");
		StatusMessage += TEXT("─────────────────────────────────");
		return StatusMessage;
	}

	TArray<FMCPToolInfo> RegisteredTools = ToolRegistry->GetAllTools();

	// Build a set for O(1) lookup instead of N*M Contains scans below
	TSet<FString> RegisteredToolNames;
	for (const FMCPToolInfo& Tool : RegisteredTools)
	{
		RegisteredToolNames.Add(Tool.Name);
	}

	const TArray<FString>& ExpectedTools = UnrealClaudeConstants::MCPServer::ExpectedTools;

	int32 AvailableCount = 0;
	TArray<FString> MissingTools;

	for (const FString& ToolName : ExpectedTools)
	{
		if (RegisteredToolNames.Contains(ToolName))
		{
			AvailableCount++;
		}
		else
		{
			MissingTools.Add(ToolName);
		}
	}

	if (MissingTools.Num() == 0)
	{
		// "User-visible" count reflects what the bridge advertises to MCP clients (12 simple
		// tools + the unreal_ue mega-router + 2 bridge helpers = 15). The other 13 backend
		// handlers are routed through unreal_ue (7 mega tools) or hidden infrastructure
		// (9: task_*, execute_script, cleanup_scripts, get_script_history, run_console_command).
		StatusMessage += FString::Printf(TEXT("  ✓ All 15 user-visible tools available (%d backend handlers, 7 routed via unreal_ue, 9 hidden infrastructure)\n"), AvailableCount);
	}
	else
	{
		StatusMessage += FString::Printf(TEXT("  ⚠️ %d/%d backend handlers available (15 user-visible when healthy)\n"), AvailableCount, ExpectedTools.Num());
		StatusMessage += TEXT("\n⚠️ Missing tools:\n");
		for (const FString& ToolName : MissingTools)
		{
			StatusMessage += FString::Printf(TEXT("  ✗ %s\n"), *ToolName);
		}
		StatusMessage += TEXT("\nCheck Output Log for details.\n");
	}

	StatusMessage += TEXT("─────────────────────────────────");

	return StatusMessage;
}

bool SClaudeEditorWidget::IsScrolledToBottom() const
{
	if (!ChatScrollBox.IsValid())
	{
		return true;  // Default to true so initial messages scroll to bottom
	}

	float CurrentOffset = ChatScrollBox->GetScrollOffset();
	float EndOffset = ChatScrollBox->GetScrollOffsetOfEnd();

	// "At bottom" tolerance: within 50px of end so slow streaming + sub-pixel rounding don't break stickiness
	return (EndOffset - CurrentOffset) <= 50.0f;
}

void SClaudeEditorWidget::ScrollToEnd()
{
	if (ChatScrollBox.IsValid())
	{
		ChatScrollBox->ScrollToEnd();
	}
}

#undef LOCTEXT_NAMESPACE
