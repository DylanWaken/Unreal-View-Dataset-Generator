// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/SListView.h"

class UCDGTrajectoryGenerator;
class ULevelSequence;
class IDetailsView;
struct FAssetData;

// ─────────────────────────────────────────────────────────────────────────────
// FGeneratorClassEntry — one entry in the "type to add" combo box
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
// FGeneratorStackItem — one entry in the generator stack list
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
// Slate window for configuring and running trajectory generators.
//
// Layout:
//   ┌─────────────────────┬────────────────────────────────────────┐
//   │  Generator Stack    │  Generator Configuration (IDetailsView) │
//   │  [Type Combo] [Add] │                                        │
//   │  1. StaticGen       │  All UPROPERTY fields for the selected  │
//   │  2. StaticGen       │  generator instance are shown here.    │
//   │  [↑][↓]  [Remove]  │                                        │
//   ├─────────────────────┴────────────────────────────────────────┤
//   │  Reference Sequence:  [SObjectPropertyEntryBox]              │
//   │  [Generate]  [Clear All]      [Export Config]  [Load Config] │
//   └──────────────────────────────────────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────
class SGeneratorEditorWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGeneratorEditorWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGeneratorEditorWindow();

private:
	// ── Left panel: generator stack ─────────────────────────────────────────

	TSharedRef<ITableRow> GenerateStackRow(
		TSharedPtr<FGeneratorStackItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	void OnStackSelectionChanged(
		TSharedPtr<FGeneratorStackItem> Item,
		ESelectInfo::Type SelectType);

	// Type-selector combo box
	void OnAddTypeChanged(
		TSharedPtr<FGeneratorClassEntry> Item,
		ESelectInfo::Type SelectType);

	TSharedRef<SWidget> MakeClassEntryRow(TSharedPtr<FGeneratorClassEntry> Item);
	FText GetAddTypeText() const;

	// Stack toolbar
	FReply OnAddClicked();
	FReply OnRemoveClicked();
	FReply OnMoveUpClicked();
	FReply OnMoveDownClicked();

	bool CanRemove() const;
	bool CanMoveUp() const;
	bool CanMoveDown() const;

	// ── Bottom: shared reference sequence ───────────────────────────────────

	void OnReferenceSequenceSelected(const FAssetData& AssetData);
	FString GetReferenceSequencePath() const;

	// ── Action buttons ───────────────────────────────────────────────────────

	FReply OnGenerateClicked();
	FReply OnClearAllClicked();
	FReply OnExportConfigClicked();
	FReply OnLoadConfigClicked();
	FReply OnCloseClicked();

	bool  CanGenerate() const;
	FText GetGenerateTooltip() const;

	// ── Helpers ──────────────────────────────────────────────────────────────

	void PopulateAvailableClasses();
	UCDGTrajectoryGenerator* CreateGeneratorInstance(UClass* InClass);

	// ── State ────────────────────────────────────────────────────────────────

	TArray<TSharedPtr<FGeneratorStackItem>>                  GeneratorItems;
	TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>   GeneratorListView;
	TSharedPtr<FGeneratorStackItem>                          SelectedItem;

	TSharedPtr<IDetailsView>                                 DetailsView;

	TArray<TSharedPtr<FGeneratorClassEntry>>                 AvailableClasses;
	TSharedPtr<FGeneratorClassEntry>                         SelectedAddClass;

	TWeakObjectPtr<ULevelSequence>                           SharedReferenceSequence;
};

// ─────────────────────────────────────────────────────────────────────────────
// CDGGeneratorEditor — utility to open the window
// ─────────────────────────────────────────────────────────────────────────────
class CDGGeneratorEditor
{
public:
	static void OpenWindow();
};
