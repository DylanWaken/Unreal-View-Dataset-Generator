// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/GeneratorEditor/CDGGeneratorEditorWindow.h"

#include "Generator/CDGTrajectoryGenerator.h"
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

// Property editor (details panel)
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
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
// SGeneratorEditorWindow — Construct
// ─────────────────────────────────────────────────────────────────────────────

void SGeneratorEditorWindow::Construct(const FArguments& InArgs)
{
	PopulateAvailableClasses();

	// Build a standalone IDetailsView for the right panel
	FPropertyEditorModule& PropEdModule =
		FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.bLockable             = false;
	DetailsArgs.bAllowSearch          = true;
	DetailsArgs.NameAreaSettings      = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bHideSelectionTip     = true;
	DetailsView = PropEdModule.CreateDetailView(DetailsArgs);

	// Hide base-class properties that are managed by the shared bottom panel,
	// so the details view only shows the generator-specific configuration.
	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda(
		[](const FPropertyAndParent& PropertyAndParent) -> bool
		{
			static const FName ReferenceSequenceName =
				GET_MEMBER_NAME_CHECKED(UCDGTrajectoryGenerator, ReferenceSequence);
			return PropertyAndParent.Property.GetFName() != ReferenceSequenceName;
		}));

	DetailsView->SetObject(nullptr);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10.f))
		[
			SNew(SVerticalBox)

			// ── Main content splitter ─────────────────────────────────────────
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				// ── LEFT: Generator Stack ─────────────────────────────────────
				+ SSplitter::Slot()
				.Value(0.35f)
				[
					SNew(SBorder)
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
							.Text(LOCTEXT("StackTitle", "Generator Stack"))
							.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
						]

						// Type picker + Add button
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 0.f, 0.f, 4.f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							[
								SNew(SComboBox<TSharedPtr<FGeneratorClassEntry>>)
								.OptionsSource(&AvailableClasses)
								.OnSelectionChanged(this, &SGeneratorEditorWindow::OnAddTypeChanged)
								.OnGenerateWidget(this, &SGeneratorEditorWindow::MakeClassEntryRow)
								.InitiallySelectedItem(AvailableClasses.Num() > 0 ? AvailableClasses[0] : nullptr)
								[
									SNew(STextBlock)
									.Text(this, &SGeneratorEditorWindow::GetAddTypeText)
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(4.f, 0.f, 0.f, 0.f)
							[
								SNew(SButton)
								.Text(LOCTEXT("AddBtn", "Add"))
								.ToolTipText(LOCTEXT("AddBtnTip", "Add the selected generator type to the stack"))
								.OnClicked(this, &SGeneratorEditorWindow::OnAddClicked)
								.IsEnabled_Lambda([this]()
								{
									return AvailableClasses.Num() > 0 && SelectedAddClass.IsValid();
								})
							]
						]

						// Generator list
						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SAssignNew(GeneratorListView, SListView<TSharedPtr<FGeneratorStackItem>>)
							.ListItemsSource(&GeneratorItems)
							.OnGenerateRow(this, &SGeneratorEditorWindow::GenerateStackRow)
							.OnSelectionChanged(this, &SGeneratorEditorWindow::OnStackSelectionChanged)
							.SelectionMode(ESelectionMode::Single)
						]

						// Re-order / Remove controls
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 4.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.f, 0.f, 4.f, 0.f)
							[
								SNew(SButton)
								.Text(LOCTEXT("MoveUpBtn", "↑"))
								.ToolTipText(LOCTEXT("MoveUpTip", "Move selected generator up"))
								.OnClicked(this, &SGeneratorEditorWindow::OnMoveUpClicked)
								.IsEnabled(this, &SGeneratorEditorWindow::CanMoveUp)
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.f, 0.f, 4.f, 0.f)
							[
								SNew(SButton)
								.Text(LOCTEXT("MoveDownBtn", "↓"))
								.ToolTipText(LOCTEXT("MoveDownTip", "Move selected generator down"))
								.OnClicked(this, &SGeneratorEditorWindow::OnMoveDownClicked)
								.IsEnabled(this, &SGeneratorEditorWindow::CanMoveDown)
							]

							+ SHorizontalBox::Slot()
							.FillWidth(1.f)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("RemoveBtn", "Remove"))
								.ToolTipText(LOCTEXT("RemoveTip", "Remove the selected generator from the stack"))
								.OnClicked(this, &SGeneratorEditorWindow::OnRemoveClicked)
								.IsEnabled(this, &SGeneratorEditorWindow::CanRemove)
							]
						]
					]
				]

				// ── RIGHT: Generator Configuration (IDetailsView) ─────────────
				+ SSplitter::Slot()
				.Value(0.65f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(FMargin(6.f))
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 0.f, 0.f, 6.f)
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
			]

			// ── Shared Reference Sequence ─────────────────────────────────────
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
					.Text(LOCTEXT("RefSeqLabel", "Reference Sequence:"))
					.ToolTipText(LOCTEXT("RefSeqTip",
						"Level Sequence whose duration defines the generation timeline.\n"
						"Propagated to every generator in the stack before Generate is run."))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(ULevelSequence::StaticClass())
					.ObjectPath(this, &SGeneratorEditorWindow::GetReferenceSequencePath)
					.OnObjectChanged(this, &SGeneratorEditorWindow::OnReferenceSequenceSelected)
					.AllowClear(true)
					.DisplayUseSelected(true)
					.DisplayBrowse(true)
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

				// Spacer
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ExportConfigBtn", "Export Config"))
					.ToolTipText(LOCTEXT("ExportConfigTip",
						"Save the current generator stack and all configurations to a JSON file"))
					.OnClicked(this, &SGeneratorEditorWindow::OnExportConfigClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("LoadConfigBtn", "Load Config"))
					.ToolTipText(LOCTEXT("LoadConfigTip",
						"Restore a generator stack from a previously exported JSON config file"))
					.OnClicked(this, &SGeneratorEditorWindow::OnLoadConfigClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("CloseBtn", "Close"))
					.ToolTipText(LOCTEXT("CloseBtnTip", "Close this window"))
					.OnClicked(this, &SGeneratorEditorWindow::OnCloseClicked)
				]
			]
		]
	];
}

