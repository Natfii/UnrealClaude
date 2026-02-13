// Copyright Natali Caggiano. All Rights Reserved.

#include "ClipboardImageUtils.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

FString FClipboardImageUtils::GetScreenshotDirectory()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("UnrealClaude"),
		UnrealClaudeConstants::ClipboardImage::ScreenshotSubdirectory);
}

bool FClipboardImageUtils::ClipboardHasImage()
{
#if PLATFORM_WINDOWS
	return ::IsClipboardFormatAvailable(CF_DIB) != 0;
#elif PLATFORM_LINUX
	// Check if wl-paste or xclip can provide image data
	// Route through /bin/sh so PATH is resolved (binaries may not be in /usr/bin)
	int32 ReturnCode = -1;
	FString StdOut, StdErr;

	// Try wl-paste first (Wayland)
	if (FPlatformProcess::ExecProcess(TEXT("/bin/sh"), TEXT("-c 'wl-paste --list-types 2>/dev/null'"), &ReturnCode, &StdOut, &StdErr) && ReturnCode == 0)
	{
		if (StdOut.Contains(TEXT("image/png")) || StdOut.Contains(TEXT("image/")))
		{
			return true;
		}
	}

	// Try xclip (X11)
	StdOut.Empty();
	StdErr.Empty();
	if (FPlatformProcess::ExecProcess(TEXT("/bin/sh"), TEXT("-c 'xclip -selection clipboard -t TARGETS -o 2>/dev/null'"), &ReturnCode, &StdOut, &StdErr) && ReturnCode == 0)
	{
		if (StdOut.Contains(TEXT("image/png")))
		{
			return true;
		}
	}

	return false;
#else
	return false;
#endif
}

bool FClipboardImageUtils::SaveClipboardImageToFile(FString& OutFilePath, const FString& SaveDirectory)
{
#if PLATFORM_WINDOWS
	if (!::OpenClipboard(NULL))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to open clipboard"));
		return false;
	}

	HANDLE hData = ::GetClipboardData(CF_DIB);
	if (!hData)
	{
		::CloseClipboard();
		UE_LOG(LogUnrealClaude, Warning, TEXT("No DIB data in clipboard"));
		return false;
	}

	void* LockedData = ::GlobalLock(hData);
	if (!LockedData)
	{
		::CloseClipboard();
		UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to lock clipboard data"));
		return false;
	}

	// Parse BITMAPINFOHEADER
	const BITMAPINFOHEADER* Header = static_cast<const BITMAPINFOHEADER*>(LockedData);
	const int32 Width = Header->biWidth;
	const int32 Height = FMath::Abs(Header->biHeight);
	const int32 BitCount = Header->biBitCount;
	const bool bTopDown = (Header->biHeight < 0);

	// Validate dimensions and bit depth
	constexpr int32 MaxReasonableDimension = 16384;
	if (Width <= 0 || Height <= 0 || (BitCount != 24 && BitCount != 32))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Unsupported clipboard image format: %dx%d, %d bpp"), Width, Height, BitCount);
		::GlobalUnlock(hData);
		::CloseClipboard();
		return false;
	}

	if (Width > MaxReasonableDimension || Height > MaxReasonableDimension)
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Clipboard image dimensions too large: %dx%d (max %d)"), Width, Height, MaxReasonableDimension);
		::GlobalUnlock(hData);
		::CloseClipboard();
		return false;
	}

	// Validate compression type
	if (Header->biCompression != BI_RGB && !(Header->biCompression == BI_BITFIELDS && BitCount == 32))
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("Unsupported clipboard DIB compression type: %d"), Header->biCompression);
		::GlobalUnlock(hData);
		::CloseClipboard();
		return false;
	}

	// Calculate pixel data offset (after header + color table if any)
	const uint8* PixelData = static_cast<const uint8*>(LockedData) + Header->biSize;
	if (Header->biCompression == BI_BITFIELDS && BitCount == 32)
	{
		PixelData += 12; // 3 DWORD color masks
	}

	// Row stride is aligned to 4 bytes
	const int32 SrcBytesPerPixel = BitCount / 8;
	const int32 SrcRowStride = ((Width * SrcBytesPerPixel + 3) / 4) * 4;

	// Convert to BGRA pixel array
	TArray<FColor> Pixels;
	Pixels.SetNum(Width * Height);

	for (int32 Y = 0; Y < Height; ++Y)
	{
		// DIB is bottom-up by default, flip unless top-down
		const int32 SrcRow = bTopDown ? Y : (Height - 1 - Y);
		const uint8* RowPtr = PixelData + SrcRow * SrcRowStride;

		for (int32 X = 0; X < Width; ++X)
		{
			const uint8* Pixel = RowPtr + X * SrcBytesPerPixel;
			FColor& OutPixel = Pixels[Y * Width + X];
			OutPixel.B = Pixel[0];
			OutPixel.G = Pixel[1];
			OutPixel.R = Pixel[2];
			OutPixel.A = (BitCount == 32) ? Pixel[3] : 255;
		}
	}

	::GlobalUnlock(hData);
	::CloseClipboard();

	// Compress to PNG using IImageWrapperModule
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to create PNG image wrapper"));
		return false;
	}

	if (!ImageWrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to set raw pixel data for PNG compression"));
		return false;
	}

	const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed();
	if (CompressedData.Num() == 0)
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("PNG compression returned empty data"));
		return false;
	}

	// Ensure directory exists
	IFileManager::Get().MakeDirectory(*SaveDirectory, true);

	// Generate filename with timestamp
	FDateTime Now = FDateTime::Now();
	FString FileName = FString::Printf(TEXT("clipboard_%04d%02d%02d_%02d%02d%02d.png"),
		Now.GetYear(), Now.GetMonth(), Now.GetDay(),
		Now.GetHour(), Now.GetMinute(), Now.GetSecond());

	OutFilePath = FPaths::Combine(SaveDirectory, FileName);

	if (!FFileHelper::SaveArrayToFile(CompressedData, *OutFilePath))
	{
		UE_LOG(LogUnrealClaude, Error, TEXT("Failed to save clipboard image to: %s"), *OutFilePath);
		return false;
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("Saved clipboard image: %s (%dx%d, %lld bytes)"),
		*OutFilePath, Width, Height, CompressedData.Num());
	return true;

