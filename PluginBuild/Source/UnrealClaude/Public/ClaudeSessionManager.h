// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Manages Claude conversation session persistence and history
 * Single responsibility: session storage and retrieval
 */
class UNREALCLAUDE_API FClaudeSessionManager
{
public:
	FClaudeSessionManager();

	/** Get conversation history */
	const TArray<TPair<FString, FString>>& GetHistory() const { return ConversationHistory; }

	/** Add a new exchange to history */
	void AddExchange(const FString& Prompt, const FString& Response);

	/** Clear conversation history (in memory only) */
	void ClearHistory();

	/** Save current session to disk */
	bool SaveSession();

	/** Load previous session from disk */
	bool LoadSession();

	/** Check if a previous session exists */
	bool HasSavedSession() const;

	/** Get session file path */
	FString GetSessionFilePath() const;

	/** Get max history size */
	int32 GetMaxHistorySize() const { return MaxHistorySize; }

	/** Set max history size */
	void SetMaxHistorySize(int32 NewMax) { MaxHistorySize = FMath::Max(1, NewMax); }

private:
	TArray<TPair<FString, FString>> ConversationHistory;
	int32 MaxHistorySize;
};
