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
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "UnrealClaude"

// ============================================================================
// SChatMessage
// ============================================================================

void SChatMessage::Construct(const FArguments& InArgs)
{
	bool bIsUser = InArgs._IsUser;
	FString Message = InArgs._Message;

	// Distinct colors for user vs assistant
	FLinearColor BackgroundColor = bIsUser
		? FLinearColor(0.13f, 0.13f, 0.18f, 1.0f)  // Dark blue-gray for user
		: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);  // Near-black for assistant

	// Accent bar color (left edge indicator like CLI prompt markers)
	FLinearColor AccentColor = bIsUser
		? FLinearColor(0.3f, 0.5f, 0.9f, 1.0f)   // Blue accent for user
		: FLinearColor(0.6f, 0.4f, 0.2f, 1.0f);   // Warm orange accent for Claude

	FLinearColor TextColor = FLinearColor::White;
	FLinearColor RoleLabelColor = bIsUser
		? FLinearColor(0.4f, 0.6f, 1.0f)   // Light blue
		: FLinearColor(0.9f, 0.6f, 0.3f);  // Warm orange

	FString RoleLabel = bIsUser ? TEXT("> You") : TEXT("Claude");

	ChildSlot
	[
		SNew(SHorizontalBox)

		// Left accent bar
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

		// Message body
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.BorderBackgroundColor(BackgroundColor)
			.Padding(FMargin(12.0f, 8.0f, 10.0f, 8.0f))
			[
				SNew(SVerticalBox)

				// Role label
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 6)
				[
					SNew(STextBlock)
					.Text(FText::FromString(RoleLabel))
					.TextStyle(FAppStyle::Get(), "SmallText")
					.ColorAndOpacity(FSlateColor(RoleLabelColor))
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
		FString WelcomeMessage = TEXT("👋 Welcome to Unreal Claude!\n\nI'm ready to help with your UE5.7 project. Ask me about:\n• C++ code patterns and best practices\n• Blueprint integration\n• Engine systems (Nanite, Lumen, GAS, etc.)\n• Debugging and optimization\n\n");

		// Add MCP tool status
		WelcomeMessage += GenerateMCPStatusMessage();

		WelcomeMessage += TEXT("\nType your question below and press Enter or click Send.");
		AddMessage(WelcomeMessage, false);
	}
}

SClaudeEditorWidget::~SClaudeEditorWidget()
{
	// Cancel any pending requests
	FClaudeCodeSubsystem::Get().CancelCurrentRequest();
}

TSharedRef<SWidget> SClaudeEditorWidget::BuildToolbar()
{
	return SNew(SClaudeToolbar)
		.bUE57ContextEnabled_Lambda([this]() { return bIncludeUE57Context; })
		.bProjectContextEnabled_Lambda([this]() { return bIncludeProjectContext; })
		.bRestoreEnabled_Lambda([this]() { return FClaudeCodeSubsystem::Get().HasSavedSession(); })
		.OnUE57ContextChanged_Lambda([this](bool bEnabled) { bIncludeUE57Context = bEnabled; })
		.OnProjectContextChanged_Lambda([this](bool bEnabled) { bIncludeProjectContext = bEnabled; })
		.OnRefreshContext_Lambda([this]() { RefreshProjectContext(); })
		.OnRestoreSession_Lambda([this]() { RestoreSession(); })
		.OnNewSession_Lambda([this]() { NewSession(); })
		.OnClear_Lambda([this]() { ClearChat(); })
		.OnCopyLast_Lambda([this]() { CopyToClipboard(); });
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
		// Add a thin separator line between messages for visual clarity
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
	// Extract image paths before checking emptiness
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

	// Build display message
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

	// Add user message to chat
	AddMessage(DisplayMessage, true);

	// Build prompt - use default if image-only
	FString Prompt = bHasText ? CurrentInputText : TEXT("Please analyze this image.");

	// Clear input
	CurrentInputText.Empty();
	if (InputArea.IsValid())
	{
		InputArea->ClearText();
	}

	// Set waiting state
	bIsWaitingForResponse = true;

	// Start streaming response display
	StartStreamingResponse();

	// Send to Claude using FClaudePromptOptions
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

		LastResponse = StreamingResponse.IsEmpty() ? Response : StreamingResponse;

		// Only add a new bubble if we had no streaming bubble at all
		if (StreamingResponse.IsEmpty())
		{
			AddMessage(Response, false);
		}
	}
	else
	{
		FinalizeStreamingResponse();
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
	ResetStreamingState();

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
	ResetStreamingState();

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
		double ElapsedSec = FPlatformTime::Seconds() - StreamingStartTime;
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
		// Add separator before streaming response
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

		// Create the first text segment container
		TSharedPtr<SVerticalBox> FirstSegmentContainer;

		// Build the inner content box with role label + first text segment
		SAssignNew(StreamingContentBox, SVerticalBox)

		// Role label
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Claude")))
			.TextStyle(FAppStyle::Get(), "SmallText")
			.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.6f, 0.3f)))
		]

		// First text segment (wrapped in container for code block replacement)
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

		// Wrap content box in accent bar + border (matching SChatMessage style)
		ChatMessagesBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(4.0f, 6.0f, 4.0f, 6.0f))
		[
			SNew(SHorizontalBox)

			// Left accent bar (orange for Claude)
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

			// Message body containing the dynamic content box
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

		// Scroll to bottom
		if (ChatScrollBox.IsValid())
		{
			ChatScrollBox->ScrollToEnd();
		}
	}
}

