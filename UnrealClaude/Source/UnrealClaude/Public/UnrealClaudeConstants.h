// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Constants used throughout the UnrealClaude plugin
 * Centralizes magic numbers and configuration values
 */
namespace UnrealClaudeConstants
{
	// Process and I/O
	namespace Process
	{
		/** Buffer size for reading process output */
		constexpr int32 OutputBufferSize = 4096;

		/** Timeout in milliseconds when waiting for process */
		constexpr int32 WaitTimeoutMs = 100;

		/** Default timeout in seconds for Claude CLI execution */
		constexpr float DefaultTimeoutSeconds = 300.0f;
	}

	// UI Dimensions
	namespace UI
	{
		/** Maximum height for input text area */
		constexpr float MaxInputHeight = 300.0f;

		/** Minimum height for input text area */
		constexpr float MinInputHeight = 60.0f;

		/** Default permission dialog width */
		constexpr float PermissionDialogWidth = 700.0f;

		/** Default permission dialog height */
		constexpr float PermissionDialogHeight = 500.0f;

		/** Maximum script preview length in characters */
		constexpr int32 MaxScriptPreviewLength = 2000;
	}

	// Session Management
	namespace Session
	{
		/** Maximum number of exchanges to store in history */
		constexpr int32 MaxHistorySize = 50;

		/** Maximum number of history exchanges to include in prompt */
		constexpr int32 MaxHistoryInPrompt = 10;
	}

	// Project Context
	namespace Context
	{
		/** Maximum UCLASS definitions to parse per search */
		constexpr int32 MaxUClassSearchLimit = 500;

		/** Maximum classes to format in context output */
		constexpr int32 MaxClassesToFormat = 30;

		/** Maximum directories to show in context output */
		constexpr int32 MaxDirectoriesToShow = 10;

		/** Maximum asset types to show in context output */
		constexpr int32 MaxAssetTypesToShow = 15;

		/** Maximum distance (in characters) to search for 'class' keyword after UCLASS macro */
		constexpr int32 MaxUClassToClassKeywordDistance = 500;

		/** Maximum distance (in characters) to search for parent class after class name */
		constexpr int32 MaxClassNameToInheritanceDistance = 50;
	}

	// Animation Blueprint Diagram Generation
	namespace AnimDiagram
	{
		/** Initial X position for condition nodes in transition graphs */
		constexpr int32 ConditionNodeStartX = 100;

		/** Initial Y position for condition nodes in transition graphs */
		constexpr int32 ConditionNodeStartY = 100;

		/** Horizontal spacing between condition nodes (e.g., TimeRemaining to comparison) */
		constexpr int32 ConditionNodeSpacing = 200;

		/** Width of each state cell in ASCII diagram output */
		constexpr int32 DiagramCellWidth = 20;

		/** Height of each state cell row in ASCII diagram output */
		constexpr int32 DiagramCellHeight = 3;

		/** Maximum state name length before truncation in diagrams */
		constexpr int32 MaxStateNameDisplayLength = 12;
	}

	// MCP Validation Limits
	namespace MCPValidation
	{
		/** Characters that are dangerous in actor names, paths, and class paths
		 *  Used for injection attack prevention and input sanitization
		 */
		static const TCHAR* DangerousChars = TEXT("<>|&;`$(){}[]!*?~");

		/** Maximum length for actor names */
		constexpr int32 MaxActorNameLength = 256;

		/** Maximum length for property paths */
		constexpr int32 MaxPropertyPathLength = 512;

		/** Maximum length for class paths */
		constexpr int32 MaxClassPathLength = 1024;

		/** Maximum length for console commands */
		constexpr int32 MaxCommandLength = 2048;

		/** Maximum length for filter strings */
		constexpr int32 MaxFilterLength = 256;

		/** Default actor query limit */
		constexpr int32 DefaultActorLimit = 100;
	}

	// Numeric Bounds
	namespace NumericBounds
	{
		/** Maximum absolute value for coordinate values */
		constexpr double MaxCoordinateValue = 1e10;
	}

	// Script Execution
	namespace ScriptExecution
	{
		/** Maximum wait time for Live Coding compilation in seconds */
		constexpr float MaxCompileWaitSeconds = 60.0f;

		/** Poll interval when waiting for compilation in seconds */
		constexpr float CompilePollIntervalSeconds = 0.5f;

		/** Maximum script history entries to retain */
		constexpr int32 MaxHistorySize = 100;

		/** Default number of recent scripts to return */
		constexpr int32 DefaultHistoryCount = 10;

		/** Maximum scripts to return in history query */
		constexpr int32 MaxHistoryQueryCount = 50;
	}

	// MCP Server
	namespace MCPServer
	{
		/** Default port for MCP HTTP server */
		constexpr uint32 DefaultPort = 3000;

		/** Timeout for game thread execution in milliseconds */
		constexpr uint32 GameThreadTimeoutMs = 30000;

		/** Default output log lines to return */
		constexpr int32 DefaultOutputLogLines = 100;

		/** Maximum output log lines to return */
		constexpr int32 MaxOutputLogLines = 1000;

		/** Expected MCP tools that should be registered at startup */
		inline const TArray<FString> ExpectedTools = {
			// Actor tools
			TEXT("spawn_actor"),
			TEXT("get_level_actors"),
			TEXT("delete_actors"),
			TEXT("move_actor"),
			TEXT("set_property"),
			// Utility tools
			TEXT("run_console_command"),
			TEXT("get_output_log"),
			TEXT("capture_viewport"),
			TEXT("execute_script"),
			TEXT("cleanup_scripts"),
			TEXT("get_script_history"),
			// Blueprint tools
			TEXT("blueprint_query"),
			TEXT("blueprint_modify"),
			TEXT("anim_blueprint_modify"),
			// Asset tools
			TEXT("asset_search"),
			TEXT("asset_dependencies"),
			TEXT("asset_referencers"),
			// Task queue tools
			TEXT("task_submit"),
			TEXT("task_status"),
			TEXT("task_result"),
			TEXT("task_list"),
			TEXT("task_cancel")
		};
	}
}
