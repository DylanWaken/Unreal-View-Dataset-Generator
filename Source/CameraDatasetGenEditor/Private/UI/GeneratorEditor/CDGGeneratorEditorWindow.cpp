// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/GeneratorEditor/CDGGeneratorEditorWindow.h"
#include "Config/GeneratorStackConfig.h"
#include "Config/GeneratorStackConfigFactory.h"

#include "Generator/CDGTrajectoryGenerator.h"
#include "Generator/CDGPositioningGenerator.h"
#include "Generator/CDGMovementGenerator.h"
#include "Generator/CDGEffectsGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGenEditor.h"

// Slate
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"

// Property editor
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

// Editor
#include "Editor.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Widgets/Notifications/SNotificationList.h"

// Assets / IO
#include "LevelSequence.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/SavePackage.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

// JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"

#define LOCTEXT_NAMESPACE "CDGGeneratorEditor"

// ─────────────────────────────────────────────────────────────────────────────
// Internal helper
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
	/** Serialize a stage's items to a JSON array value array. */
	TArray<TSharedPtr<FJsonValue>> SerializeItems(
		const TArray<TSharedPtr<FGeneratorStackItem>>& Items)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const TSharedPtr<FGeneratorStackItem>& Item : Items)
		{
			UCDGTrajectoryGenerator* Gen = Item ? Item->Generator : nullptr;
			if (!Gen) continue;

			TSharedRef<FJsonObject> GenObj = MakeShared<FJsonObject>();
			GenObj->SetStringField(TEXT("class"), Gen->GetClass()->GetPathName());

			TSharedPtr<FJsonObject> ConfigObj = MakeShared<FJsonObject>();
			Gen->SerializeGeneratorConfig(ConfigObj);
			GenObj->SetObjectField(TEXT("config"), ConfigObj);

			Array.Add(MakeShared<FJsonValueObject>(GenObj));
		}
		return Array;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Construct
// ─────────────────────────────────────────────────────────────────────────────

