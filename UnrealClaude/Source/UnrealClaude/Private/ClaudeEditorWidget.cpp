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
	// Extract image path before checking emptiness
	FString ImagePath;
	if (InputArea.IsValid())
	{
		ImagePath = InputArea->GetAttachedImagePath();
	}

	bool bHasText = !CurrentInputText.IsEmpty();
	bool bHasImage = !ImagePath.IsEmpty();

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
		FString FileName = FPaths::GetCleanFilename(ImagePath);
		if (bHasText)
		{
			DisplayMessage += FString::Printf(TEXT("\n[Attached image: %s]"), *FileName);
		}
		else
		{
			DisplayMessage = FString::Printf(TEXT("[Attached image: %s]"), *FileName);
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
	Options.AttachedImagePath = ImagePath;

	FClaudeCodeSubsystem::Get().SendPrompt(Prompt, OnComplete, Options);
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
