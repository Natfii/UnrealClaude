// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Phase 10: Central place for engine version targeting.
// Current support is Unreal Engine 5.7 only. No functional changes yet.
// When adding support for other versions, gate 5.7-specific APIs with this macro.
#ifndef UNREALCLAUDE_ENGINE_VERSION_MAJOR
#define UNREALCLAUDE_ENGINE_VERSION_MAJOR 5
#endif
#ifndef UNREALCLAUDE_ENGINE_VERSION_MINOR
#define UNREALCLAUDE_ENGINE_VERSION_MINOR 7
#endif

#if ENGINE_MAJOR_VERSION != UNREALCLAUDE_ENGINE_VERSION_MAJOR || ENGINE_MINOR_VERSION != UNREALCLAUDE_ENGINE_VERSION_MINOR
	// Hard fail at compile time so unsupported engine versions are not silently built.
	static_assert(false, "UnrealClaude is validated for Unreal Engine 5.7 only. Other versions require explicit API adjustments.");
#endif


/**
 * Constants used throughout the UnrealClaude plugin
 * Centralizes magic numbers and configuration values
 */
namespace UnrealClaudeConstants
{
	namespace Process
	{
		/** Buffer size for reading process output */
		constexpr int32 OutputBufferSize = 4096;

		/** Timeout in milliseconds when waiting for process */
		constexpr int32 WaitTimeoutMs = 100;

		/** Default timeout in seconds for Claude CLI execution */
		constexpr float DefaultTimeoutSeconds = 300.0f;

		/**
		 * Hard cap on accumulated Claude process stdout (NDJSON stream).
		 * Prevents unbounded memory growth if the CLI emits excessively.
		 * 32 MiB is well above normal tool-use transcripts.
		 */
		constexpr int32 MaxProcessOutputBytes = 32 * 1024 * 1024;
	}

	/**
	 * Tools that must not be advertised to the model (power-user / infrastructure).
	 * Single source of truth for C++. The MCP bridge mirrors this list in
	 * Resources/mcp-bridge/tool-router.js (HIDDEN_TOOL_NAMES) — keep them in sync.
	 */
	namespace HiddenTools
	{
		inline bool IsHidden(const FString& ToolName)
		{
			static const TSet<FString> Names = {
				TEXT("task_submit"),
				TEXT("task_status"),
				TEXT("task_result"),
				TEXT("task_list"),
				TEXT("task_cancel"),
				TEXT("execute_script"),
				TEXT("cleanup_scripts"),
				TEXT("get_script_history"),
				TEXT("run_console_command"),
			};
			return Names.Contains(ToolName);
		}

		/** Subset blocked from task_submit so it cannot bypass the hide policy. */
		inline bool IsBlockedFromTaskSubmit(const FString& ToolName)
		{
			static const TSet<FString> Names = {
				TEXT("execute_script"),
				TEXT("run_console_command"),
				TEXT("cleanup_scripts"),
				TEXT("get_script_history"),
			};
			return Names.Contains(ToolName);
		}
	}

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

	namespace Session
	{
		/** Maximum number of exchanges to store in history */
		constexpr int32 MaxHistorySize = 50;

		/** Maximum number of history exchanges to include in prompt */
		constexpr int32 MaxHistoryInPrompt = 10;
	}

	namespace Context
	{
		/** Maximum UCLASS definitions to parse per search */
		constexpr int32 MaxUClassSearchLimit = 500;

		/** Maximum classes to format in context output */
		constexpr int32 MaxClassesToFormat = 20;  // Phase 6: smaller default full context

		/** Maximum directories to show in context output */
		constexpr int32 MaxDirectoriesToShow = 6;  // Phase 6: smaller default full context

		/** Maximum asset types to show in context output */
		constexpr int32 MaxAssetTypesToShow = 8;  // Phase 6: smaller default full context

		/** Maximum distance (in characters) to search for 'class' keyword after UCLASS macro */
		constexpr int32 MaxUClassToClassKeywordDistance = 500;

		/** Maximum distance (in characters) to search for parent class after class name */
		constexpr int32 MaxClassNameToInheritanceDistance = 50;
	}

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

	namespace MCPValidation
	{
		/** Characters that are dangerous in actor names, paths, and class paths
		 *  Used for injection attack prevention and input sanitization
		 */
		inline constexpr const TCHAR* DangerousChars = TEXT("<>|&;`$(){}[]!*?~");

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

	namespace NumericBounds
	{
		/** Maximum absolute value for coordinate values */
		constexpr double MaxCoordinateValue = 1e10;
	}

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

		/** Maximum HTTP request body size in bytes (1 MB) */
		constexpr int32 MaxRequestBodySize = 1024 * 1024;

		/** Maximum serialized MCP tool response size in bytes (4 MB) */
		constexpr int32 MaxResponseSize = 4 * 1024 * 1024;

		/** Maximum output-log content returned by get_output_log (512 KiB) */
		constexpr int32 MaxOutputLogContentSize = 512 * 1024;

		/** Maximum viewport pixels read into memory before downscaling */
		constexpr int64 MaxViewportPixels = 4LL * 1024LL * 1024LL;


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
			TEXT("asset"),
			// Character tools
			TEXT("character"),
			TEXT("character_data"),
			// Enhanced Input tools
			TEXT("enhanced_input"),
			// Material tools
			TEXT("material"),
			// Level management tools
			TEXT("open_level"),
			// Task queue tools
			TEXT("task_submit"),
			TEXT("task_status"),
			TEXT("task_result"),
			TEXT("task_list"),
			TEXT("task_cancel")
		};
	}

	namespace ClipboardImage
	{
		/** Maximum age of saved clipboard screenshots before cleanup (seconds) */
		constexpr double MaxScreenshotAgeSeconds = 3600.0;

		/** Subdirectory under Saved/UnrealClaude/ for clipboard screenshots */
		inline constexpr const TCHAR* ScreenshotSubdirectory = TEXT("screenshots");

		/** Thumbnail preview size in the input area (pixels) */
		constexpr float ThumbnailSize = 64.0f;

		/** Maximum number of images that can be attached to a single message */
		constexpr int32 MaxImagesPerMessage = 5;

		/** Maximum file size per image in bytes (4.5 MB; Claude API limit is 5 MB) */
		constexpr int64 MaxImageFileSize = 4608 * 1024;

		/** Maximum total image payload size in bytes (20 MB) */
		constexpr int64 MaxTotalImagePayloadSize = 20 * 1024 * 1024;

		/** Spacing between thumbnail previews in the image strip (pixels) */
		constexpr float ThumbnailSpacing = 4.0f;
	}
}