void SGeneratorEditorWindow::Construct(const FArguments& InArgs)
{
	PopulateAvailableClasses();

	FPropertyEditorModule& PropEdModule =
		FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.bLockable             = false;
	DetailsArgs.bAllowSearch          = true;
	DetailsArgs.NameAreaSettings      = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bHideSelectionTip     = true;
	DetailsView = PropEdModule.CreateDetailView(DetailsArgs);

	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda(
		[](const FPropertyAndParent& PAP) -> bool
		{
			static const FName ReferenceSequenceName =
				GET_MEMBER_NAME_CHECKED(UCDGTrajectoryGenerator, ReferenceSequence);
			return PAP.Property.GetFName() != ReferenceSequenceName;
		}));

	DetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateLambda(
		[this](const FPropertyAndParent& PAP) -> bool
		{
			if (!bLetBatchProcessorFill) return false;
			static const FName PrimaryCharacterActorName =
				GET_MEMBER_NAME_CHECKED(UCDGPositioningGenerator, PrimaryCharacterActor);
			return PAP.Property.GetFName() == PrimaryCharacterActorName;
		}));

	DetailsView->SetObject(nullptr);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10.f))
		[
			SNew(SVerticalBox)

			// ── Three stage stacks (top) ──────────────────────────────────────
			+ SVerticalBox::Slot()
			.FillHeight(0.45f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				+ SSplitter::Slot()
				.Value(0.333f)
				[
					MakeStagePanel(
						LOCTEXT("PositionStageTitle", "Positioning"),
						PositionClasses, SelectedPositionClass,
						PositionItems,   PositionListView)
				]

				+ SSplitter::Slot()
				.Value(0.333f)
				[
					MakeStagePanel(
						LOCTEXT("MovementStageTitle", "Movement"),
						MovementClasses, SelectedMovementClass,
						MovementItems,   MovementListView)
				]

				+ SSplitter::Slot()
				.Value(0.334f)
				[
					MakeStagePanel(
						LOCTEXT("EffectsStageTitle", "Effects"),
						EffectsClasses, SelectedEffectsClass,
						EffectsItems,   EffectsListView)
				]
			]

			// ── Details view (middle) ─────────────────────────────────────────
			+ SVerticalBox::Slot()
			.FillHeight(0.55f)
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(FMargin(6.f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ConfigTitle", "Generator Configuration"))
						.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						DetailsView.ToSharedRef()
					]
				]
			]

			// ── Config Asset row ──────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ConfigAssetLabel", "Config Asset:"))
					.MinDesiredWidth(130.f)
					.ToolTipText(LOCTEXT("ConfigAssetTip",
						"Select a saved generator pipeline config to load it, "
						"or leave empty to use current values."))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(0.f, 0.f, 5.f, 0.f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UGeneratorStackConfig::StaticClass())
					.ObjectPath(this, &SGeneratorEditorWindow::GetConfigAssetPath)
					.OnObjectChanged(this, &SGeneratorEditorWindow::OnLoadConfigAssetChanged)
					.AllowClear(true)
					.DisplayUseSelected(true)
					.DisplayBrowse(true)
					.ToolTipText(LOCTEXT("ConfigAssetPickerTip",
						"Selecting an asset immediately restores all three generator stacks."))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("SaveConfigAssetBtn", "Save Config"))
					.ToolTipText(LOCTEXT("SaveConfigAssetTip",
						"Save the current generator pipeline to the selected config asset."))
					.OnClicked(this, &SGeneratorEditorWindow::OnSaveConfigAssetClicked)
				]
			]

			// ── Separator ─────────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Separator"))
				.Padding(FMargin(0.f, 1.f))
			]

			// ── Reference Sequence + Batch Fill toggle ────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RefSeqLabel", "Reference Sequence:"))
					.ToolTipText(LOCTEXT("RefSeqTip",
						"Level Sequence whose duration defines the generation timeline. "
						"Propagated to every generator before Generate is run."))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(ULevelSequence::StaticClass())
					.ObjectPath(this, &SGeneratorEditorWindow::GetReferenceSequencePath)
					.OnObjectChanged(this, &SGeneratorEditorWindow::OnReferenceSequenceSelected)
					.AllowClear(true)
					.DisplayUseSelected(true)
					.DisplayBrowse(true)
					.IsEnabled(this, &SGeneratorEditorWindow::IsRefSlotEnabled)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(this, &SGeneratorEditorWindow::GetBatchProcessorFillState)
					.OnCheckStateChanged(this, &SGeneratorEditorWindow::OnBatchProcessorFillChanged)
					.ToolTipText(LOCTEXT("BatchFillTip",
						"When ticked, the reference sequence and actor fields are left empty — "
						"the batch processor will inject them at runtime."))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BatchFillLabel", "Let Batch Processor Fill"))
					]
				]
			]

			// ── Action Buttons ────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("GenerateBtn", "Generate"))
					.ToolTipText(this, &SGeneratorEditorWindow::GetGenerateTooltip)
					.OnClicked(this, &SGeneratorEditorWindow::OnGenerateClicked)
					.IsEnabled(this, &SGeneratorEditorWindow::CanGenerate)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ClearAllBtn", "Clear All"))
					.ToolTipText(LOCTEXT("ClearAllTip",
						"Delete all ACDGTrajectory actors currently present in the level"))
					.OnClicked(this, &SGeneratorEditorWindow::OnClearAllClicked)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ExportConfigBtn", "Export Config"))
					.ToolTipText(LOCTEXT("ExportConfigTip",
						"Save the current generator pipeline to a JSON file"))
					.OnClicked(this, &SGeneratorEditorWindow::OnExportConfigClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("LoadConfigBtn", "Load Config"))
					.ToolTipText(LOCTEXT("LoadConfigTip",
						"Restore a generator pipeline from a previously exported JSON file"))
					.OnClicked(this, &SGeneratorEditorWindow::OnLoadConfigClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("CloseBtn", "Close"))
					.OnClicked(this, &SGeneratorEditorWindow::OnCloseClicked)
				]
			]
		]
	];
}

SGeneratorEditorWindow::~SGeneratorEditorWindow()
{
	ClearStageItems(PositionItems);
	ClearStageItems(MovementItems);
	ClearStageItems(EffectsItems);
}