void SClaudeEditorWidget::OnClaudeProgress(const FString& PartialOutput)
{
	// Append to total and current segment
	StreamingResponse += PartialOutput;
	CurrentSegmentText += PartialOutput;

	// Update the current text segment block
	if (StreamingTextBlock.IsValid())
	{
		StreamingTextBlock->SetText(FText::FromString(CurrentSegmentText));
	}

	// Auto-scroll to bottom as content streams in
	if (ChatScrollBox.IsValid())
	{
		ChatScrollBox->ScrollToEnd();
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

	default:
		UE_LOG(LogUnrealClaude, Log, TEXT("[StreamEvent] Unknown type: %d"), static_cast<int32>(Event.Type));
		break;
	}
}

void SClaudeEditorWidget::FinalizeStreamingResponse()
{
	// Save the final text segment
	AllTextSegments.Add(CurrentSegmentText);

	// Rebuild StreamingResponse from all segments for copy support
	FString Rebuilt;
	for (const FString& Segment : AllTextSegments)
	{
		Rebuilt += Segment;
	}
	if (!Rebuilt.IsEmpty())
	{
		StreamingResponse = Rebuilt;
	}

	// For simple single-segment responses (no tool events), ensure text block is up to date
	if (StreamingTextBlock.IsValid() && !StreamingResponse.IsEmpty() && TextSegmentBlocks.Num() <= 1)
	{
		StreamingTextBlock->SetText(FText::FromString(StreamingResponse));
	}

	LastResponse = StreamingResponse;

	// Post-process text segments to render code blocks
	ParseAndRenderCodeBlocks();

	// Clear all streaming state (except StreamingResponse which is used by OnClaudeResponse)
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

	// Track tool call count for status bar
	StreamingToolCallCount++;

	// Store tool name for later lookup
	ToolCallNames.Add(Event.ToolCallId, Event.ToolName);

	FString DisplayName = GetDisplayToolName(Event.ToolName);

	// Check if this is a consecutive tool (no text since last tool = same group)
	bool bIsConsecutive = CurrentSegmentText.IsEmpty() && ToolGroupInnerBox.IsValid();

	if (!bIsConsecutive)
	{
		// Freeze the current text segment
		AllTextSegments.Add(CurrentSegmentText);
		CurrentSegmentText.Empty();

		// Collapse empty text segment
		if (AllTextSegments.Last().IsEmpty() && TextSegmentContainers.Num() > 0)
		{
			TextSegmentContainers.Last()->SetVisibility(EVisibility::Collapsed);
		}

		// Start a new tool group
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

		// Create a new text segment for text after this tool group
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

	// Add tool entry to the current group
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

	// Update group summary header
	UpdateToolGroupSummary();

	if (ChatScrollBox.IsValid())
	{
		ChatScrollBox->ScrollToEnd();
	}
}

void SClaudeEditorWidget::HandleToolResultEvent(const FClaudeStreamEvent& Event)
{
	// Look up tool name
	const FString* ToolNamePtr = ToolCallNames.Find(Event.ToolCallId);
	FString ToolName = ToolNamePtr ? GetDisplayToolName(*ToolNamePtr) : TEXT("Tool");

	// Update status label to show completion
	TSharedPtr<STextBlock>* StatusLabelPtr = ToolCallStatusLabels.Find(Event.ToolCallId);
	if (StatusLabelPtr && StatusLabelPtr->IsValid())
	{
		(*StatusLabelPtr)->SetText(FText::FromString(FString::Printf(TEXT("✓ %s completed"), *ToolName)));
		(*StatusLabelPtr)->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.75f, 0.3f)));
	}

	// Set result text (truncated for display)
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

	// Make expandable area visible
	TSharedPtr<SExpandableArea>* ExpandPtr = ToolCallExpandables.Find(Event.ToolCallId);
	if (ExpandPtr && ExpandPtr->IsValid())
	{
		(*ExpandPtr)->SetVisibility(EVisibility::Visible);
	}

	// Update group summary
	ToolGroupDoneCount++;
	UpdateToolGroupSummary();

	if (ChatScrollBox.IsValid())
	{
		ChatScrollBox->ScrollToEnd();
	}
}

