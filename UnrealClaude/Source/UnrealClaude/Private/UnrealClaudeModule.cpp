// Copyright Natali Caggiano. All Rights Reserved.

#include "UnrealClaudeModule.h"
#include "UnrealClaudeSettings.h"
#include "UnrealClaudeCommands.h"
#include "ClaudeEditorWidget.h"
#include "ClaudeCodeRunner.h"
#include "ClaudeSubsystem.h"
#include "ScriptExecutionManager.h"
#include "MCP/UnrealClaudeMCPServer.h"
#include "ProjectContext.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Application/SlateApplication.h"
#include "HttpServerModule.h"

DEFINE_LOG_CATEGORY(LogUnrealClaude);

#define LOCTEXT_NAMESPACE "FUnrealClaudeModule"

static const FName ClaudeTabName("ClaudeAssistant");

void FUnrealClaudeModule::StartupModule()
{
	// Phase 10: This plugin is currently validated for Unreal Engine 5.7 only.
	// See UNREALCLAUDE_ENGINE_VERSION_* in UnrealClaudeConstants.h before porting.
	UE_LOG(LogUnrealClaude, Warning, TEXT("=== UnrealClaude BUILD 20260107-1450 THREAD_TESTS_DISABLED ==="));

	FUnrealClaudeCommands::Register();

	PluginCommands = MakeShared<FUICommandList>();

	PluginCommands->MapAction(
		FUnrealClaudeCommands::Get().OpenClaudePanel,
		FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(ClaudeTabName);
		}),
		FCanExecuteAction()
	);

	PluginCommands->MapAction(
		FUnrealClaudeCommands::Get().QuickAsk,
		FExecuteAction::CreateLambda([]()
		{
			TSharedRef<SWindow> QuickAskWindow = SNew(SWindow)
				.Title(LOCTEXT("QuickAskTitle", "Quick Ask Claude"))
				.ClientSize(FVector2D(500, 100))
				.SupportsMinimize(false)
				.SupportsMaximize(false);

			TSharedPtr<SEditableTextBox> InputBox;

			QuickAskWindow->SetContent(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(10)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("QuickAskLabel", "Ask Claude a quick question:"))
				]
				+ SVerticalBox::Slot()
				.Padding(10, 0, 10, 10)
				.FillHeight(1.0f)
				[
					SAssignNew(InputBox, SEditableTextBox)
					.HintText(LOCTEXT("QuickAskHint", "Type your question here..."))
					.OnTextCommitted_Lambda([QuickAskWindow](const FText& Text, ETextCommit::Type CommitType)
					{
						if (CommitType == ETextCommit::OnEnter && !Text.IsEmpty())
						{
							QuickAskWindow->RequestDestroyWindow();

							FString Prompt = Text.ToString();
							FClaudePromptOptions Options;
							Options.bIncludeEngineContext = true;
							Options.bIncludeProjectContext = true;
							FClaudeCodeSubsystem::Get().SendPrompt(
								Prompt,
								FOnClaudeResponse::CreateLambda([](const FString& Response, bool bSuccess)
								{
									FNotificationInfo Info(FText::FromString(
										bSuccess
											? (Response.Len() > 300 ? Response.Left(300) + TEXT("...") : Response)
											: TEXT("Error: ") + Response));
									Info.ExpireDuration = bSuccess ? 15.0f : 5.0f;
									Info.bUseLargeFont = false;
									Info.bUseSuccessFailIcons = true;
									FSlateNotificationManager::Get().AddNotification(Info);
								}),
								Options
							);
						}
					})
				]
			);

			FSlateApplication::Get().AddWindow(QuickAskWindow);

			if (InputBox.IsValid())
			{
				FSlateApplication::Get().SetKeyboardFocus(InputBox);
			}
		}),
		FCanExecuteAction::CreateLambda([]()
		{
			return FClaudeCodeRunner::IsClaudeAvailable();
		})
	);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ClaudeTabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				.Label(LOCTEXT("ClaudeTabTitle", "Claude Assistant"))
				[
					SNew(SClaudeEditorWidget)
				];
		}))
		.SetDisplayName(LOCTEXT("ClaudeTabTitle", "Claude Assistant"))
		.SetTooltipText(LOCTEXT("ClaudeTabTooltip", "Open the Claude AI Assistant for UE5.7 development help"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help"));

	// Defer menu registration until UToolMenus is initialized post-engine-startup
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnrealClaudeModule::RegisterMenus));

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetGlobalLevelEditorActions()->Append(PluginCommands.ToSharedRef());

	if (FClaudeCodeRunner::IsClaudeAvailable())
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("Claude CLI found at: %s"), *FClaudeCodeRunner::GetClaudePath());
	}
	else
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Claude CLI not found. Please install with: npm install -g @anthropic-ai/claude-code"));
	}

	StartMCPServer();

	// Kick off project-context gather; first GetContext call after this will use the cached result
	FProjectContextManager::Get().RefreshContext();

	// First Get() call constructs the singleton, which lazily creates script directories
	FScriptExecutionManager::Get();
}