// ─────────────────────────────────────────────────────────────────────────────
// MakeStagePanel
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SGeneratorEditorWindow::MakeStagePanel(
	const FText& StageTitle,
	TArray<TSharedPtr<FGeneratorClassEntry>>&                   InAvailableClasses,
	TSharedPtr<FGeneratorClassEntry>&                           InSelectedAddClass,
	TArray<TSharedPtr<FGeneratorStackItem>>&                    InItems,
	TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>&     OutListView)
{
	// Capture raw pointers to the member variables so lambdas remain valid
	// after this stack frame returns (the pointed-to objects are members of this).
	TArray<TSharedPtr<FGeneratorClassEntry>>*                      AvailPtr    = &InAvailableClasses;
	TSharedPtr<FGeneratorClassEntry>*                              SelClassPtr = &InSelectedAddClass;
	TArray<TSharedPtr<FGeneratorStackItem>>*                       ItemsPtr    = &InItems;
	TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>*        ListViewPtr = &OutListView;

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(FMargin(6.f))
		[
			SNew(SVerticalBox)

			// Title
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock)
				.Text(StageTitle)
				.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
			]

			// Type combo + Add
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SComboBox<TSharedPtr<FGeneratorClassEntry>>)
					.OptionsSource(AvailPtr)
					.OnSelectionChanged_Lambda(
						[this, SelClassPtr]
						(TSharedPtr<FGeneratorClassEntry> Item, ESelectInfo::Type SelectType)
						{
							OnAddTypeChanged(Item, SelectType, *SelClassPtr);
						})
					.OnGenerateWidget(this, &SGeneratorEditorWindow::MakeClassEntryRow)
					.InitiallySelectedItem(
						InAvailableClasses.Num() > 0 ? InAvailableClasses[0] : nullptr)
					[
						SNew(STextBlock)
						.Text_Lambda([this, SelClassPtr]()
						{
							return GetAddTypeText(*SelClassPtr);
						})
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("AddBtn", "Add"))
					.ToolTipText(LOCTEXT("AddBtnTip",
						"Add the selected generator type to this stage"))
					.OnClicked_Lambda(
						[this, ItemsPtr, SelClassPtr, ListViewPtr]()
						{
							return OnAddClicked(*ItemsPtr, *SelClassPtr, *ListViewPtr);
						})
					.IsEnabled_Lambda([AvailPtr, SelClassPtr]()
					{
						return AvailPtr->Num() > 0 && SelClassPtr->IsValid();
					})
				]
			]

			// List
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SAssignNew(*ListViewPtr, SListView<TSharedPtr<FGeneratorStackItem>>)
				.ListItemsSource(ItemsPtr)
				.OnGenerateRow(this, &SGeneratorEditorWindow::GenerateStackRow)
				.OnSelectionChanged_Lambda(
					[this, ItemsPtr]
					(TSharedPtr<FGeneratorStackItem> Item, ESelectInfo::Type SelectType)
					{
						OnStackSelectionChanged(Item, SelectType, ItemsPtr);
					})
				.SelectionMode(ESelectionMode::Single)
			]

			// Remove
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("RemoveBtn", "Remove"))
					.ToolTipText(LOCTEXT("RemoveTip", "Remove the selected generator"))
					.OnClicked_Lambda(
						[this, ItemsPtr, ListViewPtr]()
						{
							return OnRemoveClicked(*ItemsPtr, *ListViewPtr);
						})
					.IsEnabled_Lambda([this, ItemsPtr]()
					{
						return SelectedItem.IsValid()
							&& ItemsPtr->Contains(SelectedItem);
					})
				]
			]
		];
}

// ─────────────────────────────────────────────────────────────────────────────
// PopulateAvailableClasses
// ─────────────────────────────────────────────────────────────────────────────

void SGeneratorEditorWindow::PopulateAvailableClasses()
{
	PositionClasses.Empty();
	MovementClasses.Empty();
	EffectsClasses.Empty();

	TArray<UClass*> DerivedClasses;
	GetDerivedClasses(UCDGTrajectoryGenerator::StaticClass(), DerivedClasses, true);

	for (UClass* Class : DerivedClasses)
	{
		if (!Class) continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;

		FText DisplayName = FText::FromString(Class->GetName());
		if (UCDGTrajectoryGenerator* CDO = Class->GetDefaultObject<UCDGTrajectoryGenerator>())
		{
			const FName GenName = CDO->GetGeneratorName();
			if (!GenName.IsNone())
			{
				DisplayName = FText::FromName(GenName);
			}

			const EGeneratorStage Stage = CDO->GetGeneratorStage();
			TSharedPtr<FGeneratorClassEntry> Entry =
				MakeShared<FGeneratorClassEntry>(Class, DisplayName);

			switch (Stage)
			{
			case EGeneratorStage::Positioning:
				PositionClasses.Add(Entry);
				break;
			case EGeneratorStage::Movement:
				MovementClasses.Add(Entry);
				break;
			case EGeneratorStage::Effects:
				EffectsClasses.Add(Entry);
				break;
			}
		}
	}

	SelectedPositionClass = PositionClasses.Num() > 0 ? PositionClasses[0] : nullptr;
	SelectedMovementClass = MovementClasses.Num() > 0 ? MovementClasses[0] : nullptr;
	SelectedEffectsClass  = EffectsClasses.Num()  > 0 ? EffectsClasses[0]  : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateGeneratorInstance
// ─────────────────────────────────────────────────────────────────────────────

UCDGTrajectoryGenerator* SGeneratorEditorWindow::CreateGeneratorInstance(UClass* InClass)
{
	if (!InClass) return nullptr;

	UObject* Outer = GetTransientPackage();
	if (GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			Outer = World;
		}
	}

	UCDGTrajectoryGenerator* Gen = NewObject<UCDGTrajectoryGenerator>(Outer, InClass);
	if (Gen)
	{
		Gen->AddToRoot();
	}
	return Gen;
}

