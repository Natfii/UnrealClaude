// Copyright Natali Caggiano. All Rights Reserved.

/**
 * Unit tests for ClipboardImageUtils, clipboard image paste feature,
 * and stream-json multimodal payload construction/parsing
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Base64.h"
#include "HAL/FileManager.h"
#include "ClipboardImageUtils.h"
#include "ClaudeCodeRunner.h"
#include "UnrealClaudeConstants.h"
#include "IClaudeRunner.h"
#include "ClaudeSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// Screenshot Directory Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_GetScreenshotDirectory_ReturnsValidPath,
	"UnrealClaude.ClipboardImage.GetScreenshotDirectory.ReturnsValidPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_GetScreenshotDirectory_ReturnsValidPath::RunTest(const FString& Parameters)
{
	FString Dir = FClipboardImageUtils::GetScreenshotDirectory();

	TestFalse("Screenshot directory should not be empty", Dir.IsEmpty());
	TestTrue("Should contain UnrealClaude", Dir.Contains(TEXT("UnrealClaude")));
	TestTrue("Should contain screenshots subdirectory", Dir.Contains(TEXT("screenshots")));
	TestTrue("Should be under Saved directory", Dir.Contains(TEXT("Saved")));

	return true;
}

// ============================================================================
// Cleanup Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_Cleanup_DeletesOldFiles,
	"UnrealClaude.ClipboardImage.Cleanup.DeletesOldFiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_Cleanup_DeletesOldFiles::RunTest(const FString& Parameters)
{
	// Create a temp directory for testing
	FString TestDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealClaude"), TEXT("test_screenshots"));
	IFileManager::Get().MakeDirectory(*TestDir, true);

	// Create test files
	FString OldFile = FPaths::Combine(TestDir, TEXT("clipboard_20200101_120000.png"));
	FString NewFile = FPaths::Combine(TestDir, TEXT("clipboard_99991231_235959.png"));

	FFileHelper::SaveStringToFile(TEXT("old"), *OldFile);
	FFileHelper::SaveStringToFile(TEXT("new"), *NewFile);

	// Backdate the "old" file's modification timestamp so cleanup treats it as expired
	IFileManager::Get().SetTimeStamp(*OldFile, FDateTime(2020, 1, 1));

	TestTrue("Old file should exist before cleanup", FPaths::FileExists(OldFile));
	TestTrue("New file should exist before cleanup", FPaths::FileExists(NewFile));

	// Cleanup with 1 hour max age - the backdated file should be deleted
	FClipboardImageUtils::CleanupOldScreenshots(TestDir, 3600.0);

	TestFalse("Old file should be deleted after cleanup", FPaths::FileExists(OldFile));
	// New file might or might not exist depending on timing, so we don't assert on it

	// Cleanup test directory
	IFileManager::Get().DeleteDirectory(*TestDir, false, true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_Cleanup_IgnoresNonClipboardFiles,
	"UnrealClaude.ClipboardImage.Cleanup.IgnoresNonClipboardFiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_Cleanup_IgnoresNonClipboardFiles::RunTest(const FString& Parameters)
{
	FString TestDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealClaude"), TEXT("test_screenshots2"));
	IFileManager::Get().MakeDirectory(*TestDir, true);

	// Create a non-clipboard file
	FString OtherFile = FPaths::Combine(TestDir, TEXT("important_data.png"));
	FFileHelper::SaveStringToFile(TEXT("keep me"), *OtherFile);

	// Cleanup with very short max age
	FClipboardImageUtils::CleanupOldScreenshots(TestDir, 0.0);

	TestTrue("Non-clipboard file should survive cleanup", FPaths::FileExists(OtherFile));

	// Cleanup
	IFileManager::Get().DeleteDirectory(*TestDir, false, true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_Cleanup_HandlesNonExistentDirectory,
	"UnrealClaude.ClipboardImage.Cleanup.HandlesNonExistentDirectory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_Cleanup_HandlesNonExistentDirectory::RunTest(const FString& Parameters)
{
	// Should not crash on non-existent directory
	FClipboardImageUtils::CleanupOldScreenshots(TEXT("C:/NonExistent/Path/That/Does/Not/Exist"), 1.0);

	// If we get here without crashing, the test passes
	TestTrue("Should handle non-existent directory gracefully", true);

	return true;
}

// ============================================================================
// Constants Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_Constants_ReasonableValues,
	"UnrealClaude.ClipboardImage.Constants.ReasonableValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_Constants_ReasonableValues::RunTest(const FString& Parameters)
{
	TestTrue("MaxScreenshotAgeSeconds should be positive",
		UnrealClaudeConstants::ClipboardImage::MaxScreenshotAgeSeconds > 0.0);

	TestTrue("MaxScreenshotAgeSeconds should be at least 60 seconds",
		UnrealClaudeConstants::ClipboardImage::MaxScreenshotAgeSeconds >= 60.0);

	TestTrue("ThumbnailSize should be positive",
		UnrealClaudeConstants::ClipboardImage::ThumbnailSize > 0.0f);

	TestTrue("ThumbnailSize should be reasonable (16-256px)",
		UnrealClaudeConstants::ClipboardImage::ThumbnailSize >= 16.0f &&
		UnrealClaudeConstants::ClipboardImage::ThumbnailSize <= 256.0f);

	TestTrue("ScreenshotSubdirectory should not be null",
		UnrealClaudeConstants::ClipboardImage::ScreenshotSubdirectory != nullptr);

	return true;
}

// ============================================================================
// Data Struct Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_RequestConfig_HasAttachedImagePath,
	"UnrealClaude.ClipboardImage.DataStructs.RequestConfigHasImagePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_RequestConfig_HasAttachedImagePath::RunTest(const FString& Parameters)
{
	FClaudeRequestConfig Config;

	// Default should be empty
	TestTrue("AttachedImagePath should default to empty", Config.AttachedImagePath.IsEmpty());

	// Should be assignable
	Config.AttachedImagePath = TEXT("C:/test/image.png");
	TestEqual("AttachedImagePath should be assignable",
		Config.AttachedImagePath, TEXT("C:/test/image.png"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_PromptOptions_HasAttachedImagePath,
	"UnrealClaude.ClipboardImage.DataStructs.PromptOptionsHasImagePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_PromptOptions_HasAttachedImagePath::RunTest(const FString& Parameters)
{
	FClaudePromptOptions Options;

	// Default should be empty
	TestTrue("AttachedImagePath should default to empty", Options.AttachedImagePath.IsEmpty());

	// Should be assignable
	Options.AttachedImagePath = TEXT("C:/test/screenshot.png");
	TestEqual("AttachedImagePath should be assignable",
		Options.AttachedImagePath, TEXT("C:/test/screenshot.png"));

	// Default constructor values should be preserved
	TestTrue("bIncludeEngineContext should default to true", Options.bIncludeEngineContext);
	TestTrue("bIncludeProjectContext should default to true", Options.bIncludeProjectContext);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_PromptOptions_ConvenienceConstructor,
	"UnrealClaude.ClipboardImage.DataStructs.ConvenienceConstructorPreservesDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_PromptOptions_ConvenienceConstructor::RunTest(const FString& Parameters)
{
	FClaudePromptOptions Options(true, false);

	TestTrue("bIncludeEngineContext should be true", Options.bIncludeEngineContext);
	TestFalse("bIncludeProjectContext should be false", Options.bIncludeProjectContext);
	TestTrue("AttachedImagePath should be empty after convenience constructor",
		Options.AttachedImagePath.IsEmpty());

	return true;
}

// ============================================================================
// Path Validation Tests (security)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_PathValidation_ScreenshotDirIsUnderSaved,
	"UnrealClaude.ClipboardImage.PathValidation.ScreenshotDirIsUnderSaved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_PathValidation_ScreenshotDirIsUnderSaved::RunTest(const FString& Parameters)
{
	FString ScreenshotDir = FClipboardImageUtils::GetScreenshotDirectory();
	FString SavedDir = FPaths::ProjectSavedDir();

	FString FullScreenshotDir = FPaths::ConvertRelativePathToFull(ScreenshotDir);
	FString FullSavedDir = FPaths::ConvertRelativePathToFull(SavedDir);

	TestTrue("Screenshot directory should be under Saved/",
		FullScreenshotDir.StartsWith(FullSavedDir));

	return true;
}

// ============================================================================
// ClipboardHasImage Tests (platform-dependent)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_ClipboardHasImage_DoesNotCrash,
	"UnrealClaude.ClipboardImage.ClipboardHasImage.DoesNotCrash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_ClipboardHasImage_DoesNotCrash::RunTest(const FString& Parameters)
{
	// Just verify it doesn't crash - result depends on clipboard state
	bool bResult = FClipboardImageUtils::ClipboardHasImage();
	// bResult can be true or false depending on clipboard contents
	// The important thing is it doesn't crash
	TestTrue("ClipboardHasImage should return without crashing", true);

	return true;
}

// ============================================================================
// Stream-JSON Output Parsing Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_StreamJson_ParseResultMessage,
	"UnrealClaude.ClipboardImage.StreamJson.ParseResultMessage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_StreamJson_ParseResultMessage::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;

	// Simulate stream-json output with a result message
	FString SimulatedOutput =
		TEXT("{\"type\":\"system\",\"subtype\":\"init\",\"session_id\":\"test\"}\n")
		TEXT("{\"type\":\"assistant\",\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"partial\"}]}}\n")
		TEXT("{\"type\":\"result\",\"subtype\":\"success\",\"result\":\"This is the final response text.\",\"cost_usd\":0.01}\n");

	FString Parsed = Runner.ParseStreamJsonOutput(SimulatedOutput);

	TestEqual("Should extract result text", Parsed, TEXT("This is the final response text."));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_StreamJson_FallbackToAssistantBlocks,
	"UnrealClaude.ClipboardImage.StreamJson.FallbackToAssistantBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_StreamJson_FallbackToAssistantBlocks::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;

	// Simulate output with no result message, only assistant content
	FString SimulatedOutput =
		TEXT("{\"type\":\"system\",\"subtype\":\"init\"}\n")
		TEXT("{\"type\":\"assistant\",\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"Hello from assistant.\"}]}}\n");

	FString Parsed = Runner.ParseStreamJsonOutput(SimulatedOutput);

	TestEqual("Should extract assistant text as fallback", Parsed, TEXT("Hello from assistant."));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_StreamJson_HandleEmptyOutput,
	"UnrealClaude.ClipboardImage.StreamJson.HandleEmptyOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_StreamJson_HandleEmptyOutput::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;

	FString Parsed = Runner.ParseStreamJsonOutput(TEXT(""));

	// Empty input should return empty (the raw output)
	TestTrue("Should handle empty output gracefully", Parsed.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_StreamJson_HandleMalformedJson,
	"UnrealClaude.ClipboardImage.StreamJson.HandleMalformedJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_StreamJson_HandleMalformedJson::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;

	FString SimulatedOutput =
		TEXT("not valid json\n")
		TEXT("{broken json{{\n")
		TEXT("{\"type\":\"result\",\"result\":\"Found it despite bad lines.\"}\n");

	FString Parsed = Runner.ParseStreamJsonOutput(SimulatedOutput);

	TestEqual("Should skip malformed lines and find result", Parsed, TEXT("Found it despite bad lines."));

	return true;
}

// ============================================================================
// Stream-JSON Payload Construction Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_StreamJson_BuildPayloadWithImage,
	"UnrealClaude.ClipboardImage.StreamJson.BuildPayloadWithImage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_StreamJson_BuildPayloadWithImage::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;

	// Create a small test PNG file in the screenshots directory
	FString TestDir = FClipboardImageUtils::GetScreenshotDirectory();
	IFileManager::Get().MakeDirectory(*TestDir, true);
	FString TestImagePath = FPaths::Combine(TestDir, TEXT("clipboard_test_payload.png"));

	// Write minimal data (doesn't need to be a real PNG for JSON construction test)
	TArray<uint8> FakeImageData;
	FakeImageData.Add(0x89); FakeImageData.Add(0x50); FakeImageData.Add(0x4E); FakeImageData.Add(0x47);
	FFileHelper::SaveArrayToFile(FakeImageData, *TestImagePath);

	FString Payload = Runner.BuildStreamJsonPayload(TEXT("Hello world"), TestImagePath);

	// Verify it's valid JSON (single line NDJSON)
	TestFalse("Payload should not be empty", Payload.IsEmpty());

	// Parse the JSON
	FString JsonLine = Payload.TrimEnd();
	TSharedPtr<FJsonObject> Envelope;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
	bool bParsed = FJsonSerializer::Deserialize(Reader, Envelope);

	TestTrue("Payload should be valid JSON", bParsed && Envelope.IsValid());

	if (bParsed && Envelope.IsValid())
	{
		// Check envelope structure
		FString EnvType;
		TestTrue("Should have type field", Envelope->TryGetStringField(TEXT("type"), EnvType));
		TestEqual("Envelope type should be 'user'", EnvType, TEXT("user"));

		// Check message
		const TSharedPtr<FJsonObject>* MessageObj;
		TestTrue("Should have message field", Envelope->TryGetObjectField(TEXT("message"), MessageObj));
		if (MessageObj)
		{
			FString Role;
			(*MessageObj)->TryGetStringField(TEXT("role"), Role);
			TestEqual("Role should be 'user'", Role, TEXT("user"));

			// Check content array
			const TArray<TSharedPtr<FJsonValue>>* ContentArray;
			TestTrue("Should have content array", (*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray));
			if (ContentArray)
			{
				TestEqual("Should have 2 content blocks (text + image)", ContentArray->Num(), 2);

				if (ContentArray->Num() >= 2)
				{
					// First block: text
					const TSharedPtr<FJsonObject>* TextBlock;
					(*ContentArray)[0]->TryGetObject(TextBlock);
					if (TextBlock)
					{
						FString BlockType;
						(*TextBlock)->TryGetStringField(TEXT("type"), BlockType);
						TestEqual("First block type should be 'text'", BlockType, TEXT("text"));
					}

					// Second block: image
					const TSharedPtr<FJsonObject>* ImageBlock;
					(*ContentArray)[1]->TryGetObject(ImageBlock);
					if (ImageBlock)
					{
						FString BlockType;
						(*ImageBlock)->TryGetStringField(TEXT("type"), BlockType);
						TestEqual("Second block type should be 'image'", BlockType, TEXT("image"));

						const TSharedPtr<FJsonObject>* SourceObj;
						if ((*ImageBlock)->TryGetObjectField(TEXT("source"), SourceObj))
						{
							FString MediaType;
							(*SourceObj)->TryGetStringField(TEXT("media_type"), MediaType);
							TestEqual("Media type should be 'image/png'", MediaType, TEXT("image/png"));

							FString SourceType;
							(*SourceObj)->TryGetStringField(TEXT("type"), SourceType);
							TestEqual("Source type should be 'base64'", SourceType, TEXT("base64"));

							FString Data;
							TestTrue("Should have data field", (*SourceObj)->TryGetStringField(TEXT("data"), Data));
							TestFalse("Base64 data should not be empty", Data.IsEmpty());
						}
					}
				}
			}
		}
	}

	// Cleanup test file
	IFileManager::Get().Delete(*TestImagePath);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_StreamJson_BuildPayloadRejectsInvalidPath,
	"UnrealClaude.ClipboardImage.StreamJson.BuildPayloadRejectsInvalidPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_StreamJson_BuildPayloadRejectsInvalidPath::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;

	// Path outside screenshots directory should be rejected
	FString Payload = Runner.BuildStreamJsonPayload(TEXT("test message"), TEXT("C:/Windows/System32/evil.png"));

	// Should still produce valid NDJSON with text block only (no image)
	TestFalse("Payload should not be empty", Payload.IsEmpty());

	FString JsonLine = Payload.TrimEnd();
	TSharedPtr<FJsonObject> Envelope;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
	bool bParsed = FJsonSerializer::Deserialize(Reader, Envelope);
	TestTrue("Should be valid JSON even without image", bParsed && Envelope.IsValid());

	if (bParsed && Envelope.IsValid())
	{
		const TSharedPtr<FJsonObject>* MessageObj;
		if (Envelope->TryGetObjectField(TEXT("message"), MessageObj))
		{
			const TArray<TSharedPtr<FJsonValue>>* ContentArray;
			if ((*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray))
			{
				TestEqual("Should have only 1 content block (text, no image)", ContentArray->Num(), 1);
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
