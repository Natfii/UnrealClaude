// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCPToolBase.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Ticker.h"

/**
 * Status of an async MCP task
 */
enum class EMCPTaskStatus : uint8
{
	/** Task is queued but not yet started */
	Pending,
	/** Task is currently executing */
	Running,
	/** Task completed successfully */
	Completed,
	/** Task failed with an error */
	Failed,
	/** Task was cancelled */
	Cancelled,
	/** Task timed out */
	TimedOut
};

/**
 * Represents an async MCP task that can be submitted and polled for results
 */
struct FMCPAsyncTask
{
	/** Unique task identifier */
	FGuid TaskId;

	/** Name of the tool being executed */
	FString ToolName;

	/** Parameters passed to the tool */
	TSharedPtr<FJsonObject> Parameters;

	/** Current status of the task */
	TAtomic<EMCPTaskStatus> Status;

	/** Result of the task execution (valid when Completed or Failed) */
	FMCPToolResult Result;

	/** Progress percentage (0-100), -1 if not applicable */
	TAtomic<int32> Progress;

	/** Optional progress message */
	FString ProgressMessage;

	/** When the task was submitted */
	FDateTime SubmittedTime;

	/** When the task started executing */
	FDateTime StartedTime;

	/** When the task completed */
	FDateTime CompletedTime;

	/** Timeout in milliseconds (0 = use default) */
	uint32 TimeoutMs;

	/** Flag to request cancellation */
	FThreadSafeBool bCancellationRequested;

	/** Protects non-atomic state and result data. */
	mutable FCriticalSection StateLock;

	/** Event used while a task waits for game-thread execution. */
	TSharedPtr<FEvent, ESPMode::ThreadSafe> GameThreadCompletionEvent;

	/** Pending game-thread ticker callback, if one exists. */
	FDelegateHandle GameThreadTickerHandle;

	FMCPAsyncTask()
		: Status(EMCPTaskStatus::Pending)
		, Progress(-1)
		, SubmittedTime(FDateTime::UtcNow())
		, TimeoutMs(0)
		, bCancellationRequested(false)
	{
		TaskId = FGuid::NewGuid();
	}

	/** Get status as string for JSON serialization */
	static FString StatusToString(EMCPTaskStatus InStatus)
	{
		switch (InStatus)
		{
		case EMCPTaskStatus::Pending: return TEXT("pending");
		case EMCPTaskStatus::Running: return TEXT("running");
		case EMCPTaskStatus::Completed: return TEXT("completed");
		case EMCPTaskStatus::Failed: return TEXT("failed");
		case EMCPTaskStatus::Cancelled: return TEXT("cancelled");
		case EMCPTaskStatus::TimedOut: return TEXT("timed_out");
		default: return TEXT("unknown");
		}
	}

	/** Check if task is in a terminal state */
	bool IsComplete() const
	{
		EMCPTaskStatus CurrentStatus = Status.Load();
		return CurrentStatus == EMCPTaskStatus::Completed ||
			   CurrentStatus == EMCPTaskStatus::Failed ||
			   CurrentStatus == EMCPTaskStatus::Cancelled ||
			   CurrentStatus == EMCPTaskStatus::TimedOut;
	}

	/** Convert task info to JSON for API responses */
	TSharedPtr<FJsonObject> ToJson(bool bIncludeResult = false) const
	{
		FScopeLock StateScopeLock(&StateLock);
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("task_id"), TaskId.ToString());
		Json->SetStringField(TEXT("tool_name"), ToolName);
		Json->SetStringField(TEXT("status"), StatusToString(Status.Load()));
		Json->SetNumberField(TEXT("progress"), Progress.Load());

		if (!ProgressMessage.IsEmpty())
		{
			Json->SetStringField(TEXT("progress_message"), ProgressMessage);
		}

		Json->SetStringField(TEXT("submitted_at"), SubmittedTime.ToIso8601());

		if (Status.Load() != EMCPTaskStatus::Pending)
		{
			Json->SetStringField(TEXT("started_at"), StartedTime.ToIso8601());
		}

		if (IsComplete())
		{
			Json->SetStringField(TEXT("completed_at"), CompletedTime.ToIso8601());

			if (StartedTime > FDateTime::MinValue())
			{
				FTimespan Duration = CompletedTime - StartedTime;
				Json->SetNumberField(TEXT("duration_ms"), Duration.GetTotalMilliseconds());
			}

			if (bIncludeResult)
			{
				Json->SetBoolField(TEXT("success"), Result.bSuccess);
				Json->SetStringField(TEXT("message"), Result.Message);
				if (Result.Data.IsValid())
				{
					Json->SetObjectField(TEXT("data"), Result.Data);
				}
				if (Result.Warnings.Num() > 0)
				{
					TArray<TSharedPtr<FJsonValue>> WarningsJson;
					for (const FString& Warning : Result.Warnings)
					{
						WarningsJson.Add(MakeShared<FJsonValueString>(Warning));
					}
					Json->SetArrayField(TEXT("warnings"), WarningsJson);
				}
			}
		}

		return Json;
	}
};
