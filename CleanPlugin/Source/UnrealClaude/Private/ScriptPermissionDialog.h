// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScriptTypes.h"

/**
 * Modal dialog for requesting user permission before script execution.
 * Implements the principle of least privilege for Claude-generated scripts.
 */
class FScriptPermissionDialog
{
public:
	/**
	 * Show a modal permission dialog for script execution.
	 * Must be called from the game thread.
	 *
	 * @param ScriptPreview The script content to display for review
	 * @param Type The type of script being executed
	 * @param Description Human-readable description of what the script does
	 * @return true if user approved, false if denied or error
	 */
	static bool Show(
		const FString& ScriptPreview,
		EScriptType Type,
		const FString& Description);

private:
	/** Maximum characters to show in preview before truncating */
	static constexpr int32 MaxPreviewLength = 2000;

	/** Dialog window size */
	static constexpr float DialogWidth = 700.0f;
	static constexpr float DialogHeight = 500.0f;

	/** Build the dialog content widget */
	static TSharedRef<class SWidget> BuildContent(
		const FString& DisplayPreview,
		const FString& TypeStr,
		const FString& Description,
		TSharedPtr<bool> bApprovedPtr,
		TSharedPtr<class SWindow> Window);

	/** Build the header section with warning and description */
	static TSharedRef<class SVerticalBox> BuildHeaderSection(
		const FString& TypeStr,
		const FString& Description);

	/** Build the script preview section */
	static TSharedRef<class SWidget> BuildPreviewSection(const FString& DisplayPreview);

	/** Build the button bar */
	static TSharedRef<class SWidget> BuildButtonBar(
		TSharedPtr<bool> bApprovedPtr,
		TSharedPtr<class SWindow> Window);
};