// ─────────────────────────────────────────────────────────────────────────────
// GenerateStackRow
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<ITableRow> SGeneratorEditorWindow::GenerateStackRow(
	TSharedPtr<FGeneratorStackItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	FText RowText = LOCTEXT("UnknownGen", "(Unknown)");
	if (Item && Item->Generator)
	{
		const FName GenName = Item->Generator->GetGeneratorName();
		RowText = FText::FromName(GenName.IsNone() ? FName("(unnamed)") : GenName);
	}

	return SNew(STableRow<TSharedPtr<FGeneratorStackItem>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(RowText)
			.Margin(FMargin(6.f, 3.f))
		];
}

// ─────────────────────────────────────────────────────────────────────────────
// OnStackSelectionChanged
// ─────────────────────────────────────────────────────────────────────────────

void SGeneratorEditorWindow::OnStackSelectionChanged(
	TSharedPtr<FGeneratorStackItem> Item,
	ESelectInfo::Type /*SelectType*/,
	TArray<TSharedPtr<FGeneratorStackItem>>* OwnerArray)
{
	if (!Item.IsValid()) return;

	SelectedItem = Item;

	// Clear selection in the other two lists
	auto ClearOther = [&](
		TArray<TSharedPtr<FGeneratorStackItem>>* OtherArray,
		TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>> OtherListView)
	{
		if (OtherArray != OwnerArray && OtherListView.IsValid())
		{
			OtherListView->ClearSelection();
		}
	};

	ClearOther(&PositionItems, PositionListView);
	ClearOther(&MovementItems, MovementListView);
	ClearOther(&EffectsItems,  EffectsListView);

	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(Item->Generator);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Combo-box helpers
// ─────────────────────────────────────────────────────────────────────────────

void SGeneratorEditorWindow::OnAddTypeChanged(
	TSharedPtr<FGeneratorClassEntry> Item,
	ESelectInfo::Type /*SelectType*/,
	TSharedPtr<FGeneratorClassEntry>& OutSelected)
{
	OutSelected = Item;
}

TSharedRef<SWidget> SGeneratorEditorWindow::MakeClassEntryRow(
	TSharedPtr<FGeneratorClassEntry> Item)
{
	return SNew(STextBlock)
		.Text(Item.IsValid() ? Item->DisplayName : LOCTEXT("NoneClass", "(None)"))
		.Margin(FMargin(4.f, 2.f));
}

FText SGeneratorEditorWindow::GetAddTypeText(
	const TSharedPtr<FGeneratorClassEntry>& SelectedClass) const
{
	return SelectedClass.IsValid()
		? SelectedClass->DisplayName
		: LOCTEXT("SelectType", "Select type...");
}

// ─────────────────────────────────────────────────────────────────────────────
// Add / Remove
// ─────────────────────────────────────────────────────────────────────────────

FReply SGeneratorEditorWindow::OnAddClicked(
	TArray<TSharedPtr<FGeneratorStackItem>>& TargetItems,
	TSharedPtr<FGeneratorClassEntry>& SelectedClass,
	TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>& ListView)
{
	if (!SelectedClass.IsValid() || !SelectedClass->Class) return FReply::Handled();

	UCDGTrajectoryGenerator* NewGen = CreateGeneratorInstance(SelectedClass->Class);
	if (!NewGen) return FReply::Handled();

	if (SharedReferenceSequence.IsValid())
	{
		NewGen->ReferenceSequence = SharedReferenceSequence.Get();
	}

	TSharedPtr<FGeneratorStackItem> NewItem = MakeShared<FGeneratorStackItem>(NewGen);
	TargetItems.Add(NewItem);
	ListView->RequestListRefresh();
	ListView->SetSelection(NewItem);

	return FReply::Handled();
}

FReply SGeneratorEditorWindow::OnRemoveClicked(
	TArray<TSharedPtr<FGeneratorStackItem>>& TargetItems,
	TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>& ListView)
{
	if (!SelectedItem.IsValid()) return FReply::Handled();
	if (!TargetItems.Contains(SelectedItem)) return FReply::Handled();

	if (SelectedItem->Generator && SelectedItem->Generator->IsRooted())
	{
		SelectedItem->Generator->RemoveFromRoot();
	}

	TargetItems.Remove(SelectedItem);
	SelectedItem.Reset();

	if (DetailsView.IsValid()) DetailsView->SetObject(nullptr);
	ListView->RequestListRefresh();

	return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
// Reference Sequence
// ─────────────────────────────────────────────────────────────────────────────

void SGeneratorEditorWindow::OnReferenceSequenceSelected(const FAssetData& AssetData)
{
	SharedReferenceSequence = Cast<ULevelSequence>(AssetData.GetAsset());

	auto PropagateToStage = [this](TArray<TSharedPtr<FGeneratorStackItem>>& Items)
	{
		for (const TSharedPtr<FGeneratorStackItem>& Item : Items)
		{
			if (Item && Item->Generator)
			{
				Item->Generator->ReferenceSequence = SharedReferenceSequence.Get();
			}
		}
	};

	PropagateToStage(PositionItems);
	PropagateToStage(MovementItems);
	PropagateToStage(EffectsItems);
}

FString SGeneratorEditorWindow::GetReferenceSequencePath() const
{
	return SharedReferenceSequence.IsValid()
		? SharedReferenceSequence->GetPathName()
		: FString();
}

// ─────────────────────────────────────────────────────────────────────────────
// Generate
// ─────────────────────────────────────────────────────────────────────────────

FReply SGeneratorEditorWindow::OnGenerateClicked()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogCameraDatasetGenEditor, Error,
			TEXT("[GeneratorEditor] No editor world available"));
		return FReply::Handled();
	}

	// Helper: ensure generator is outered to the current world
	auto PrepareGen = [World](UCDGTrajectoryGenerator* Gen,
		ULevelSequence* RefSeq) -> UCDGTrajectoryGenerator*
	{
		if (!Gen) return nullptr;
		if (RefSeq) Gen->ReferenceSequence = RefSeq;
		if (Gen->GetOuter() != World)
		{
			Gen->Rename(nullptr, World,
				REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		}
		return Gen;
	};

	ULevelSequence* RefSeq = SharedReferenceSequence.Get();

	// ── Cartesian product: every (positioning × movement) pair runs independently.
	// Each positioning generator produces its own placement set; each movement
	// generator consumes that set separately, so total trajectories =
	//   sum_i(placements_i)  ×  |MovementStack|
	// All resulting trajectories are then forwarded to every effects generator
	// in sequence (effects chain, not multiply).
	TArray<ACDGTrajectory*> AllTrajectories;

	for (const TSharedPtr<FGeneratorStackItem>& PosItem : PositionItems)
	{
		UCDGPositioningGenerator* PosGen =
			Cast<UCDGPositioningGenerator>(PrepareGen(PosItem ? PosItem->Generator : nullptr, RefSeq));
		if (!PosGen) continue;

		const TArray<FCDGCameraPlacement> Placements = PosGen->GeneratePlacements();

		UE_LOG(LogCameraDatasetGenEditor, Log,
			TEXT("[GeneratorEditor] %s produced %d placement(s)"),
			*PosGen->GetGeneratorName().ToString(), Placements.Num());

		if (Placements.IsEmpty()) continue;

		for (const TSharedPtr<FGeneratorStackItem>& MovItem : MovementItems)
		{
			UCDGMovementGenerator* MovGen =
				Cast<UCDGMovementGenerator>(PrepareGen(MovItem ? MovItem->Generator : nullptr, RefSeq));
			if (!MovGen) continue;

			// Propagate subject context from the paired positioning generator
			MovGen->PrimaryCharacterActor = PosGen->PrimaryCharacterActor;
			MovGen->FocusedAnchor         = PosGen->FocusedAnchor;

			TArray<ACDGTrajectory*> Created = MovGen->GenerateMovement(Placements);
			AllTrajectories.Append(Created);

			UE_LOG(LogCameraDatasetGenEditor, Log,
				TEXT("[GeneratorEditor] %s × %s → %d trajectory/ies"),
				*PosGen->GetGeneratorName().ToString(),
				*MovGen->GetGeneratorName().ToString(),
				Created.Num());
		}
	}

	// Collect subject context from the first valid positioning generator so that
	// Effects generators (which have their Subject category hidden in the UI) get
	// their PrimaryCharacterActor / FocusedAnchor populated before ApplyEffects.
	UCDGPositioningGenerator* FirstPosGen = nullptr;
	for (const TSharedPtr<FGeneratorStackItem>& PosItem : PositionItems)
	{
		if (PosItem && PosItem->Generator)
		{
			FirstPosGen = Cast<UCDGPositioningGenerator>(PosItem->Generator);
			if (FirstPosGen) break;
		}
	}

	// ── Effects: each generator in the stack is applied in sequence to the
	// full combined trajectory set (effects chain, not multiply).
	for (const TSharedPtr<FGeneratorStackItem>& FxItem : EffectsItems)
	{
		UCDGEffectsGenerator* FxGen =
			Cast<UCDGEffectsGenerator>(PrepareGen(FxItem ? FxItem->Generator : nullptr, RefSeq));
		if (!FxGen) continue;

		// Propagate subject context from the first positioning generator
		if (FirstPosGen)
		{
			FxGen->PrimaryCharacterActor = FirstPosGen->PrimaryCharacterActor;
			FxGen->FocusedAnchor         = FirstPosGen->FocusedAnchor;
		}

		FxGen->ApplyEffects(AllTrajectories);

		UE_LOG(LogCameraDatasetGenEditor, Log,
			TEXT("[GeneratorEditor] %s applied effects to %d trajectory/ies"),
			*FxGen->GetGeneratorName().ToString(), AllTrajectories.Num());
	}

	FNotificationInfo Info(FText::Format(
		LOCTEXT("GenerateDoneNotif", "Generation complete — {0} trajectories created"),
		FText::AsNumber(AllTrajectories.Num())));
	Info.ExpireDuration      = 4.f;
	Info.bUseLargeFont       = false;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

bool SGeneratorEditorWindow::CanGenerate() const
{
	if (bLetBatchProcessorFill) return false;
	return PositionItems.Num() > 0 && MovementItems.Num() > 0;
}

FText SGeneratorEditorWindow::GetGenerateTooltip() const
{
	if (bLetBatchProcessorFill)
	{
		return LOCTEXT("GenerateTipBatchMode",
			"Generate is disabled — 'Let Batch Processor Fill' is active.");
	}
	if (PositionItems.IsEmpty())
	{
		return LOCTEXT("GenerateTipNoPos",
			"Add at least one Positioning generator before running.");
	}
	if (MovementItems.IsEmpty())
	{
		return LOCTEXT("GenerateTipNoMov",
			"Add at least one Movement generator before running.");
	}
	return LOCTEXT("GenerateTip",
		"Run the full pipeline.\n"
		"Each Positioning generator pairs with every Movement generator (cartesian product).\n"
		"Total trajectories = N_positions × M_movement_generators.\n"
		"Effects are then applied in sequence to all trajectories.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Clear All
// ─────────────────────────────────────────────────────────────────────────────

FReply SGeneratorEditorWindow::OnClearAllClicked()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FReply::Handled();

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem) return FReply::Handled();

	const TArray<FName> Names = Subsystem->GetTrajectoryNames();
	for (const FName& Name : Names)
	{
		Subsystem->DeleteTrajectory(Name);
	}

	TArray<ACDGKeyframe*> Keyframes;
	for (TActorIterator<ACDGKeyframe> It(World); It; ++It) Keyframes.Add(*It);
	for (ACDGKeyframe* KF : Keyframes)
	{
		World->EditorDestroyActor(KF, true);
	}

	FNotificationInfo Info(FText::Format(
		LOCTEXT("ClearAllNotif", "Cleared {0} trajectories and {1} keyframes from the level"),
		FText::AsNumber(Names.Num()),
		FText::AsNumber(Keyframes.Num())));
	Info.ExpireDuration       = 3.f;
	Info.bUseLargeFont        = false;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialization helpers
// ─────────────────────────────────────────────────────────────────────────────

TArray<TSharedPtr<FJsonValue>> SGeneratorEditorWindow::SerializeStageItems(
	const TArray<TSharedPtr<FGeneratorStackItem>>& Items) const
{
	return SerializeItems(Items);
}

void SGeneratorEditorWindow::DeserializeStageItems(
	const TArray<TSharedPtr<FJsonValue>>& Array,
	TArray<TSharedPtr<FGeneratorStackItem>>& OutItems,
	TSharedPtr<SListView<TSharedPtr<FGeneratorStackItem>>>& ListView)
{
	for (const TSharedPtr<FJsonValue>& Val : Array)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;

		FString ClassName;
		if (!(*ObjPtr)->TryGetStringField(TEXT("class"), ClassName)) continue;

		UClass* Class = FindObject<UClass>(nullptr, *ClassName);
		if (!Class)
		{
			UE_LOG(LogCameraDatasetGenEditor, Warning,
				TEXT("[GeneratorEditor] Load: class not found: %s"), *ClassName);
			continue;
		}

		UCDGTrajectoryGenerator* Gen = CreateGeneratorInstance(Class);
		if (!Gen) continue;

		const TSharedPtr<FJsonObject>* CfgPtr = nullptr;
		if ((*ObjPtr)->TryGetObjectField(TEXT("config"), CfgPtr) && CfgPtr)
		{
			Gen->FetchGeneratorConfig(*CfgPtr);
		}

		if (SharedReferenceSequence.IsValid())
		{
			Gen->ReferenceSequence = SharedReferenceSequence.Get();
		}

		OutItems.Add(MakeShared<FGeneratorStackItem>(Gen));
	}

	if (ListView.IsValid()) ListView->RequestListRefresh();
}

void SGeneratorEditorWindow::ClearStageItems(TArray<TSharedPtr<FGeneratorStackItem>>& Items)
{
	for (const TSharedPtr<FGeneratorStackItem>& Item : Items)
	{
		if (Item && Item->Generator && Item->Generator->IsRooted())
		{
			Item->Generator->RemoveFromRoot();
		}
	}
	Items.Empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// Export Config
// ─────────────────────────────────────────────────────────────────────────────

FReply SGeneratorEditorWindow::OnExportConfigClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return FReply::Handled();

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	void* ParentHandle =
		(ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	TArray<FString> OutFiles;
	if (!DesktopPlatform->SaveFileDialog(
		ParentHandle,
		TEXT("Export Generator Config"),
		FPaths::ProjectSavedDir(),
		TEXT("GeneratorConfig.json"),
		TEXT("JSON Files (*.json)|*.json|All Files (*.*)|*.*"),
		EFileDialogFlags::None, OutFiles)
		|| OutFiles.IsEmpty())
	{
		return FReply::Handled();
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("positioning"), SerializeStageItems(PositionItems));
	Root->SetArrayField(TEXT("movement"),    SerializeStageItems(MovementItems));
	Root->SetArrayField(TEXT("effects"),     SerializeStageItems(EffectsItems));

	FString JsonString;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&JsonString));
	const bool bSuccess = FFileHelper::SaveStringToFile(JsonString, *OutFiles[0]);

	FNotificationInfo Info(bSuccess
		? FText::Format(LOCTEXT("ExportConfigSuccess", "Config exported to:\n{0}"),
			FText::FromString(OutFiles[0]))
		: LOCTEXT("ExportConfigFailed", "Failed to export generator config"));
	Info.ExpireDuration = 5.f;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
// Load Config
// ─────────────────────────────────────────────────────────────────────────────

FReply SGeneratorEditorWindow::OnLoadConfigClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return FReply::Handled();

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	void* ParentHandle =
		(ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	TArray<FString> OutFiles;
	if (!DesktopPlatform->OpenFileDialog(
		ParentHandle,
		TEXT("Load Generator Config"),
		FPaths::ProjectSavedDir(),
		TEXT(""),
		TEXT("JSON Files (*.json)|*.json|All Files (*.*)|*.*"),
		EFileDialogFlags::None, OutFiles)
		|| OutFiles.IsEmpty())
	{
		return FReply::Handled();
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *OutFiles[0]))
	{
		FNotificationInfo Info(LOCTEXT("LoadConfigReadFailed", "Failed to read the config file"));
		Info.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FReply::Handled();
	}

	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonString), Root) || !Root.IsValid())
	{
		FNotificationInfo Info(LOCTEXT("LoadConfigParseFailed", "Failed to parse the config JSON"));
		Info.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FReply::Handled();
	}

	// Clear all stages
	ClearStageItems(PositionItems);
	ClearStageItems(MovementItems);
	ClearStageItems(EffectsItems);
	SelectedItem.Reset();
	if (DetailsView.IsValid()) DetailsView->SetObject(nullptr);

	// Restore each stage
	const TArray<TSharedPtr<FJsonValue>>* PosArray = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* MovArray = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* FxArray  = nullptr;

	if (Root->TryGetArrayField(TEXT("positioning"), PosArray) && PosArray)
		DeserializeStageItems(*PosArray, PositionItems, PositionListView);
	if (Root->TryGetArrayField(TEXT("movement"), MovArray) && MovArray)
		DeserializeStageItems(*MovArray, MovementItems, MovementListView);
	if (Root->TryGetArrayField(TEXT("effects"), FxArray) && FxArray)
		DeserializeStageItems(*FxArray, EffectsItems, EffectsListView);

	const int32 Total = PositionItems.Num() + MovementItems.Num() + EffectsItems.Num();
	FNotificationInfo Info(FText::Format(
		LOCTEXT("LoadConfigSuccess", "Loaded {0} generators from config"),
		FText::AsNumber(Total)));
	Info.ExpireDuration       = 4.f;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