SGeneratorEditorWindow::~SGeneratorEditorWindow()
{
	// Allow all transient generator instances to be garbage collected
	for (const TSharedPtr<FGeneratorStackItem>& Item : GeneratorItems)
	{
		if (Item && Item->Generator && Item->Generator->IsRooted())
		{
			Item->Generator->RemoveFromRoot();
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// PopulateAvailableClasses
// ─────────────────────────────────────────────────────────────────────────────

void SGeneratorEditorWindow::PopulateAvailableClasses()
{
	AvailableClasses.Empty();

	TArray<UClass*> DerivedClasses;
	GetDerivedClasses(UCDGTrajectoryGenerator::StaticClass(), DerivedClasses, /*bRecursive=*/true);

	for (UClass* Class : DerivedClasses)
	{
		if (!Class) continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;

		// Use the generator's self-reported name as display name when available
		FText DisplayName = FText::FromString(Class->GetName());
		if (UCDGTrajectoryGenerator* CDO = Class->GetDefaultObject<UCDGTrajectoryGenerator>())
		{
			const FName GenName = CDO->GetGeneratorName();
			if (!GenName.IsNone())
			{
				DisplayName = FText::FromName(GenName);
			}
		}

		AvailableClasses.Add(MakeShared<FGeneratorClassEntry>(Class, DisplayName));
	}

	SelectedAddClass = AvailableClasses.Num() > 0 ? AvailableClasses[0] : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateGeneratorInstance
// ─────────────────────────────────────────────────────────────────────────────

UCDGTrajectoryGenerator* SGeneratorEditorWindow::CreateGeneratorInstance(UClass* InClass)
{
	if (!InClass) return nullptr;

	// Use the current editor world as outer so GetWorld() resolves correctly inside Generate()
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
		Gen->AddToRoot(); // Prevent GC while the editor window is open
	}
	return Gen;
}

// ─────────────────────────────────────────────────────────────────────────────
// Left panel — list row generation
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<ITableRow> SGeneratorEditorWindow::GenerateStackRow(
	TSharedPtr<FGeneratorStackItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	FText RowText = LOCTEXT("UnknownGen", "(Unknown)");
	if (Item && Item->Generator)
	{
		const int32 Idx = GeneratorItems.IndexOfByKey(Item);
		const FName GenName = Item->Generator->GetGeneratorName();
		RowText = FText::FromString(
			FString::Printf(TEXT("%d.  %s"), Idx + 1, *GenName.ToString()));
	}

	return SNew(STableRow<TSharedPtr<FGeneratorStackItem>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(RowText)
			.Margin(FMargin(6.f, 3.f))
		];
}

void SGeneratorEditorWindow::OnStackSelectionChanged(
	TSharedPtr<FGeneratorStackItem> Item,
	ESelectInfo::Type /*SelectType*/)
{
	SelectedItem = Item;
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(Item.IsValid() ? Item->Generator : nullptr);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Type combo box
// ─────────────────────────────────────────────────────────────────────────────

void SGeneratorEditorWindow::OnAddTypeChanged(
	TSharedPtr<FGeneratorClassEntry> Item,
	ESelectInfo::Type /*SelectType*/)
{
	SelectedAddClass = Item;
}

TSharedRef<SWidget> SGeneratorEditorWindow::MakeClassEntryRow(TSharedPtr<FGeneratorClassEntry> Item)
{
	return SNew(STextBlock)
		.Text(Item.IsValid() ? Item->DisplayName : LOCTEXT("NoneClass", "(None)"))
		.Margin(FMargin(4.f, 2.f));
}

FText SGeneratorEditorWindow::GetAddTypeText() const
{
	return SelectedAddClass.IsValid()
		? SelectedAddClass->DisplayName
		: LOCTEXT("SelectType", "Select type...");
}

// ─────────────────────────────────────────────────────────────────────────────
// Stack toolbar — Add / Remove / Move
// ─────────────────────────────────────────────────────────────────────────────

FReply SGeneratorEditorWindow::OnAddClicked()
{
	if (!SelectedAddClass.IsValid() || !SelectedAddClass->Class)
	{
		return FReply::Handled();
	}

	UCDGTrajectoryGenerator* NewGen = CreateGeneratorInstance(SelectedAddClass->Class);
	if (!NewGen) return FReply::Handled();

	// Propagate the shared reference sequence to the newly created generator
	if (SharedReferenceSequence.IsValid())
	{
		NewGen->ReferenceSequence = SharedReferenceSequence.Get();
	}

	TSharedPtr<FGeneratorStackItem> NewItem = MakeShared<FGeneratorStackItem>(NewGen);
	GeneratorItems.Add(NewItem);
	GeneratorListView->RequestListRefresh();

	// Auto-select the newly added generator
	GeneratorListView->SetSelection(NewItem);

	return FReply::Handled();
}

FReply SGeneratorEditorWindow::OnRemoveClicked()
{
	if (!SelectedItem.IsValid()) return FReply::Handled();

	if (SelectedItem->Generator && SelectedItem->Generator->IsRooted())
	{
		SelectedItem->Generator->RemoveFromRoot();
	}

	GeneratorItems.Remove(SelectedItem);
	SelectedItem.Reset();

	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(nullptr);
	}

	GeneratorListView->RequestListRefresh();
	return FReply::Handled();
}

FReply SGeneratorEditorWindow::OnMoveUpClicked()
{
	if (!CanMoveUp()) return FReply::Handled();
	const int32 Idx = GeneratorItems.IndexOfByKey(SelectedItem);
	GeneratorItems.Swap(Idx, Idx - 1);
	GeneratorListView->RequestListRefresh();
	return FReply::Handled();
}

FReply SGeneratorEditorWindow::OnMoveDownClicked()
{
	if (!CanMoveDown()) return FReply::Handled();
	const int32 Idx = GeneratorItems.IndexOfByKey(SelectedItem);
	GeneratorItems.Swap(Idx, Idx + 1);
	GeneratorListView->RequestListRefresh();
	return FReply::Handled();
}

bool SGeneratorEditorWindow::CanRemove() const
{
	return SelectedItem.IsValid();
}

bool SGeneratorEditorWindow::CanMoveUp() const
{
	if (!SelectedItem.IsValid()) return false;
	const int32 Idx = GeneratorItems.IndexOfByKey(SelectedItem);
	return Idx > 0;
}

bool SGeneratorEditorWindow::CanMoveDown() const
{
	if (!SelectedItem.IsValid()) return false;
	const int32 Idx = GeneratorItems.IndexOfByKey(SelectedItem);
	return Idx >= 0 && Idx < GeneratorItems.Num() - 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Reference Sequence picker
// ─────────────────────────────────────────────────────────────────────────────

void SGeneratorEditorWindow::OnReferenceSequenceSelected(const FAssetData& AssetData)
{
	SharedReferenceSequence = Cast<ULevelSequence>(AssetData.GetAsset());

	// Propagate immediately to all generators already in the stack
	for (const TSharedPtr<FGeneratorStackItem>& Item : GeneratorItems)
	{
		if (Item && Item->Generator)
		{
			Item->Generator->ReferenceSequence = SharedReferenceSequence.Get();
		}
	}
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
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("[GeneratorEditor] No editor world available"));
		return FReply::Handled();
	}

	int32 TotalCreated = 0;

	for (const TSharedPtr<FGeneratorStackItem>& Item : GeneratorItems)
	{
		UCDGTrajectoryGenerator* Gen = Item ? Item->Generator : nullptr;
		if (!Gen) continue;

		// Always push the shared reference sequence (overrides per-generator value)
		if (SharedReferenceSequence.IsValid())
		{
			Gen->ReferenceSequence = SharedReferenceSequence.Get();
		}

		// Re-outer to the current editor world so GetWorld() resolves inside Generate()
		if (Gen->GetOuter() != World)
		{
			Gen->Rename(
				nullptr, World,
				REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		}

		const TArray<ACDGTrajectory*> Created = Gen->Generate();
		TotalCreated += Created.Num();

		UE_LOG(LogCameraDatasetGenEditor, Log,
			TEXT("[GeneratorEditor] %s created %d trajectory/ies"),
			*Gen->GetGeneratorName().ToString(), Created.Num());
	}

	FNotificationInfo Info(FText::Format(
		LOCTEXT("GenerateDoneNotif", "Generation complete — {0} trajectories created"),
		FText::AsNumber(TotalCreated)));
	Info.ExpireDuration      = 4.f;
	Info.bUseLargeFont       = false;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

bool SGeneratorEditorWindow::CanGenerate() const
{
	return GeneratorItems.Num() > 0;
}

FText SGeneratorEditorWindow::GetGenerateTooltip() const
{
	if (GeneratorItems.Num() == 0)
	{
		return LOCTEXT("GenerateTipEmpty",
			"Add at least one generator to the stack before running");
	}
	return LOCTEXT("GenerateTip",
		"Run all generators in the stack sequentially and create trajectories in the current level");
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

	// Delete all trajectory actors via the subsystem
	const TArray<FName> Names = Subsystem->GetTrajectoryNames();
	for (const FName& Name : Names)
	{
		Subsystem->DeleteTrajectory(Name);
	}

	// Destroy any remaining keyframe actors (orphaned or not yet registered)
	TArray<ACDGKeyframe*> Keyframes;
	for (TActorIterator<ACDGKeyframe> It(World); It; ++It)
	{
		Keyframes.Add(*It);
	}
	for (ACDGKeyframe* Keyframe : Keyframes)
	{
		World->EditorDestroyActor(Keyframe, /*bShouldModifyLevel=*/true);
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
// Export Config
// ─────────────────────────────────────────────────────────────────────────────

FReply SGeneratorEditorWindow::OnExportConfigClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return FReply::Handled();

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	void* ParentWindowHandle =
		(ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	TArray<FString> OutFiles;
	const bool bSelected = DesktopPlatform->SaveFileDialog(
		ParentWindowHandle,
		TEXT("Export Generator Config"),
		FPaths::ProjectSavedDir(),
		TEXT("GeneratorConfig.json"),
		TEXT("JSON Files (*.json)|*.json|All Files (*.*)|*.*"),
		EFileDialogFlags::None,
		OutFiles);

	if (!bSelected || OutFiles.Num() == 0) return FReply::Handled();

	// Build JSON document
	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> GeneratorsArray;
	for (const TSharedPtr<FGeneratorStackItem>& Item : GeneratorItems)
	{
		UCDGTrajectoryGenerator* Gen = Item ? Item->Generator : nullptr;
		if (!Gen) continue;

		TSharedRef<FJsonObject> GenObj = MakeShared<FJsonObject>();
		GenObj->SetStringField(TEXT("class"), Gen->GetClass()->GetPathName());

		TSharedPtr<FJsonObject> ConfigObj = MakeShared<FJsonObject>();
		Gen->SerializeGeneratorConfig(ConfigObj);
		GenObj->SetObjectField(TEXT("config"), ConfigObj);

		GeneratorsArray.Add(MakeShared<FJsonValueObject>(GenObj));
	}

	RootObj->SetArrayField(TEXT("generators"), GeneratorsArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObj, Writer);

	const bool bSuccess = FFileHelper::SaveStringToFile(JsonString, *OutFiles[0]);

	FNotificationInfo Info(bSuccess
		? FText::Format(LOCTEXT("ExportConfigSuccess", "Config exported to:\n{0}"),
			FText::FromString(OutFiles[0]))
		: LOCTEXT("ExportConfigFailed", "Failed to export generator config"));
	Info.ExpireDuration       = 5.f;
	Info.bUseLargeFont        = false;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);

	if (bSuccess)
	{
		UE_LOG(LogCameraDatasetGenEditor, Log,
			TEXT("[GeneratorEditor] Config exported: %s (%d generators)"),
			*OutFiles[0], GeneratorItems.Num());
	}
	else
	{
		UE_LOG(LogCameraDatasetGenEditor, Error,
			TEXT("[GeneratorEditor] Failed to write config: %s"), *OutFiles[0]);
	}

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
	void* ParentWindowHandle =
		(ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	TArray<FString> OutFiles;
	const bool bSelected = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Load Generator Config"),
		FPaths::ProjectSavedDir(),
		TEXT(""),
		TEXT("JSON Files (*.json)|*.json|All Files (*.*)|*.*"),
		EFileDialogFlags::None,
		OutFiles);

	if (!bSelected || OutFiles.Num() == 0) return FReply::Handled();

	// Read file
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *OutFiles[0]))
	{
		FNotificationInfo Info(LOCTEXT("LoadConfigReadFailed", "Failed to read the config file"));
		Info.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FReply::Handled();
	}

	// Parse JSON
	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		FNotificationInfo Info(LOCTEXT("LoadConfigParseFailed", "Failed to parse the config JSON"));
		Info.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FReply::Handled();
	}

	const TArray<TSharedPtr<FJsonValue>>* GeneratorsArray = nullptr;
	if (!RootObj->TryGetArrayField(TEXT("generators"), GeneratorsArray) || !GeneratorsArray)
	{
		FNotificationInfo Info(LOCTEXT("LoadConfigInvalid",
			"Invalid config file: missing 'generators' array"));
		Info.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FReply::Handled();
	}

	// Clear existing stack
	for (const TSharedPtr<FGeneratorStackItem>& Item : GeneratorItems)
	{
		if (Item && Item->Generator && Item->Generator->IsRooted())
		{
			Item->Generator->RemoveFromRoot();
		}
	}
	GeneratorItems.Empty();
	SelectedItem.Reset();
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(nullptr);
	}

	// Reconstruct generators from JSON
	int32 LoadedCount = 0;
	for (const TSharedPtr<FJsonValue>& GenValue : *GeneratorsArray)
	{
		const TSharedPtr<FJsonObject>* GenObjPtr = nullptr;
		if (!GenValue->TryGetObject(GenObjPtr) || !GenObjPtr) continue;
		const TSharedPtr<FJsonObject>& GenObj = *GenObjPtr;

		FString ClassName;
		if (!GenObj->TryGetStringField(TEXT("class"), ClassName)) continue;

		UClass* Class = FindObject<UClass>(nullptr, *ClassName);
		if (!Class)
		{
			UE_LOG(LogCameraDatasetGenEditor, Warning,
				TEXT("[GeneratorEditor] Load: class not found: %s"), *ClassName);
			continue;
		}

		UCDGTrajectoryGenerator* Gen = CreateGeneratorInstance(Class);
		if (!Gen) continue;

		// Restore configuration
		const TSharedPtr<FJsonObject>* ConfigObjPtr = nullptr;
		if (GenObj->TryGetObjectField(TEXT("config"), ConfigObjPtr) && ConfigObjPtr)
		{
			Gen->FetchGeneratorConfig(*ConfigObjPtr);
		}

		// Apply the shared reference sequence
		if (SharedReferenceSequence.IsValid())
		{
			Gen->ReferenceSequence = SharedReferenceSequence.Get();
		}

		GeneratorItems.Add(MakeShared<FGeneratorStackItem>(Gen));
		++LoadedCount;
	}

	GeneratorListView->RequestListRefresh();

	FNotificationInfo Info(FText::Format(
		LOCTEXT("LoadConfigSuccess", "Loaded {0} generators from config"),
		FText::AsNumber(LoadedCount)));
	Info.ExpireDuration       = 4.f;
	Info.bUseLargeFont        = false;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);

	UE_LOG(LogCameraDatasetGenEditor, Log,
		TEXT("[GeneratorEditor] Config loaded: %s (%d generators)"),
		*OutFiles[0], LoadedCount);

	return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
// Close
// ─────────────────────────────────────────────────────────────────────────────

FReply SGeneratorEditorWindow::OnCloseClicked()
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (Window.IsValid())
	{
		Window->RequestDestroyWindow();
	}
	return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
// CDGGeneratorEditor::OpenWindow
// ─────────────────────────────────────────────────────────────────────────────

void CDGGeneratorEditor::OpenWindow()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Trajectory Generator Editor"))
		.ClientSize(FVector2D(950.f, 620.f))
		.MinWidth(700.f)
		.MinHeight(450.f);

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