void SClaudeEditorWidget::HandleResultEvent(const FClaudeStreamEvent& Event)
{
	if (!StreamingContentBox.IsValid())
	{
		return;
	}

	// Collapse empty trailing text block if no text followed the last tool
	if (CurrentSegmentText.IsEmpty() && TextSegmentContainers.Num() > 0)
	{
		TextSegmentContainers.Last()->SetVisibility(EVisibility::Collapsed);
	}

	// Format stats footer
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

	// Store final stats for the status bar
	LastResultStats = StatsText;

	// Append stats footer to content box
	StreamingContentBox->AddSlot()
	.AutoHeight()
	.Padding(0, 8, 0, 0)
	[
		SNew(STextBlock)
		.Text(FText::FromString(StatsText))
		.TextStyle(FAppStyle::Get(), "SmallText")
		.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.45f)))
	];

	if (ChatScrollBox.IsValid())
	{
		ChatScrollBox->ScrollToEnd();
	}
}

FString SClaudeEditorWidget::GetDisplayToolName(const FString& FullToolName)
{
	FString Name = FullToolName;
	// Strip common MCP server prefix for cleaner display
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
		// Single tool - show its name in the header
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
		// Multiple tools - show count summary
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

		// Get the segment text
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

		// Parse into code and plain text sections
		TArray<TPair<FString, bool>> Sections;
		ParseCodeFences(SegmentText, Sections);

		if (Sections.Num() <= 1)
		{
			continue;
		}

		// Replace container contents with parsed sections
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
				// Plain text
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

	// Try to get MCP server
	TSharedPtr<FUnrealClaudeMCPServer> MCPServer = FUnrealClaudeModule::Get().GetMCPServer();

	if (!MCPServer.IsValid() || !MCPServer->IsRunning())
	{
		// MCP server not running
		StatusMessage += TEXT("❌ MCP Server: NOT RUNNING\n\n");
		StatusMessage += TEXT("⚠️ MCP tools are unavailable.\n\n");
		StatusMessage += TEXT("Troubleshooting:\n");
		StatusMessage += TEXT("  • Check Output Log for MCP errors\n");
		StatusMessage += TEXT("  • Run: npm install in Resources/mcp-bridge\n");
		StatusMessage += FString::Printf(TEXT("  • Verify port %d is available\n"), UnrealClaudeConstants::MCPServer::DefaultPort);
		StatusMessage += TEXT("─────────────────────────────────");
		return StatusMessage;
	}

	// MCP server running - check tools
	TSharedPtr<FMCPToolRegistry> ToolRegistry = MCPServer->GetToolRegistry();
	if (!ToolRegistry.IsValid())
	{
		StatusMessage += TEXT("❌ Tool Registry: NOT INITIALIZED\n");
		StatusMessage += TEXT("─────────────────────────────────");
		return StatusMessage;
	}

	// Get registered tools
	TArray<FMCPToolInfo> RegisteredTools = ToolRegistry->GetAllTools();

	// Build set of registered tool names for quick lookup
	TSet<FString> RegisteredToolNames;
	for (const FMCPToolInfo& Tool : RegisteredTools)
	{
		RegisteredToolNames.Add(Tool.Name);
	}

	// Get expected tools from constants
	const TArray<FString>& ExpectedTools = UnrealClaudeConstants::MCPServer::ExpectedTools;

	// Check each expected tool - only track missing ones
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

	// Summary - only show details if there are issues
	if (MissingTools.Num() == 0)
	{
		StatusMessage += FString::Printf(TEXT("  ✓ All %d tools operational\n"), AvailableCount);
	}
	else
	{
		StatusMessage += FString::Printf(TEXT("  ✓ %d/%d tools available\n"), AvailableCount, ExpectedTools.Num());
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

#undef LOCTEXT_NAMESPACE
