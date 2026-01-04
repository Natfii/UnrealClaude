// Copyright Your Name. All Rights Reserved.

#include "MCPTool_CaptureViewport.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "UnrealClient.h"
#include "Slate/SceneViewport.h"

namespace
{
	// Target resolution
	constexpr int32 TargetWidth = 1024;
	constexpr int32 TargetHeight = 576;
	constexpr int32 JPEGQuality = 70;

	// Simple bilinear resize
	void ResizePixels(const TArray<FColor>& InPixels, int32 InWidth, int32 InHeight,
		TArray<FColor>& OutPixels, int32 OutWidth, int32 OutHeight)
	{
		OutPixels.SetNumUninitialized(OutWidth * OutHeight);

		const float ScaleX = static_cast<float>(InWidth) / OutWidth;
		const float ScaleY = static_cast<float>(InHeight) / OutHeight;

		for (int32 Y = 0; Y < OutHeight; ++Y)
		{
			for (int32 X = 0; X < OutWidth; ++X)
			{
				const int32 SrcX = FMath::Clamp(static_cast<int32>(X * ScaleX), 0, InWidth - 1);
				const int32 SrcY = FMath::Clamp(static_cast<int32>(Y * ScaleY), 0, InHeight - 1);
				OutPixels[Y * OutWidth + X] = InPixels[SrcY * InWidth + SrcX];
			}
		}
	}
}

FMCPToolResult FMCPTool_CaptureViewport::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Validate editor is available
	if (!GEditor)
	{
		return FMCPToolResult::Error(TEXT("Editor is not available."));
	}

	// Get viewport - PIE first, then editor
	FViewport* Viewport = GEditor->GetPIEViewport();
	FString ViewportType = TEXT("PIE");

	if (!Viewport)
	{
		Viewport = GEditor->GetActiveViewport();
		ViewportType = TEXT("Editor");
	}

	if (!Viewport)
	{
		return FMCPToolResult::Error(TEXT("No viewport available. Open a level or start PIE."));
	}

	// Get viewport size
	const FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return FMCPToolResult::Error(TEXT("Viewport has invalid size."));
	}

	// Read pixels from viewport
	TArray<FColor> Pixels;
	if (!Viewport->ReadPixels(Pixels))
	{
		return FMCPToolResult::Error(TEXT("Failed to read viewport pixels."));
	}

	// Validate pixel array size
	const int32 ExpectedPixels = ViewportSize.X * ViewportSize.Y;
	if (Pixels.Num() != ExpectedPixels)
	{
		return FMCPToolResult::Error(TEXT("Pixel array size mismatch."));
	}

	// Resize to target resolution
	TArray<FColor> ResizedPixels;
	ResizePixels(Pixels, ViewportSize.X, ViewportSize.Y, ResizedPixels, TargetWidth, TargetHeight);

	// Compress to JPEG
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (!ImageWrapper.IsValid())
	{
		return FMCPToolResult::Error(TEXT("Failed to create image wrapper."));
	}

	// Set raw pixel data (BGRA format from FColor)
	if (!ImageWrapper->SetRaw(ResizedPixels.GetData(), ResizedPixels.Num() * sizeof(FColor),
		TargetWidth, TargetHeight, ERGBFormat::BGRA, 8))
	{
		return FMCPToolResult::Error(TEXT("Failed to set image data."));
	}

	// Get compressed JPEG data (UE 5.7 API returns TArray64 directly)
	TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(JPEGQuality);
	if (CompressedData.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("Failed to compress image to JPEG."));
	}

	// Encode to base64
	const FString Base64Image = FBase64::Encode(CompressedData.GetData(), CompressedData.Num());

	// Build result JSON
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("image_base64"), Base64Image);
	ResultData->SetNumberField(TEXT("width"), TargetWidth);
	ResultData->SetNumberField(TEXT("height"), TargetHeight);
	ResultData->SetStringField(TEXT("format"), TEXT("jpeg"));
	ResultData->SetNumberField(TEXT("quality"), JPEGQuality);
	ResultData->SetStringField(TEXT("viewport_type"), ViewportType);
	ResultData->SetNumberField(TEXT("original_width"), ViewportSize.X);
	ResultData->SetNumberField(TEXT("original_height"), ViewportSize.Y);

	UE_LOG(LogUnrealClaude, Log, TEXT("Captured %s viewport: %dx%d -> %dx%d JPEG (%d bytes base64)"),
		*ViewportType, ViewportSize.X, ViewportSize.Y, TargetWidth, TargetHeight, Base64Image.Len());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Captured %s viewport: %dx%d JPEG"), *ViewportType, TargetWidth, TargetHeight),
		ResultData
	);
}