// Config asset save / load
// ─────────────────────────────────────────────────────────────────────────────

FString SGeneratorEditorWindow::GetConfigAssetPath() const
{
	return LoadedConfigAsset.IsValid() ? LoadedConfigAsset->GetPathName() : FString();
}

void SGeneratorEditorWindow::OnLoadConfigAssetChanged(const FAssetData& AssetData)
{
	UGeneratorStackConfig* Config = Cast<UGeneratorStackConfig>(AssetData.GetAsset());
	LoadedConfigAsset = Config;
	if (!Config || Config->GeneratorsJson.IsEmpty()) return;

	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(
		TJsonReaderFactory<>::Create(Config->GeneratorsJson), Root) || !Root.IsValid())
	{
		return;
	}

	ClearStageItems(PositionItems);
	ClearStageItems(MovementItems);
	ClearStageItems(EffectsItems);
	SelectedItem.Reset();
	if (DetailsView.IsValid()) DetailsView->SetObject(nullptr);

	const TArray<TSharedPtr<FJsonValue>>* PosArray = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* MovArray = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* FxArray  = nullptr;

	if (Root->TryGetArrayField(TEXT("positioning"), PosArray) && PosArray)
		DeserializeStageItems(*PosArray, PositionItems, PositionListView);
	if (Root->TryGetArrayField(TEXT("movement"), MovArray) && MovArray)
		DeserializeStageItems(*MovArray, MovementItems, MovementListView);
	if (Root->TryGetArrayField(TEXT("effects"), FxArray) && FxArray)
		DeserializeStageItems(*FxArray, EffectsItems, EffectsListView);

	bLetBatchProcessorFill = Config->bLetBatchProcessorFill;
	if (DetailsView.IsValid()) DetailsView->ForceRefresh();

	const int32 Total = PositionItems.Num() + MovementItems.Num() + EffectsItems.Num();
	FNotificationInfo Info(FText::Format(
		LOCTEXT("LoadConfigAssetSuccess", "Loaded {0} generators from config asset"),
		FText::AsNumber(Total)));
	Info.ExpireDuration       = 3.f;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);
}

