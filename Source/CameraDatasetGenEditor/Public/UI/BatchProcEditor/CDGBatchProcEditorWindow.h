// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "AssetRegistry/AssetData.h"

class UBatchProcConfig;
class UGeneratorStackConfig;
class ULevelSeqExportConfig;
class UCDGBatchProcExecService;

// ─────────────────────────────────────────────────────────────────────────────
// List item types
// ─────────────────────────────────────────────────────────────────────────────

struct FBatchLevelItem
{
	FAssetData AssetData;

	/** True if we confirmed at least one ACDGLevelSceneAnchor exists in this level */
	bool bHasAnchors    = true;
	/** True once the world was found in memory and the anchor check was performed */
	bool bAnchorChecked = false;

	explicit FBatchLevelItem(const FAssetData& InData) : AssetData(InData) {}
};

struct FBatchCharacterItem
{
	FAssetData AssetData;
	bool bHasMismatch = false;
	explicit FBatchCharacterItem(const FAssetData& InData) : AssetData(InData) {}
};

struct FBatchAnimationItem
{
	FAssetData AssetData;
	bool bHasMismatch = false;
	explicit FBatchAnimationItem(const FAssetData& InData) : AssetData(InData) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// SBatchProcEditorWindow
//
// Layout:
//   ┌────────────────┬────────────────┬────────────────────────────────┐
//   │  Levels        │  Characters    │  Animations                    │
//   │  [Level_A] [×] │  [SK_War] [×] │  [Walk]             [×]        │
//   │  [Level_B] [×] │  [SK_Mage][×] │  [Elven] ⚠ No skel [×]        │
//   │                │     ⚠ No anim │                                 │
//   │  Add: [_____]  │  Add: [_____]  │  Add: [_____]                  │
//   ├────────────────┴────────────────┴────────────────────────────────┤
//   │  Generator Config:   [asset picker ___________________________]  │
//   │  Exporter Config:    [asset picker ___________________________]  │
//   │  Batch Config Asset: [asset picker ___________________________]  │
//   │                                                                  │
//   │          [Export/Save Batch Config Asset]  [Start Batch Proc]   │
//   └──────────────────────────────────────────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────
class SBatchProcEditorWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBatchProcEditorWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// ── List row generators ──────────────────────────────────────────────────

	TSharedRef<ITableRow> GenerateLevelRow(
		TSharedPtr<FBatchLevelItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	TSharedRef<ITableRow> GenerateCharacterRow(
		TSharedPtr<FBatchCharacterItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	TSharedRef<ITableRow> GenerateAnimationRow(
		TSharedPtr<FBatchAnimationItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	// ── Staged (add-picker) callbacks ────────────────────────────────────────

	void OnStagedLevelChanged(const FAssetData& AssetData);
	void OnStagedCharacterChanged(const FAssetData& AssetData);
	void OnStagedAnimationChanged(const FAssetData& AssetData);

	FString GetStagedLevelPath() const;
	FString GetStagedCharacterPath() const;
	FString GetStagedCharacterBlueprintPath() const;
	FString GetStagedAnimationPath() const;

	// ── Remove callbacks ─────────────────────────────────────────────────────

	FReply OnRemoveLevelClicked(TSharedPtr<FBatchLevelItem> Item);
	FReply OnRemoveCharacterClicked(TSharedPtr<FBatchCharacterItem> Item);
	FReply OnRemoveAnimationClicked(TSharedPtr<FBatchAnimationItem> Item);

	// ── Config asset slots ───────────────────────────────────────────────────

	void   OnGeneratorConfigChanged(const FAssetData& AssetData);
	FString GetGeneratorConfigPath() const;

	void   OnExporterConfigChanged(const FAssetData& AssetData);
	FString GetExporterConfigPath() const;

	void   OnBatchConfigChanged(const FAssetData& AssetData);
	FString GetBatchConfigPath() const;

	// ── Action buttons ───────────────────────────────────────────────────────

	FReply OnSaveBatchConfigClicked();
	FReply OnStartBatchProcClicked();
	FReply OnCancelBatchProcClicked();

	// ── Helpers ──────────────────────────────────────────────────────────────

	/** Recompute bHasMismatch on all character and animation items and refresh lists. */
	void RefreshMismatchWarnings();

	/** Returns true when batch execution should be blocked: empty required lists or unresolved item warnings. */
	bool HasAnyWarnings() const;

	/** Returns the human-readable message to display in the warning banner. */
	FText GetWarningMessage() const;

	/**
	 * For each level in the list, check whether the corresponding UWorld (if it is
	 * already loaded in memory) contains at least one ACDGLevelSceneAnchor.
	 * Sets bHasAnchors / bAnchorChecked on each FBatchLevelItem and refreshes the list.
	 */
	void RefreshAnchorStatuses();

	/** Populate all three lists + config refs from a loaded UBatchProcConfig. */
	void PopulateFromConfig(UBatchProcConfig* Config);

	/** Write current UI state into a UBatchProcConfig and save its package. */
	void WriteToConfig(UBatchProcConfig* Config) const;

	// ── Data ─────────────────────────────────────────────────────────────────

	TArray<TSharedPtr<FBatchLevelItem>>     LevelItems;
	TArray<TSharedPtr<FBatchCharacterItem>> CharacterItems;
	TArray<TSharedPtr<FBatchAnimationItem>> AnimationItems;

	TSharedPtr<SListView<TSharedPtr<FBatchLevelItem>>>     LevelListView;
	TSharedPtr<SListView<TSharedPtr<FBatchCharacterItem>>> CharacterListView;
	TSharedPtr<SListView<TSharedPtr<FBatchAnimationItem>>> AnimationListView;

	// Transient staging paths for the add-pickers (always empty after add)
	FString StagedLevelPath;
	FString StagedCharacterPath;
	FString StagedCharacterBlueprintPath;
	FString StagedAnimationPath;

	// Currently selected config assets
	TWeakObjectPtr<UGeneratorStackConfig> GeneratorConfig;
	TWeakObjectPtr<ULevelSeqExportConfig> ExporterConfig;
	TWeakObjectPtr<UBatchProcConfig>      LoadedBatchConfig;

	// Active batch execution service (valid while a batch is running)
	TObjectPtr<UCDGBatchProcExecService> ActiveBatchService;

	// Progress display
	int32 BatchProgress      = 0;
	int32 BatchProgressTotal = 0;
	TArray<FString> BatchLog;
};

// ─────────────────────────────────────────────────────────────────────────────
// CDGBatchProcEditor — utility to open the window
// ─────────────────────────────────────────────────────────────────────────────
class CDGBatchProcEditor
{
public:
	static void OpenWindow();
};