void FUnrealClaudeModule::ShutdownModule()
{
	UE_LOG(LogUnrealClaude, Log, TEXT("UnrealClaude module shutting down"));

	StopMCPServer();

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FUnrealClaudeCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ClaudeTabName);
}

FUnrealClaudeModule& FUnrealClaudeModule::Get()
{
	return FModuleManager::LoadModuleChecked<FUnrealClaudeModule>("UnrealClaude");
}

bool FUnrealClaudeModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("UnrealClaude");
}

void FUnrealClaudeModule::RegisterMenus()
{
	// Owner scope ensures our entries can be cleanly removed by UToolMenus::UnregisterOwner on shutdown
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->FindOrAddSection("UnrealClaude");

		Section.AddMenuEntryWithCommandList(
			FUnrealClaudeCommands::Get().OpenClaudePanel,
			PluginCommands,
			LOCTEXT("OpenClaudeMenuItem", "Claude Assistant"),
			LOCTEXT("OpenClaudeMenuItemTooltip", "Open the Claude AI Assistant for UE5.7 help (Ctrl+Shift+C)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help")
		);

		Section.AddMenuEntryWithCommandList(
			FUnrealClaudeCommands::Get().QuickAsk,
			PluginCommands,
			LOCTEXT("QuickAskMenuItem", "Quick Ask Claude"),
			LOCTEXT("QuickAskMenuItemTooltip", "Quickly ask Claude a question (Ctrl+Alt+C)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help")
		);
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("UnrealClaude");
		
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FUnrealClaudeCommands::Get().OpenClaudePanel,
			LOCTEXT("ClaudeToolbarButton", "Claude"),
			LOCTEXT("ClaudeToolbarTooltip", "Open Claude Assistant (Ctrl+Shift+C)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help")
		));
	}
}

void FUnrealClaudeModule::UnregisterMenus()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

uint32 FUnrealClaudeModule::GetMCPServerPort()
{
	const int32 DefaultPort = static_cast<int32>(UnrealClaudeConstants::MCPServer::DefaultPort);
	int32 Port = DefaultPort;

	if (const UUnrealClaudeSettings* Settings = GetDefault<UUnrealClaudeSettings>())
	{
		Port = Settings->MCPServerPort;
	}

	// Clamp to a safe user-port range. Invalid values fall back to the default.
	if (Port < 1024 || Port > 65535)
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("MCP Server Port setting %d is out of range (1024-65535). Using default %d."), Port, DefaultPort);
		Port = DefaultPort;
	}

	return static_cast<uint32>(Port);
}


void FUnrealClaudeModule::StartMCPServer()
{
	if (MCPServer.IsValid())
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("MCP Server already exists"));
		return;
	}

	MCPServer = MakeShared<FUnrealClaudeMCPServer>();

	if (!MCPServer->Start(GetMCPServerPort()))
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to start MCP Server on port %d"), GetMCPServerPort());
		MCPServer.Reset();
	}
}

void FUnrealClaudeModule::StopMCPServer()
{
	if (MCPServer.IsValid())
	{
		MCPServer->Stop();
		MCPServer.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealClaudeModule, UnrealClaude)
