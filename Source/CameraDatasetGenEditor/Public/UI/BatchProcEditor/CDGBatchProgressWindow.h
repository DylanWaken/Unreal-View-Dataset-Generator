// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/BatchProcEditor/CDGBatchProcExecService.h"  // FBatchDetailedProgress

// ─────────────────────────────────────────────────────────────────────────────
// SBatchProgressWindow
//
// Floating always-on-top widget.  Layout:
//
// ┌──────────────────────────────────────────────────────────────────┐
// │  [Level 1/2] – [Anchor 1/3] – [Char 1/1] – [Anim 2/4]  [Cancel]│
// │  ────────────────────────────────────────────  Shot 0/6 — 37%   │
// │  [█████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] │
// │  Rendering Batch01_Anchor0_BP_Char_Walk …                        │
// └──────────────────────────────────────────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────
class SBatchProgressWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBatchProgressWindow)
		: _OnCancelRequested()
	{}
		/** Called when the user presses Cancel (or the window is closed). */
		SLATE_EVENT(FSimpleDelegate, OnCancelRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// ── State update API ────────────────────────────────────────────────────

	/** Full per-dimension update — call from OnDetailedProgressUpdated. */
	void UpdateDetailedProgress(const FBatchDetailedProgress& InProgress);

	/** Overall combo counter only (used as fallback / initial update). */
	void UpdateProgress(int32 Completed, int32 Total);

	/** Append a log message; the window shows only the last line. */
	void AddLog(const FString& Msg);

	/** Switch the Cancel button to "Close" and freeze the bar at 100 %. */
	void MarkCompleted(bool bSuccess);

	// ── Window helpers ──────────────────────────────────────────────────────

	TSharedPtr<SWindow> OpenWindow(TSharedPtr<SWindow> ParentWindow);
	void CloseWindow();

private:
	FBatchDetailedProgress Detail;
	bool            bDone = false;
	TArray<FString> Log;

	FSimpleDelegate OnCancelRequested;
	TWeakPtr<SWindow> OwningSWindow;

	// Helpers used by Text_Lambda attributes
	FText MakeBreakdownText() const;
	FText MakeShotPercentText() const;
};
