// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_CaptureViewport.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeConstants.h"
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
	constexpr int32 TargetWidth = 1024;
	constexpr int32 TargetHeight = 576;
	constexpr int32 JPEGQuality = 70;

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
	if (!GEditor)
	{
		return FMCPToolResult::Error(TEXT("Editor is not available."));
	}

	// Prefer PIE viewport when active; falls back to the editor viewport
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

	const FIntPoint ViewportSize = Viewport->GetSizeXY();
	const int64 ViewportPixelCount = static_cast<int64>(ViewportSize.X) * static_cast<int64>(ViewportSize.Y);
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return FMCPToolResult::Error(TEXT("Viewport has invalid size."));
	}
	if (ViewportPixelCount > UnrealClaudeConstants::MCPServer::MaxViewportPixels)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Viewport is too large to capture safely (%lld pixels, maximum %lld). Resize the viewport and retry."),
			ViewportPixelCount, UnrealClaudeConstants::MCPServer::MaxViewportPixels));
	}

	TArray<FColor> Pixels;
	if (!Viewport->ReadPixels(Pixels))
	{
		return FMCPToolResult::Error(TEXT("Failed to read viewport pixels."));
	}

	const int32 ExpectedPixels = ViewportSize.X * ViewportSize.Y;
	if (Pixels.Num() != ExpectedPixels)
	{
		return FMCPToolResult::Error(TEXT("Pixel array size mismatch."));
	}

	TArray<FColor> ResizedPixels;
	ResizePixels(Pixels, ViewportSize.X, ViewportSize.Y, ResizedPixels, TargetWidth, TargetHeight);

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (!ImageWrapper.IsValid())
	{
		return FMCPToolResult::Error(TEXT("Failed to create image wrapper."));
	}

	// FColor is BGRA on little-endian platforms — declare BGRA so JPEG encoder doesn't swap channels
	if (!ImageWrapper->SetRaw(ResizedPixels.GetData(), ResizedPixels.Num() * sizeof(FColor),
		TargetWidth, TargetHeight, ERGBFormat::BGRA, 8))
	{
		return FMCPToolResult::Error(TEXT("Failed to set image data."));
	}

	// UE 5.7 ImageWrapper returns TArray64 directly (not via out-param like 5.6 and earlier)
	TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(JPEGQuality);
	if (CompressedData.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("Failed to compress image to JPEG."));
	}

	const FString Base64Image = FBase64::Encode(CompressedData.GetData(), CompressedData.Num());

	// Sidecar metadata. image_base64 is no longer set here — BuildToolResultJson
	// emits it as a backward-compat shim alongside the canonical `base64` field.
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetNumberField(TEXT("width"), TargetWidth);
	ResultData->SetNumberField(TEXT("height"), TargetHeight);
	ResultData->SetStringField(TEXT("format"), TEXT("jpeg"));
	ResultData->SetNumberField(TEXT("quality"), JPEGQuality);
	ResultData->SetStringField(TEXT("viewport_type"), ViewportType);
	ResultData->SetNumberField(TEXT("original_width"), ViewportSize.X);
	ResultData->SetNumberField(TEXT("original_height"), ViewportSize.Y);

	UE_LOG(LogUnrealClaude, Log, TEXT("Captured %s viewport: %dx%d -> %dx%d JPEG (%d bytes base64)"),
		*ViewportType, ViewportSize.X, ViewportSize.Y, TargetWidth, TargetHeight, Base64Image.Len());

	return FMCPToolResult::Image(
		Base64Image,
		TEXT("image/jpeg"),
		FString::Printf(TEXT("Captured %s viewport: %dx%d JPEG"), *ViewportType, TargetWidth, TargetHeight),
		ResultData
	);
}
