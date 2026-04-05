// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_LiveCoding.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"

#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

FMCPToolInfo FMCPTool_LiveCoding::GetInfo() const
{
	FMCPToolInfo Info;
	Info.Name = TEXT("live_coding");
	Info.Description = TEXT("Trigger Live Coding hot reload compilation (equivalent to Ctrl+Alt+F11)");

	Info.Parameters.Add(FMCPToolParameter(TEXT("operation"), TEXT("string"),
		TEXT("Operation: compile, status (default: compile)")));
	Info.Parameters.Add(FMCPToolParameter(TEXT("timeout"), TEXT("number"),
		TEXT("Max wait time in seconds for compilation (default: 60)")));

	Info.Annotations = FMCPToolAnnotations::Modifying();

	return Info;
}

FMCPToolResult FMCPTool_LiveCoding::Execute(const TSharedRef<FJsonObject>& Params)
{
#if WITH_LIVE_CODING
	FString Operation = TEXT("compile");
	if (Params->HasField(TEXT("operation")))
	{
		Operation = Params->GetStringField(TEXT("operation")).ToLower();
	}

	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>("LiveCoding");
	if (!LiveCoding)
	{
		return FMCPToolResult::Error(TEXT("Live Coding module not loaded"));
	}

	// Status check
	if (Operation == TEXT("status"))
	{
		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetBoolField(TEXT("enabled"), LiveCoding->IsEnabledForSession());
		ResultData->SetBoolField(TEXT("compiling"), LiveCoding->IsCompiling());

		return FMCPToolResult::Success(
			LiveCoding->IsEnabledForSession() ? TEXT("Live Coding is enabled") : TEXT("Live Coding is disabled"),
			ResultData
		);
	}

	// Compile
	if (Operation == TEXT("compile"))
	{
		if (!LiveCoding->IsEnabledForSession())
		{
			return FMCPToolResult::Error(TEXT("Live Coding is not enabled for this session. User must press Ctrl+Alt+F11 first to enable it."));
		}

		if (LiveCoding->IsCompiling())
		{
			return FMCPToolResult::Error(TEXT("Live Coding is already compiling"));
		}

		// Trigger compilation
		LiveCoding->Compile(ELiveCodingCompileFlags::None, nullptr);

		// Wait for compilation
		float MaxWait = 60.0f;
		if (Params->HasField(TEXT("timeout")))
		{
			MaxWait = FMath::Clamp((float)Params->GetNumberField(TEXT("timeout")), 5.0f, 300.0f);
		}

		float WaitTime = 0.0f;
		const float PollInterval = 0.5f;

		while (LiveCoding->IsCompiling() && WaitTime < MaxWait)
		{
			FPlatformProcess::Sleep(PollInterval);
			WaitTime += PollInterval;
		}

		if (WaitTime >= MaxWait)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Live Coding compilation timed out after %.0f seconds"), MaxWait));
		}

		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetNumberField(TEXT("compile_time_seconds"), WaitTime);
		ResultData->SetBoolField(TEXT("success"), true);

		return FMCPToolResult::Success(
			FString::Printf(TEXT("Live Coding compilation completed in %.1f seconds"), WaitTime),
			ResultData
		);
	}

	return FMCPToolResult::Error(FString::Printf(TEXT("Unknown operation: %s. Valid: compile, status"), *Operation));

#else
	return FMCPToolResult::Error(TEXT("Live Coding is not available in this build"));
#endif
}
