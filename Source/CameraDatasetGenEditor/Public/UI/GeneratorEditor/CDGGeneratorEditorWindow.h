// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/SListView.h"

class UCDGTrajectoryGenerator;
class UCDGPositioningGenerator;
class UCDGMovementGenerator;
class UCDGEffectsGenerator;
class ULevelSequence;
class UGeneratorStackConfig;
class IDetailsView;
struct FAssetData;

// ─────────────────────────────────────────────────────────────────────────────
// FGeneratorClassEntry — one entry in a stage's "type to add" combo box
// ─────────────────────────────────────────────────────────────────────────────
struct FGeneratorClassEntry
{
	UClass* Class = nullptr;
	FText   DisplayName;

	FGeneratorClassEntry() = default;
	FGeneratorClassEntry(UClass* InClass, const FText& InName)
		: Class(InClass), DisplayName(InName) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// FGeneratorStackItem — one row in a stage's list
// ─────────────────────────────────────────────────────────────────────────────
struct FGeneratorStackItem
{
	UCDGTrajectoryGenerator* Generator = nullptr;

	explicit FGeneratorStackItem(UCDGTrajectoryGenerator* InGenerator)
		: Generator(InGenerator) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// SGeneratorEditorWindow
//
// Slate window for configuring and running the three-stage trajectory pipeline:
//
//   POSITIONING  →  MOVEMENT  →  EFFECTS
//
// Layout:
//   ┌────────────────────────────────────────────────────────────────────────┐
//   │  [Positioning Stack] [Movement Stack] [Effects Stack]                  │
//   │  Each stack has: [TypeCombo][Add] — list — [Remove]                   │
//   ├────────────────────────────────────────────────────────────────────────┤
//   │  Generator Configuration (IDetailsView for the selected generator)     │
//   ├────────────────────────────────────────────────────────────────────────┤
//   │  Config Asset: [picker] [Save Config]                                  │
//   │  Reference Sequence: [picker]  [Let Batch Processor Fill]              │
//   │  [Generate]  [Clear All]              [Export Config]  [Load Config]   │
//   └────────────────────────────────────────────────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────
class SGeneratorEditorWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGeneratorEditorWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGeneratorEditorWindow();

private:
	// ── Stage stack helpers ───────────────────────────────────────────────────

	/** Build one complete stage panel (title + combo + list + remove button). */
	TSharedRef<SWidget> MakeStagePanel(
		const FText& StageTitle,
		TArray<TSharedPtr<FGeneratorClassEntry>>&                    InAvailableClasses,
		TSharedPtr<FGeneratorClassEntry>&                            InSelectedAddClass,
		TArray<TSharedPtr<FGeneratorStackItem>>&                     InItems,
		TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>&      OutListView);

	/** Row factory used by all three list views. */
	TSharedRef<ITableRow> GenerateStackRow(
		TSharedPtr<FGeneratorStackItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	/** Shared selection handler — updates DetailsView and tracks ActiveItems. */
	void OnStackSelectionChanged(
		TSharedPtr<FGeneratorStackItem> Item,
		ESelectInfo::Type SelectType,
		TArray<TSharedPtr<FGeneratorStackItem>>* OwnerArray);

	// ── Per-stage combo box helpers ───────────────────────────────────────────

	void OnAddTypeChanged(TSharedPtr<FGeneratorClassEntry> Item, ESelectInfo::Type,
		TSharedPtr<FGeneratorClassEntry>& OutSelected);

	TSharedRef<SWidget> MakeClassEntryRow(TSharedPtr<FGeneratorClassEntry> Item);

	FText GetAddTypeText(const TSharedPtr<FGeneratorClassEntry>& SelectedClass) const;

	// ── Per-stage Add / Remove ────────────────────────────────────────────────

	FReply OnAddClicked(TArray<TSharedPtr<FGeneratorStackItem>>& TargetItems,
		TSharedPtr<FGeneratorClassEntry>& SelectedClass,
		TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>& ListView);

	FReply OnRemoveClicked(TArray<TSharedPtr<FGeneratorStackItem>>& TargetItems,
		TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>& ListView);

	// ── Bottom: shared reference sequence ────────────────────────────────────

	void OnReferenceSequenceSelected(const FAssetData& AssetData);
	FString GetReferenceSequencePath() const;

	// ── Config asset save / load ──────────────────────────────────────────────

	FReply OnSaveConfigAssetClicked();
	void   OnLoadConfigAssetChanged(const FAssetData& AssetData);
	FString GetConfigAssetPath() const;

	// ── Batch processor fill toggle ───────────────────────────────────────────

	ECheckBoxState GetBatchProcessorFillState() const;
	void           OnBatchProcessorFillChanged(ECheckBoxState NewState);
	bool           IsRefSlotEnabled() const;

	// ── Action buttons ────────────────────────────────────────────────────────

	FReply OnGenerateClicked();
	FReply OnClearAllClicked();
	FReply OnExportConfigClicked();
	FReply OnLoadConfigClicked();
	FReply OnCloseClicked();

	bool  CanGenerate() const;
	FText GetGenerateTooltip() const;

	// ── Internal helpers ──────────────────────────────────────────────────────

	void PopulateAvailableClasses();
	UCDGTrajectoryGenerator* CreateGeneratorInstance(UClass* InClass);

	/** Serialize one stage's items to a JSON array. */
	TArray<TSharedPtr<FJsonValue>> SerializeStageItems(
		const TArray<TSharedPtr<FGeneratorStackItem>>& Items) const;

	/** Deserialize one stage's JSON array into generator instances. */
	void DeserializeStageItems(
		const TArray<TSharedPtr<FJsonValue>>& Array,
		TArray<TSharedPtr<FGeneratorStackItem>>& OutItems,
		TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>& ListView);

	/** Release all GC-roots for a stage's items and clear the array. */
	void ClearStageItems(TArray<TSharedPtr<FGeneratorStackItem>>& Items);

	// ── State: Positioning stack ──────────────────────────────────────────────

	TArray<TSharedPtr<FGeneratorStackItem>>                 PositionItems;
	TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>  PositionListView;
	TArray<TSharedPtr<FGeneratorClassEntry>>                PositionClasses;
	TSharedPtr<FGeneratorClassEntry>                        SelectedPositionClass;

	// ── State: Movement stack ─────────────────────────────────────────────────

	TArray<TSharedPtr<FGeneratorStackItem>>                 MovementItems;
	TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>  MovementListView;
	TArray<TSharedPtr<FGeneratorClassEntry>>                MovementClasses;
	TSharedPtr<FGeneratorClassEntry>                        SelectedMovementClass;

	// ── State: Effects stack ──────────────────────────────────────────────────

	TArray<TSharedPtr<FGeneratorStackItem>>                 EffectsItems;
	TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>  EffectsListView;
	TArray<TSharedPtr<FGeneratorClassEntry>>                EffectsClasses;
	TSharedPtr<FGeneratorClassEntry>                        SelectedEffectsClass;

	// ── State: shared ─────────────────────────────────────────────────────────

	/** The generator currently shown in the details view (from any stack). */
	TSharedPtr<FGeneratorStackItem>         SelectedItem;

	TSharedPtr<IDetailsView>                DetailsView;
	TWeakObjectPtr<ULevelSequence>          SharedReferenceSequence;
	TWeakObjectPtr<UGeneratorStackConfig>   LoadedConfigAsset;
	bool                                    bLetBatchProcessorFill = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// CDGGeneratorEditor — utility to open the window
// ─────────────────────────────────────────────────────────────────────────────
class CDGGeneratorEditor
{
public:
	static void OpenWindow();
};
