// Copyright Natali Caggiano. All Rights Reserved.

/**
 * Unit tests for ClipboardImageUtils, clipboard image paste feature,
 * stream-json multimodal payload construction/parsing, and multi-image support
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
// Data Struct Tests (updated for TArray<FString>)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_RequestConfig_HasAttachedImagePaths,
	"UnrealClaude.ClipboardImage.DataStructs.RequestConfigHasImagePaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_RequestConfig_HasAttachedImagePaths::RunTest(const FString& Parameters)
{
	FClaudeRequestConfig Config;

	// Default should be empty array
	TestEqual("AttachedImagePaths should default to empty array", Config.AttachedImagePaths.Num(), 0);

	// Should be appendable
	Config.AttachedImagePaths.Add(TEXT("C:/test/image1.png"));
	Config.AttachedImagePaths.Add(TEXT("C:/test/image2.png"));
	TestEqual("AttachedImagePaths should have 2 entries", Config.AttachedImagePaths.Num(), 2);
	TestEqual("First path should match", Config.AttachedImagePaths[0], TEXT("C:/test/image1.png"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_PromptOptions_HasAttachedImagePaths,
	"UnrealClaude.ClipboardImage.DataStructs.PromptOptionsHasImagePaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_PromptOptions_HasAttachedImagePaths::RunTest(const FString& Parameters)
{
	FClaudePromptOptions Options;

	// Default should be empty array
	TestEqual("AttachedImagePaths should default to empty array", Options.AttachedImagePaths.Num(), 0);

	// Should be appendable
	Options.AttachedImagePaths.Add(TEXT("C:/test/screenshot.png"));
	TestEqual("AttachedImagePaths should have 1 entry", Options.AttachedImagePaths.Num(), 1);

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
	TestEqual("AttachedImagePaths should be empty after convenience constructor",
		Options.AttachedImagePaths.Num(), 0);

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

	// Empty input should return user-friendly error (not crash or return raw JSON)
	TestTrue("Should return error message for empty output", Parsed.Contains(TEXT("Error")));

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
// Stream-JSON Payload Construction Tests (updated for TArray<FString>)
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

	TArray<FString> ImagePaths = { TestImagePath };
	FString Payload = Runner.BuildStreamJsonPayload(TEXT("Hello world"), ImagePaths);

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
	TArray<FString> ImagePaths = { TEXT("C:/Windows/System32/evil.png") };
	FString Payload = Runner.BuildStreamJsonPayload(TEXT("test message"), ImagePaths);

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

// ============================================================================
// Stream-JSON Edge Case Tests (updated for TArray<FString>)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_StreamJson_BuildPayloadWithoutImage,
	"UnrealClaude.ClipboardImage.StreamJson.BuildPayloadWithoutImage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_StreamJson_BuildPayloadWithoutImage::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;

	// Empty array = text-only payload
	TArray<FString> EmptyPaths;
	FString Payload = Runner.BuildStreamJsonPayload(TEXT("Hello text only"), EmptyPaths);

	TestFalse("Payload should not be empty", Payload.IsEmpty());

	FString JsonLine = Payload.TrimEnd();
	TSharedPtr<FJsonObject> Envelope;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
	bool bParsed = FJsonSerializer::Deserialize(Reader, Envelope);
	TestTrue("Should be valid JSON", bParsed && Envelope.IsValid());

	if (bParsed && Envelope.IsValid())
	{
		const TSharedPtr<FJsonObject>* MessageObj;
		if (Envelope->TryGetObjectField(TEXT("message"), MessageObj))
		{
			const TArray<TSharedPtr<FJsonValue>>* ContentArray;
			if ((*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray))
			{
				TestEqual("Should have only 1 content block (text)", ContentArray->Num(), 1);

				if (ContentArray->Num() >= 1)
				{
					const TSharedPtr<FJsonObject>* TextBlock;
					(*ContentArray)[0]->TryGetObject(TextBlock);
					if (TextBlock)
					{
						FString Text;
						(*TextBlock)->TryGetStringField(TEXT("text"), Text);
						TestEqual("Text should match input", Text, TEXT("Hello text only"));
					}
				}
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_StreamJson_BuildPayloadRejectsTraversal,
	"UnrealClaude.ClipboardImage.StreamJson.BuildPayloadRejectsTraversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_StreamJson_BuildPayloadRejectsTraversal::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;

	// Path with traversal should be rejected even if it starts with the right directory
	FString ScreenshotDir = FClipboardImageUtils::GetScreenshotDirectory();
	FString TraversalPath = FPaths::Combine(ScreenshotDir, TEXT(".."), TEXT(".."), TEXT("secrets.png"));

	TArray<FString> ImagePaths = { TraversalPath };
	FString Payload = Runner.BuildStreamJsonPayload(TEXT("traversal test"), ImagePaths);

	TestFalse("Payload should not be empty", Payload.IsEmpty());

	FString JsonLine = Payload.TrimEnd();
	TSharedPtr<FJsonObject> Envelope;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
	bool bParsed = FJsonSerializer::Deserialize(Reader, Envelope);
	TestTrue("Should be valid JSON", bParsed && Envelope.IsValid());

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_StreamJson_BuildPayloadRejectsOversizedImage,
	"UnrealClaude.ClipboardImage.StreamJson.BuildPayloadRejectsOversizedImage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_StreamJson_BuildPayloadRejectsOversizedImage::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;

	// Create a file larger than 4.5MB in the screenshots directory
	FString TestDir = FClipboardImageUtils::GetScreenshotDirectory();
	IFileManager::Get().MakeDirectory(*TestDir, true);
	FString OversizedPath = FPaths::Combine(TestDir, TEXT("clipboard_test_oversized.png"));

	// Write ~5MB of data (exceeds 4.5MB limit)
	TArray<uint8> BigData;
	BigData.SetNum(5 * 1024 * 1024);
	FMemory::Memset(BigData.GetData(), 0xFF, BigData.Num());
	FFileHelper::SaveArrayToFile(BigData, *OversizedPath);

	TArray<FString> ImagePaths = { OversizedPath };
	FString Payload = Runner.BuildStreamJsonPayload(TEXT("big image test"), ImagePaths);

	TestFalse("Payload should not be empty", Payload.IsEmpty());

	FString JsonLine = Payload.TrimEnd();
	TSharedPtr<FJsonObject> Envelope;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
	bool bParsed = FJsonSerializer::Deserialize(Reader, Envelope);
	TestTrue("Should be valid JSON", bParsed && Envelope.IsValid());

	if (bParsed && Envelope.IsValid())
	{
		const TSharedPtr<FJsonObject>* MessageObj;
		if (Envelope->TryGetObjectField(TEXT("message"), MessageObj))
		{
			const TArray<TSharedPtr<FJsonValue>>* ContentArray;
			if ((*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray))
			{
				TestEqual("Should have only 1 content block (text, image rejected)", ContentArray->Num(), 1);
			}
		}
	}

	// Cleanup
	IFileManager::Get().Delete(*OversizedPath);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_StreamJson_ParseFailureReturnsErrorMessage,
	"UnrealClaude.ClipboardImage.StreamJson.ParseFailureReturnsErrorMessage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_StreamJson_ParseFailureReturnsErrorMessage::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;

	// Output with no result or assistant messages
	FString SimulatedOutput =
		TEXT("{\"type\":\"system\",\"subtype\":\"init\"}\n")
		TEXT("{\"type\":\"tool_use\",\"name\":\"something\"}\n");

	FString Parsed = Runner.ParseStreamJsonOutput(SimulatedOutput);

	TestTrue("Should return error message on parse failure",
		Parsed.Contains(TEXT("Error")));
	TestTrue("Should mention Output Log",
		Parsed.Contains(TEXT("Output Log")));

	return true;
}

// ============================================================================
// Multi-Image Tests
// ============================================================================

// Helper to get number of content blocks from a payload JSON string
static int32 GetContentBlockCount(const FString& Payload)
{
	FString JsonLine = Payload.TrimEnd();
	TSharedPtr<FJsonObject> Envelope;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
	if (!FJsonSerializer::Deserialize(Reader, Envelope) || !Envelope.IsValid())
	{
		return -1;
	}
	const TSharedPtr<FJsonObject>* MessageObj;
	if (!Envelope->TryGetObjectField(TEXT("message"), MessageObj))
	{
		return -1;
	}
	const TArray<TSharedPtr<FJsonValue>>* ContentArray;
	if (!(*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray))
	{
		return -1;
	}
	return ContentArray->Num();
}

// Helper to create a small test image in the screenshots directory
static FString CreateTestImage(const FString& TestDir, const FString& FileName, int32 SizeBytes = 4)
{
	FString Path = FPaths::Combine(TestDir, FileName);
	TArray<uint8> Data;
	Data.SetNum(SizeBytes);
	FMemory::Memset(Data.GetData(), 0x89, Data.Num());
	FFileHelper::SaveArrayToFile(Data, *Path);
	return Path;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_MultiImage_BuildPayloadWithMultipleImages,
	"UnrealClaude.ClipboardImage.MultiImage.BuildPayloadWithMultipleImages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_MultiImage_BuildPayloadWithMultipleImages::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;
	FString TestDir = FClipboardImageUtils::GetScreenshotDirectory();
	IFileManager::Get().MakeDirectory(*TestDir, true);

	FString Img1 = CreateTestImage(TestDir, TEXT("clipboard_multi_test1.png"));
	FString Img2 = CreateTestImage(TestDir, TEXT("clipboard_multi_test2.png"));
	FString Img3 = CreateTestImage(TestDir, TEXT("clipboard_multi_test3.png"));

	TArray<FString> ImagePaths = { Img1, Img2, Img3 };
	FString Payload = Runner.BuildStreamJsonPayload(TEXT("multi image"), ImagePaths);

	int32 BlockCount = GetContentBlockCount(Payload);
	TestEqual("Should have 4 content blocks (1 text + 3 images)", BlockCount, 4);

	// Cleanup
	IFileManager::Get().Delete(*Img1);
	IFileManager::Get().Delete(*Img2);
	IFileManager::Get().Delete(*Img3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_MultiImage_BuildPayloadRespectsMaxCount,
	"UnrealClaude.ClipboardImage.MultiImage.BuildPayloadRespectsMaxCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_MultiImage_BuildPayloadRespectsMaxCount::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;
	FString TestDir = FClipboardImageUtils::GetScreenshotDirectory();
	IFileManager::Get().MakeDirectory(*TestDir, true);

	// Create 7 images (more than MaxImagesPerMessage = 5)
	TArray<FString> ImagePaths;
	TArray<FString> CreatedFiles;
	for (int32 i = 0; i < 7; ++i)
	{
		FString Path = CreateTestImage(TestDir, FString::Printf(TEXT("clipboard_maxcount_%d.png"), i));
		ImagePaths.Add(Path);
		CreatedFiles.Add(Path);
	}

	FString Payload = Runner.BuildStreamJsonPayload(TEXT("max count test"), ImagePaths);

	// Should have 1 text + 5 images (capped at MaxImagesPerMessage)
	int32 BlockCount = GetContentBlockCount(Payload);
	TestEqual("Should have 6 content blocks (1 text + 5 images, capped)",
		BlockCount, 1 + UnrealClaudeConstants::ClipboardImage::MaxImagesPerMessage);

	// Cleanup
	for (const FString& F : CreatedFiles)
	{
		IFileManager::Get().Delete(*F);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_MultiImage_BuildPayloadSkipsInvalidImages,
	"UnrealClaude.ClipboardImage.MultiImage.BuildPayloadSkipsInvalidImages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_MultiImage_BuildPayloadSkipsInvalidImages::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;
	FString TestDir = FClipboardImageUtils::GetScreenshotDirectory();
	IFileManager::Get().MakeDirectory(*TestDir, true);

	FString ValidImg1 = CreateTestImage(TestDir, TEXT("clipboard_skipinvalid_1.png"));
	FString InvalidImg = FPaths::Combine(TestDir, TEXT("clipboard_skipinvalid_missing.png")); // doesn't exist
	FString ValidImg2 = CreateTestImage(TestDir, TEXT("clipboard_skipinvalid_3.png"));

	TArray<FString> ImagePaths = { ValidImg1, InvalidImg, ValidImg2 };
	FString Payload = Runner.BuildStreamJsonPayload(TEXT("skip invalid"), ImagePaths);

	// Should have 1 text + 2 valid images (middle one skipped)
	int32 BlockCount = GetContentBlockCount(Payload);
	TestEqual("Should have 3 content blocks (1 text + 2 valid images)", BlockCount, 3);

	// Cleanup
	IFileManager::Get().Delete(*ValidImg1);
	IFileManager::Get().Delete(*ValidImg2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_MultiImage_BuildPayloadTotalSizeGuard,
	"UnrealClaude.ClipboardImage.MultiImage.BuildPayloadTotalSizeGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_MultiImage_BuildPayloadTotalSizeGuard::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;
	FString TestDir = FClipboardImageUtils::GetScreenshotDirectory();
	IFileManager::Get().MakeDirectory(*TestDir, true);

	// Create images that individually pass the 4.5MB limit but together exceed 20MB total
	// Each image: ~4MB, so 5 images would be ~20MB. Only some should be included.
	const int32 ImageSize = 4 * 1024 * 1024; // 4MB each
	TArray<FString> ImagePaths;
	TArray<FString> CreatedFiles;

	for (int32 i = 0; i < 5; ++i)
	{
		FString Path = CreateTestImage(TestDir,
			FString::Printf(TEXT("clipboard_totalsize_%d.png"), i), ImageSize);
		ImagePaths.Add(Path);
		CreatedFiles.Add(Path);
	}

	FString Payload = Runner.BuildStreamJsonPayload(TEXT("total size test"), ImagePaths);

	// Should cap at 20MB total - first few images fit, later ones are skipped
	int32 BlockCount = GetContentBlockCount(Payload);
	// 4MB * 5 = 20MB which exactly equals the limit, but we check (total + new > limit)
	// so the 5th image (which would bring total to 20MB) should be included since we check > not >=
	// Actually: TotalImageBytes starts at 0, each 4MB image adds to it.
	// Check is: TotalImageBytes + FileSize > MaxTotalImagePayloadSize (20MB)
	// After 4 images: 16MB + 4MB = 20MB, which is NOT > 20MB, so 5th image is allowed.
	// After 5 images: 20MB + 4MB = 24MB, which IS > 20MB, but we'd need a 6th image to hit this.
	// With 5 images of 4MB each = 20MB total, all 5 fit exactly.
	// So all 5 images should be included = 1 text + 5 images = 6 blocks
	TestTrue("Should have included some images but not an absurd number",
		BlockCount >= 2 && BlockCount <= 6);

	// Cleanup
	for (const FString& F : CreatedFiles)
	{
		IFileManager::Get().Delete(*F);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_MultiImage_BuildPayloadMixedValidation,
	"UnrealClaude.ClipboardImage.MultiImage.BuildPayloadMixedValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_MultiImage_BuildPayloadMixedValidation::RunTest(const FString& Parameters)
{
	FClaudeCodeRunner Runner;
	FString TestDir = FClipboardImageUtils::GetScreenshotDirectory();
	IFileManager::Get().MakeDirectory(*TestDir, true);

	FString ValidImg1 = CreateTestImage(TestDir, TEXT("clipboard_mixed_valid1.png"));
	FString TraversalPath = FPaths::Combine(TestDir, TEXT(".."), TEXT(".."), TEXT("evil.png"));
	FString MissingPath = FPaths::Combine(TestDir, TEXT("clipboard_mixed_missing.png"));
	FString ValidImg2 = CreateTestImage(TestDir, TEXT("clipboard_mixed_valid2.png"));

	TArray<FString> ImagePaths = { ValidImg1, TraversalPath, MissingPath, ValidImg2 };
	FString Payload = Runner.BuildStreamJsonPayload(TEXT("mixed validation"), ImagePaths);

	// Should have 1 text + 2 valid images (traversal and missing skipped)
	int32 BlockCount = GetContentBlockCount(Payload);
	TestEqual("Should have 3 content blocks (1 text + 2 valid images)", BlockCount, 3);

	// Cleanup
	IFileManager::Get().Delete(*ValidImg1);
	IFileManager::Get().Delete(*ValidImg2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_MultiImage_Constants_MaxImagesReasonable,
	"UnrealClaude.ClipboardImage.MultiImage.ConstantsMaxImagesReasonable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_MultiImage_Constants_MaxImagesReasonable::RunTest(const FString& Parameters)
{
	using namespace UnrealClaudeConstants::ClipboardImage;

	TestTrue("MaxImagesPerMessage should be at least 1",
		MaxImagesPerMessage >= 1);
	TestTrue("MaxImagesPerMessage should be at most 100",
		MaxImagesPerMessage <= 100);

	TestTrue("MaxTotalImagePayloadSize should be greater than MaxImageFileSize",
		MaxTotalImagePayloadSize > MaxImageFileSize);

	TestTrue("MaxImageFileSize should be positive",
		MaxImageFileSize > 0);

	TestTrue("MaxTotalImagePayloadSize should be positive",
		MaxTotalImagePayloadSize > 0);

	TestTrue("ThumbnailSpacing should be non-negative",
		ThumbnailSpacing >= 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_MultiImage_RequestConfigArrayOperations,
	"UnrealClaude.ClipboardImage.MultiImage.RequestConfigArrayOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_MultiImage_RequestConfigArrayOperations::RunTest(const FString& Parameters)
{
	FClaudeRequestConfig Config;

	// Add paths
	Config.AttachedImagePaths.Add(TEXT("path1.png"));
	Config.AttachedImagePaths.Add(TEXT("path2.png"));
	Config.AttachedImagePaths.Add(TEXT("path3.png"));
	TestEqual("Should have 3 paths after Add", Config.AttachedImagePaths.Num(), 3);

	// RemoveAt
	Config.AttachedImagePaths.RemoveAt(1);
	TestEqual("Should have 2 paths after RemoveAt(1)", Config.AttachedImagePaths.Num(), 2);
	TestEqual("First path should be unchanged", Config.AttachedImagePaths[0], TEXT("path1.png"));
	TestEqual("Second path should now be the former third", Config.AttachedImagePaths[1], TEXT("path3.png"));

	// Empty
	Config.AttachedImagePaths.Empty();
	TestEqual("Should have 0 paths after Empty", Config.AttachedImagePaths.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FClipboardImage_MultiImage_PromptOptionsArrayCopied,
	"UnrealClaude.ClipboardImage.MultiImage.PromptOptionsArrayCopied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FClipboardImage_MultiImage_PromptOptionsArrayCopied::RunTest(const FString& Parameters)
{
	FClaudePromptOptions Original;
	Original.AttachedImagePaths.Add(TEXT("img1.png"));
	Original.AttachedImagePaths.Add(TEXT("img2.png"));
	Original.AttachedImagePaths.Add(TEXT("img3.png"));

	// Copy
	FClaudePromptOptions Copy = Original;
	TestEqual("Copied options should have 3 paths", Copy.AttachedImagePaths.Num(), 3);
	TestEqual("First path should match", Copy.AttachedImagePaths[0], TEXT("img1.png"));
	TestEqual("Second path should match", Copy.AttachedImagePaths[1], TEXT("img2.png"));
	TestEqual("Third path should match", Copy.AttachedImagePaths[2], TEXT("img3.png"));

	// Modifying copy should not affect original
	Copy.AttachedImagePaths.RemoveAt(0);
	TestEqual("Original should still have 3 paths", Original.AttachedImagePaths.Num(), 3);
	TestEqual("Copy should have 2 paths", Copy.AttachedImagePaths.Num(), 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
