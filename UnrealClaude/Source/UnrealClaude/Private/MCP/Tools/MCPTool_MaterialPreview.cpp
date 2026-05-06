// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_MaterialPreview.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"

#include "ObjectTools.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ObjectThumbnail.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Misc/Base64.h"

FMCPToolInfo FMCPTool_MaterialPreview::GetInfo() const
{
	FMCPToolInfo Info;
	Info.Name = TEXT("material_preview");
	Info.Description = TEXT("Capture a material preview image (like the material editor preview sphere)");

	Info.Parameters.Add(FMCPToolParameter(TEXT("material_path"), TEXT("string"),
		TEXT("Asset path to the material (e.g., /Game/Materials/M_MyMaterial)"), true));
	Info.Parameters.Add(FMCPToolParameter(TEXT("size"), TEXT("integer"),
		TEXT("Preview image size in pixels (default: 256, max: 1024)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("quality"), TEXT("integer"),
		TEXT("JPEG quality 1-100 (default: 85)")));

	Info.Annotations = FMCPToolAnnotations::ReadOnly();

	return Info;
}

FMCPToolResult FMCPTool_MaterialPreview::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Extract material path
	FString MaterialPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("material_path"), MaterialPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(MaterialPath, Error))
	{
		return Error.GetValue();
	}

	// Get optional parameters
	int32 ImageSize = 256;
	if (Params->HasField(TEXT("size")))
	{
		ImageSize = FMath::Clamp(Params->GetIntegerField(TEXT("size")), 64, 1024);
	}

	int32 Quality = 85;
	if (Params->HasField(TEXT("quality")))
	{
		Quality = FMath::Clamp(Params->GetIntegerField(TEXT("quality")), 1, 100);
	}

	// Load the material
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Material)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
	}

	// Render thumbnail
	FObjectThumbnail Thumbnail;
	ThumbnailTools::RenderThumbnail(
		Material,
		ImageSize,
		ImageSize,
		ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush,
		nullptr,  // Use scratch render target
		&Thumbnail
	);

	// Get raw pixel data
	const TArray<uint8>& RawData = Thumbnail.GetUncompressedImageData();
	if (RawData.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("Failed to render material preview - no image data returned"));
	}

	// Compress to JPEG
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (!ImageWrapper->SetRaw(
		RawData.GetData(),
		RawData.Num(),
		Thumbnail.GetImageWidth(),
		Thumbnail.GetImageHeight(),
		ERGBFormat::BGRA,
		8))
	{
		return FMCPToolResult::Error(TEXT("Failed to encode image data"));
	}

	TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(Quality);
	if (CompressedData.Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("Failed to compress image to JPEG"));
	}

	// Encode as base64
	FString Base64Image = FBase64::Encode(CompressedData.GetData(), CompressedData.Num());

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("image_base64"), Base64Image);
	ResultData->SetNumberField(TEXT("width"), Thumbnail.GetImageWidth());
	ResultData->SetNumberField(TEXT("height"), Thumbnail.GetImageHeight());
	ResultData->SetStringField(TEXT("format"), TEXT("jpeg"));
	ResultData->SetStringField(TEXT("material_name"), Material->GetName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Captured %dx%d preview of %s"), Thumbnail.GetImageWidth(), Thumbnail.GetImageHeight(), *Material->GetName()),
		ResultData
	);
}