FReply SGeneratorEditorWindow::OnSaveConfigAssetClicked()
{
	IAssetTools& AssetTools =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Build the JSON string for all three stages
	auto BuildJson = [this]() -> FString
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetArrayField(TEXT("positioning"), SerializeStageItems(PositionItems));
		Root->SetArrayField(TEXT("movement"),    SerializeStageItems(MovementItems));
		Root->SetArrayField(TEXT("effects"),     SerializeStageItems(EffectsItems));

		FString Out;
		FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Out));
		return Out;
	};

	auto WriteToConfig = [this, &BuildJson](UGeneratorStackConfig* Config)
	{
		Config->Modify();
		Config->GeneratorsJson        = BuildJson();
		Config->bLetBatchProcessorFill = bLetBatchProcessorFill;
		Config->MarkPackageDirty();

		UPackage* Package = Config->GetOutermost();
		FString PackageFilename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, Config, *PackageFilename, SaveArgs);
	};

	if (LoadedConfigAsset.IsValid())
	{
		WriteToConfig(LoadedConfigAsset.Get());
		FNotificationInfo Info(LOCTEXT("SaveConfigAssetDone", "Generator pipeline config saved"));
		Info.ExpireDuration = 3.f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		UGeneratorStackConfigFactory* Factory = NewObject<UGeneratorStackConfigFactory>();
		UObject* NewObj = AssetTools.CreateAssetWithDialog(
			TEXT("GeneratorStackConfig"), TEXT("/Game"),
			UGeneratorStackConfig::StaticClass(), Factory);

		if (UGeneratorStackConfig* NewConfig = Cast<UGeneratorStackConfig>(NewObj))
		{
			WriteToConfig(NewConfig);
			LoadedConfigAsset = NewConfig;
			FNotificationInfo Info(LOCTEXT("CreateConfigAssetDone",
				"Generator pipeline config created and saved"));
			Info.ExpireDuration = 3.f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}

	return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch processor fill toggle
// ─────────────────────────────────────────────────────────────────────────────

ECheckBoxState SGeneratorEditorWindow::GetBatchProcessorFillState() const
{
	return bLetBatchProcessorFill ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SGeneratorEditorWindow::OnBatchProcessorFillChanged(ECheckBoxState NewState)
{
	bLetBatchProcessorFill = (NewState == ECheckBoxState::Checked);
	if (DetailsView.IsValid()) DetailsView->ForceRefresh();
}

bool SGeneratorEditorWindow::IsRefSlotEnabled() const
{
	return !bLetBatchProcessorFill;
}

// ─────────────────────────────────────────────────────────────────────────────
// Close
// ─────────────────────────────────────────────────────────────────────────────

FReply SGeneratorEditorWindow::OnCloseClicked()
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (Window.IsValid()) Window->RequestDestroyWindow();
	return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
// CDGGeneratorEditor::OpenWindow
// ─────────────────────────────────────────────────────────────────────────────

void CDGGeneratorEditor::OpenWindow()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Trajectory Generator Editor"))
		.ClientSize(FVector2D(1100.f, 720.f))
		.MinWidth(800.f)
		.MinHeight(550.f);

	Window->SetContent(SNew(SGeneratorEditorWindow));

	IMainFrameModule& MainFrame =
		FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

	if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window, ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window);
	}
}

#undef LOCTEXT_NAMESPACE
