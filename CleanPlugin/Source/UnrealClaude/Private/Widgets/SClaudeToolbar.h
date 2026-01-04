// Copyright Your Name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE(FOnToolbarAction)
DECLARE_DELEGATE_OneParam(FOnCheckboxChanged, bool)

/**
 * Toolbar widget for Claude Editor
 * Handles UE context toggles, session management buttons
 */
class SClaudeToolbar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClaudeToolbar)
		: _bUE57ContextEnabled(true)
		, _bProjectContextEnabled(true)
		, _bRestoreEnabled(false)
	{}
		SLATE_ATTRIBUTE(bool, bUE57ContextEnabled)
		SLATE_ATTRIBUTE(bool, bProjectContextEnabled)
		SLATE_ATTRIBUTE(bool, bRestoreEnabled)
		SLATE_EVENT(FOnCheckboxChanged, OnUE57ContextChanged)
		SLATE_EVENT(FOnCheckboxChanged, OnProjectContextChanged)
		SLATE_EVENT(FOnToolbarAction, OnRefreshContext)
		SLATE_EVENT(FOnToolbarAction, OnRestoreSession)
		SLATE_EVENT(FOnToolbarAction, OnNewSession)
		SLATE_EVENT(FOnToolbarAction, OnClear)
		SLATE_EVENT(FOnToolbarAction, OnCopyLast)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TAttribute<bool> bUE57ContextEnabled;
	TAttribute<bool> bProjectContextEnabled;
	TAttribute<bool> bRestoreEnabled;

	FOnCheckboxChanged OnUE57ContextChanged;
	FOnCheckboxChanged OnProjectContextChanged;
	FOnToolbarAction OnRefreshContext;
	FOnToolbarAction OnRestoreSession;
	FOnToolbarAction OnNewSession;
	FOnToolbarAction OnClear;
	FOnToolbarAction OnCopyLast;
};
