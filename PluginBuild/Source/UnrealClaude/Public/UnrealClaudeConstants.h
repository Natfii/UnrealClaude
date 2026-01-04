// Copyright Your Name. All Rights Reserved.

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
	}

	// MCP Validation Limits
	namespace MCPValidation
	{
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
}
