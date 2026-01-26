// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Utility class for clipboard image operations.
 * Reads image data from the Windows clipboard and saves as PNG.
 */
class FClipboardImageUtils
{
public:
	/** Check if the clipboard currently contains image data */
	static bool ClipboardHasImage();

	/**
	 * Save the current clipboard image to a PNG file.
	 * @param OutFilePath - Receives the full path to the saved PNG
	 * @param SaveDirectory - Directory to save the file in
	 * @return true if image was successfully saved
	 */
	static bool SaveClipboardImageToFile(FString& OutFilePath, const FString& SaveDirectory);

	/** Delete old clipboard_*.png files beyond the max age */
	static void CleanupOldScreenshots(const FString& Directory, double MaxAgeSeconds);

	/** Get the default screenshot save directory (Saved/UnrealClaude/screenshots/) */
	static FString GetScreenshotDirectory();
};