#elif PLATFORM_LINUX
	// On Linux, use wl-paste (Wayland) or xclip (X11) to get clipboard image as PNG

	// Ensure directory exists
	IFileManager::Get().MakeDirectory(*SaveDirectory, true);

	// Generate filename with timestamp
	FDateTime Now = FDateTime::Now();
	FString FileName = FString::Printf(TEXT("clipboard_%04d%02d%02d_%02d%02d%02d.png"),
		Now.GetYear(), Now.GetMonth(), Now.GetDay(),
		Now.GetHour(), Now.GetMinute(), Now.GetSecond());

	OutFilePath = FPaths::Combine(SaveDirectory, FileName);

	int32 ReturnCode = -1;
	FString StdOut, StdErr;

	// Try wl-paste first (Wayland) - outputs PNG directly to stdout
	// We shell out via /bin/sh since ExecProcess captures stdout as text, not binary
	if (FPlatformProcess::ExecProcess(TEXT("/bin/sh"), *FString::Printf(TEXT("-c 'wl-paste --type image/png > \"%s\"'"), *OutFilePath),
		&ReturnCode, &StdOut, &StdErr) && ReturnCode == 0)
	{
		if (IFileManager::Get().FileExists(*OutFilePath) && IFileManager::Get().FileSize(*OutFilePath) > 0)
		{
			UE_LOG(LogUnrealClaude, Log, TEXT("Saved clipboard image via wl-paste: %s (%lld bytes)"),
				*OutFilePath, IFileManager::Get().FileSize(*OutFilePath));
			return true;
		}
	}

	// Try xclip (X11)
	StdOut.Empty();
	StdErr.Empty();
	if (FPlatformProcess::ExecProcess(TEXT("/bin/sh"), *FString::Printf(TEXT("-c 'xclip -selection clipboard -t image/png -o > \"%s\"'"), *OutFilePath),
		&ReturnCode, &StdOut, &StdErr) && ReturnCode == 0)
	{
		if (IFileManager::Get().FileExists(*OutFilePath) && IFileManager::Get().FileSize(*OutFilePath) > 0)
		{
			UE_LOG(LogUnrealClaude, Log, TEXT("Saved clipboard image via xclip: %s (%lld bytes)"),
				*OutFilePath, IFileManager::Get().FileSize(*OutFilePath));
			return true;
		}
	}

	// Clean up empty/failed file
	if (IFileManager::Get().FileExists(*OutFilePath))
	{
		IFileManager::Get().Delete(*OutFilePath);
	}

	UE_LOG(LogUnrealClaude, Warning, TEXT("Failed to get clipboard image. Install wl-paste (wl-clipboard) or xclip."));
	return false;

#else
	UE_LOG(LogUnrealClaude, Warning, TEXT("Clipboard image paste is not supported on this platform"));
	return false;
#endif
}

void FClipboardImageUtils::CleanupOldScreenshots(const FString& Directory, double MaxAgeSeconds)
{
	if (!FPaths::DirectoryExists(Directory))
	{
		return;
	}

	FDateTime CutoffTime = FDateTime::UtcNow() - FTimespan::FromSeconds(MaxAgeSeconds);

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("clipboard_*.png")), true, false);

	int32 DeletedCount = 0;
	for (const FString& FileName : Files)
	{
		FString FullPath = FPaths::Combine(Directory, FileName);
		FDateTime ModTime = IFileManager::Get().GetTimeStamp(*FullPath);

		if (ModTime < CutoffTime)
		{
			IFileManager::Get().Delete(*FullPath);
			DeletedCount++;
		}
	}

	if (DeletedCount > 0)
	{
		UE_LOG(LogUnrealClaude, Log, TEXT("Cleaned up %d old clipboard screenshots from %s"), DeletedCount, *Directory);
	}
}
