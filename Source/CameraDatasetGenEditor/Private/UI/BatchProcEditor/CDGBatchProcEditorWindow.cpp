// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BatchProcEditor/CDGBatchProcEditorWindow.h"
#include "UI/BatchProcEditor/CDGBatchProcExecService.h"
#include "Config/BatchProcConfig.h"
#include "Config/BatchProcConfigFactory.h"
#include "Config/GeneratorStackConfig.h"
#include "Config/LevelSeqExportConfig.h"
#include "LogCameraDatasetGenEditor.h"

// Skeleton-compatibility check
#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

// Anchor check
#include "Anchor/CDGLevelSceneAnchor.h"
#include "Anchor/CDGCharacterAnchor.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Engine/Engine.h"

// Slate
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"

// Editor
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Widgets/Notifications/SNotificationList.h"

// Asset tools
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "CDGBatchProcEditor"

// =============================================================================
// SBatchProcEditorWindow::Construct
// =============================================================================

void SBatchProcEditorWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10.f))
		[
			SNew(SVerticalBox)

			// ─── Top half: 3-column asset lists ────────────────────────────
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				// ── Column 1: Levels ─────────────────────────────────────
				+ SSplitter::Slot()
				.Value(0.33f)
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
							.Text(LOCTEXT("LevelsTitle", "Levels"))
							.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SAssignNew(LevelListView, SListView<TSharedPtr<FBatchLevelItem>>)
							.ListItemsSource(&LevelItems)
							.OnGenerateRow(this, &SBatchProcEditorWindow::GenerateLevelRow)
							.SelectionMode(ESelectionMode::None)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 4.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.f, 0.f, 4.f, 0.f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AddLevelLabel", "Add:"))
								.Font(FAppStyle::GetFontStyle("SmallFont"))
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							]

							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							[
						SNew(SObjectPropertyEntryBox)
							.AllowedClass(UWorld::StaticClass())
							.ObjectPath(this, &SBatchProcEditorWindow::GetStagedLevelPath)
							.OnObjectChanged(this, &SBatchProcEditorWindow::OnStagedLevelChanged)
							.AllowClear(false)
							.DisplayUseSelected(true)
							.DisplayBrowse(true)
							.ToolTipText(LOCTEXT("AddLevelTip", "Drag a level asset here or click Browse to add it to the list.\nOnly levels containing at least one CDGLevelSceneAnchor actor are shown."))
							.OnShouldFilterAsset(FOnShouldFilterAsset::CreateLambda([](const FAssetData& AssetData) -> bool
							{
								// Returns true = FILTER OUT (hide from picker).
								// Only show levels that contain at least one ACDGLevelSceneAnchor actor.
								//
								// Stage 1 — in-memory world scan (no I/O, most authoritative):
								//   If the UWorld is already loaded, iterate actors directly.
								//
								// Stage 2 — WP actor asset check:
								//   World Partition registers each actor as a separate AR asset under
								//   <LevelPackage>/__ExternalActors__/... — query for those.
								//
								// Stage 3 — AR package dependency check (works for ALL unloaded levels):
								//   Any map containing CDGLevelSceneAnchor must import its class from
								//   /Script/CameraDatasetGen. The AR indexes these hard imports as
								//   package dependencies when it scans the .umap file. If the map is
								//   indexed but has NO dependency on that script package, it cannot
								//   contain any CDGLevelSceneAnchor actors.

								// ── Stage 1: in-memory world scan ─────────────────────────────────
								UWorld* World = nullptr;
								if (GEngine)
								{
									for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
									{
										if (UWorld* W = Ctx.World())
										{
											if (W->GetOutermost()->GetFName() == AssetData.PackageName)
											{
												World = W;
												break;
											}
										}
									}
								}
								if (!World)
								{
									const FString WorldPath = AssetData.PackageName.ToString()
										+ TEXT(".") + AssetData.AssetName.ToString();
									World = FindObject<UWorld>(nullptr, *WorldPath);
								}
								if (World)
								{
									for (TActorIterator<ACDGLevelSceneAnchor> It(World); It; ++It)
										return false; // At least one anchor found → show
									return true;     // World loaded, no anchors found → filter out
								}

								IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

								// ── Stage 2: WP external actor assets ─────────────────────────────
								FARFilter WPFilter;
								WPFilter.ClassPaths.Add(ACDGLevelSceneAnchor::StaticClass()->GetClassPathName());
								WPFilter.PackagePaths.Add(AssetData.PackageName);
								WPFilter.bRecursivePaths = true;
								TArray<FAssetData> FoundAnchors;
								AR.GetAssets(WPFilter, FoundAnchors);
								if (FoundAnchors.Num() > 0)
									return false; // WP level has registered anchor actors → show

								// ── Stage 3: AR package dependency check ──────────────────────────
								// GetDependencies returns true when the package is known to the AR.
								// A false return means the package hasn't been scanned yet — allow through.
								TArray<FAssetDependency> Deps;
								const bool bKnown = AR.GetDependencies(
									FAssetIdentifier(AssetData.PackageName),
									Deps,
									UE::AssetRegistry::EDependencyCategory::Package);

								if (bKnown)
								{
									static const FName CDGScriptPkg(TEXT("/Script/CameraDatasetGen"));
									for (const FAssetDependency& Dep : Deps)
									{
										if (Dep.AssetId.PackageName == CDGScriptPkg)
											return false; // Has CDG script dep → may have anchors → show
									}
									// Package is indexed but has no /Script/CameraDatasetGen import.
									// It is impossible for it to contain a CDGLevelSceneAnchor.
									return true;
								}

								// Package not yet scanned by the AR → allow through (no false-positives)
								return false;
							}))
							]
						]
					]
				]

				// ── Column 2: Characters ─────────────────────────────────
				+ SSplitter::Slot()
				.Value(0.33f)
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
							.Text(LOCTEXT("CharactersTitle", "Characters"))
							.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SAssignNew(CharacterListView, SListView<TSharedPtr<FBatchCharacterItem>>)
							.ListItemsSource(&CharacterItems)
							.OnGenerateRow(this, &SBatchProcEditorWindow::GenerateCharacterRow)
							.SelectionMode(ESelectionMode::None)
						]

						// Add: Blueprint Character Class
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 4.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.f, 0.f, 4.f, 0.f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AddBPLabel", "Add:"))
								.Font(FAppStyle::GetFontStyle("SmallFont"))
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							]

							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							[
							SNew(SObjectPropertyEntryBox)
							.AllowedClass(UBlueprint::StaticClass())
							.ObjectPath(this, &SBatchProcEditorWindow::GetStagedCharacterBlueprintPath)
							.OnObjectChanged(this, &SBatchProcEditorWindow::OnStagedCharacterChanged)
							.AllowClear(false)
							.DisplayUseSelected(true)
							.DisplayBrowse(true)
							.ToolTipText(LOCTEXT("AddBPTip", "Drag a Blueprint character class here or click Browse to add it to the character list.\nOnly Blueprints that contain both CDGCharacterAnchor components and a SkeletalMeshComponent are shown."))
						.OnShouldFilterAsset(FOnShouldFilterAsset::CreateLambda([](const FAssetData& AssetData) -> bool
						{
							// Returns true = FILTER OUT (hide from picker).
							// Shows only Blueprint assets that have BOTH:
							//   (a) at least one UCDGCharacterAnchor component
							//   (b) at least one USkeletalMeshComponent
							//
							// Stage 1 (in-memory BPs): SCS parent-chain walk — runs before the AR
							//   dependency check so unsaved in-session changes are always respected.
							//   Checks for both component types in a single pass through the chain.
							//   SCS::GetAllNodes() only covers that Blueprint's own nodes, so the
							//   chain walk handles components inherited from parent Blueprints.
							//   The CDO GetComponents() approach is intentionally avoided: MetaHuman
							//   characters suppress the native ACharacter::Mesh via
							//   DoNotCreateDefaultSubobject, so the CDO carries no SkeletalMesh
							//   components — Body/Face etc. are SCS-added and never in OwnedComponents.
							//
							// Stage 2 (unloaded BPs): AR package dependency check.
							//   A Blueprint with UCDGCharacterAnchor SCS nodes hard-imports
							//   /Script/CameraDatasetGen. If the AR has scanned the package and
							//   finds no such import, it cannot contain CDG anchors → filter out.
							//   Only used for unloaded BPs; Stage 1 takes precedence for loaded ones
							//   since in-memory state may differ from the on-disk scan.
							//
							// Fallback: BP not in memory and package not yet scanned → allow through.

							// ── Stage 1: in-memory Blueprint SCS parent-chain walk ────────────────
							UBlueprint* BP = Cast<UBlueprint>(FindObject<UObject>(nullptr, *AssetData.GetSoftObjectPath().ToString()));
							if (BP)
							{
								bool bHasSkeletalMesh = false;
								bool bHasCDGAnchor    = false;

								for (UBlueprint* CurrentBP = BP;
									CurrentBP && !(bHasSkeletalMesh && bHasCDGAnchor); )
								{
									if (USimpleConstructionScript* SCS = CurrentBP->SimpleConstructionScript)
									{
										for (USCS_Node* Node : SCS->GetAllNodes())
										{
											if (!Node || !Node->ComponentClass) continue;
											if (!bHasSkeletalMesh && Node->ComponentClass->IsChildOf(USkeletalMeshComponent::StaticClass()))
												bHasSkeletalMesh = true;
											if (!bHasCDGAnchor && Node->ComponentClass->IsChildOf(UCDGCharacterAnchor::StaticClass()))
												bHasCDGAnchor = true;
										}
									}
									UClass* ParentClass = CurrentBP->ParentClass;
									CurrentBP = ParentClass
										? Cast<UBlueprint>(ParentClass->ClassGeneratedBy)
										: nullptr;
								}

								return !bHasSkeletalMesh || !bHasCDGAnchor;
							}

							// ── Stage 2: AR dependency check for unloaded packages ────────────────
							{
								IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
								static const FName CDGScriptPkg(TEXT("/Script/CameraDatasetGen"));
								TArray<FAssetDependency> Deps;
								const bool bKnown = AR.GetDependencies(
									FAssetIdentifier(AssetData.PackageName),
									Deps,
									UE::AssetRegistry::EDependencyCategory::Package);

								if (bKnown)
								{
									for (const FAssetDependency& Dep : Deps)
									{
										if (Dep.AssetId.PackageName == CDGScriptPkg)
											return false; // Has CDG dep → show
									}
									return true; // Scanned, no CDG dep → filter out
								}
							}

							return false; // Not yet scanned → allow through
						}))
							]
						]
					]
				]

				// ── Column 3: Animations ─────────────────────────────────
				+ SSplitter::Slot()
				.Value(0.34f)
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
							.Text(LOCTEXT("AnimationsTitle", "Animations"))
							.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SAssignNew(AnimationListView, SListView<TSharedPtr<FBatchAnimationItem>>)
							.ListItemsSource(&AnimationItems)
							.OnGenerateRow(this, &SBatchProcEditorWindow::GenerateAnimationRow)
							.SelectionMode(ESelectionMode::None)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 4.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.f, 0.f, 4.f, 0.f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AddAnimLabel", "Add:"))
								.Font(FAppStyle::GetFontStyle("SmallFont"))
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							]

							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							[
								SNew(SObjectPropertyEntryBox)
								.AllowedClass(UAnimationAsset::StaticClass())
								.ObjectPath(this, &SBatchProcEditorWindow::GetStagedAnimationPath)
								.OnObjectChanged(this, &SBatchProcEditorWindow::OnStagedAnimationChanged)
								.AllowClear(false)
								.DisplayUseSelected(true)
								.DisplayBrowse(true)
								.ToolTipText(LOCTEXT("AddAnimTip", "Drag an animation asset here or click Browse to add it to the list"))
							]
						]
					]
				]
			]

			// ─── Separator ─────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Separator"))
				.Padding(FMargin(0.f, 1.f))
			]

			// ─── Bottom half: config reference slots ─────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GenConfigLabel", "Generator Config:"))
					.MinDesiredWidth(140.f)
					.ToolTipText(LOCTEXT("GenConfigTip",
						"UGeneratorStackConfig asset — defines the trajectory generator stack used during batch processing."))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UGeneratorStackConfig::StaticClass())
					.ObjectPath(this, &SBatchProcEditorWindow::GetGeneratorConfigPath)
					.OnObjectChanged(this, &SBatchProcEditorWindow::OnGeneratorConfigChanged)
					.AllowClear(true)
					.DisplayUseSelected(true)
					.DisplayBrowse(true)
					.ToolTipText(LOCTEXT("GenConfigPickerTip",
						"Select the generator stack config to use for this batch job."))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ExpConfigLabel", "Exporter Config:"))
					.MinDesiredWidth(140.f)
					.ToolTipText(LOCTEXT("ExpConfigTip",
						"ULevelSeqExportConfig asset — defines export and render settings for the level sequence exporter."))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(ULevelSeqExportConfig::StaticClass())
					.ObjectPath(this, &SBatchProcEditorWindow::GetExporterConfigPath)
					.OnObjectChanged(this, &SBatchProcEditorWindow::OnExporterConfigChanged)
					.AllowClear(true)
					.DisplayUseSelected(true)
					.DisplayBrowse(true)
					.ToolTipText(LOCTEXT("ExpConfigPickerTip",
						"Select the level sequence exporter config to use for this batch job."))
				]
			]

			// ─── Separator ─────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Separator"))
				.Padding(FMargin(0.f, 1.f))
			]

			// ─── Batch config asset row ───────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BatchConfigLabel", "Batch Config Asset:"))
					.MinDesiredWidth(140.f)
					.ToolTipText(LOCTEXT("BatchConfigTip",
						"Select a saved BatchProcConfig to load it, or leave empty to save a new one."))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UBatchProcConfig::StaticClass())
					.ObjectPath(this, &SBatchProcEditorWindow::GetBatchConfigPath)
					.OnObjectChanged(this, &SBatchProcEditorWindow::OnBatchConfigChanged)
					.AllowClear(true)
					.DisplayUseSelected(true)
					.DisplayBrowse(true)
					.ToolTipText(LOCTEXT("BatchConfigPickerTip",
						"Selecting an asset immediately loads all its levels, characters, animations and config references into the UI."))
				]
			]

			// ─── Warning banner (visible only when warnings exist) ────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(FMargin(8.f, 4.f))
				.Visibility_Lambda([this]()
				{
					return HasAnyWarnings() ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("\u26A0")))
						.ColorAndOpacity(FLinearColor(1.f, 0.2f, 0.2f, 1.f))
						.Font(FAppStyle::GetFontStyle("NormalFontBold"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return GetWarningMessage(); })
						.ColorAndOpacity(FLinearColor(1.f, 0.2f, 0.2f, 1.f))
						.AutoWrapText(true)
					]
				]
			]

			// ─── Action buttons ───────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// Spacer to push buttons right
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("SaveBatchConfigBtn", "Export/Save Batch Config Asset"))
					.ToolTipText(LOCTEXT("SaveBatchConfigTip",
						"Save the current levels, characters, animations, and config references to a BatchProcConfig asset.\n"
						"If an asset is already selected above, it is updated in-place; otherwise a new asset is created."))
					.OnClicked(this, &SBatchProcEditorWindow::OnSaveBatchConfigClicked)
					.IsEnabled_Lambda([this]() { return !HasAnyWarnings(); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CancelBatchBtn", "Cancel"))
					.ToolTipText(LOCTEXT("CancelBatchTip", "Request cancellation of the running batch process."))
					.OnClicked(this, &SBatchProcEditorWindow::OnCancelBatchProcClicked)
					.IsEnabled_Lambda([this]() { return ActiveBatchService && ActiveBatchService->IsRunning(); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text_Lambda([this]()
					{
						if (ActiveBatchService && ActiveBatchService->IsRunning())
							return FText::Format(
								LOCTEXT("StartBatchBtnProgress", "Running... ({0}/{1})"),
								FText::AsNumber(BatchProgress),
								FText::AsNumber(BatchProgressTotal));
						return LOCTEXT("StartBatchBtn", "Start Batch Proc");
					})
					.ToolTipText(LOCTEXT("StartBatchTip", "Begin batch processing."))
					.OnClicked(this, &SBatchProcEditorWindow::OnStartBatchProcClicked)
					.IsEnabled_Lambda([this]()
					{
						const bool bRunning = ActiveBatchService && ActiveBatchService->IsRunning();
						return !HasAnyWarnings() && !bRunning;
					})
				]
			]
		]
	];
}

// =============================================================================
// Row generators
// =============================================================================

TSharedRef<ITableRow> SBatchProcEditorWindow::GenerateLevelRow(
	TSharedPtr<FBatchLevelItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const FText AssetName = Item.IsValid()
		? FText::FromName(Item->AssetData.AssetName)
		: LOCTEXT("UnknownAsset", "<unknown>");

	const FText AssetPath = Item.IsValid()
		? FText::FromString(Item->AssetData.GetSoftObjectPath().ToString())
		: FText::GetEmpty();

	return SNew(STableRow<TSharedPtr<FBatchLevelItem>>, OwnerTable)
		.Padding(FMargin(2.f, 1.f))
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(4.f, 2.f)
		[
			SNew(STextBlock)
			.Text(AssetName)
			.ToolTipText(AssetPath)
		]

		// Warning: no ACDGLevelSceneAnchor found in this level (only when world is loaded)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoAnchorsWarn", "\u26A0 No scene anchors"))
			.ColorAndOpacity(FLinearColor(1.f, 0.25f, 0.25f, 1.f))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
			.ToolTipText(LOCTEXT("NoAnchorsTip",
				"No ACDGLevelSceneAnchor actors were found in this level.\n"
				"Add at least one LevelSceneAnchor so the batch processor knows where to place characters."))
			.Visibility_Lambda([Item]()
			{
				return (Item.IsValid() && Item->bAnchorChecked && !Item->bHasAnchors)
					? EVisibility::Visible : EVisibility::Collapsed;
			})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.Text(FText::FromString(TEXT("\xD7")))  // multiplication sign ×
			.ToolTipText(LOCTEXT("RemoveLevelTip", "Remove this level from the list"))
			.OnClicked_Lambda([this, Item]() -> FReply { return OnRemoveLevelClicked(Item); })
		]
	];
}

TSharedRef<ITableRow> SBatchProcEditorWindow::GenerateCharacterRow(
	TSharedPtr<FBatchCharacterItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const FText AssetName = Item.IsValid()
		? FText::FromName(Item->AssetData.AssetName)
		: LOCTEXT("UnknownAsset", "<unknown>");

	const FText AssetPath = Item.IsValid()
		? FText::FromString(Item->AssetData.GetSoftObjectPath().ToString())
		: FText::GetEmpty();

	return SNew(STableRow<TSharedPtr<FBatchCharacterItem>>, OwnerTable)
		.Padding(FMargin(2.f, 1.f))
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(4.f, 2.f)
		[
			SNew(STextBlock)
			.Text(AssetName)
			.ToolTipText(AssetPath)
		]

		// Warning: no compatible animation found for this skeletal mesh's skeleton
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CharMismatchWarn", "\u26A0 No matching animation"))
			.ColorAndOpacity(FLinearColor(1.f, 0.25f, 0.25f, 1.f))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
			.ToolTipText(LOCTEXT("CharMismatchTip",
				"None of the animations in the list use a skeleton that is compatible with this skeletal mesh."))
			.Visibility_Lambda([Item]()
			{
				return (Item.IsValid() && Item->bHasMismatch) ? EVisibility::Visible : EVisibility::Collapsed;
			})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.Text(FText::FromString(TEXT("\xD7")))
			.ToolTipText(LOCTEXT("RemoveCharTip", "Remove this skeletal mesh from the list"))
			.OnClicked_Lambda([this, Item]() -> FReply { return OnRemoveCharacterClicked(Item); })
		]
	];
}

TSharedRef<ITableRow> SBatchProcEditorWindow::GenerateAnimationRow(
	TSharedPtr<FBatchAnimationItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const FText AssetName = Item.IsValid()
		? FText::FromName(Item->AssetData.AssetName)
		: LOCTEXT("UnknownAsset", "<unknown>");

	const FText AssetPath = Item.IsValid()
		? FText::FromString(Item->AssetData.GetSoftObjectPath().ToString())
		: FText::GetEmpty();

	return SNew(STableRow<TSharedPtr<FBatchAnimationItem>>, OwnerTable)
		.Padding(FMargin(2.f, 1.f))
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(4.f, 2.f)
		[
			SNew(STextBlock)
			.Text(AssetName)
			.ToolTipText(AssetPath)
		]

		// Warning: no compatible skeletal mesh for this animation's skeleton
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AnimMismatchWarn", "\u26A0 No matching skeleton"))
			.ColorAndOpacity(FLinearColor(1.f, 0.25f, 0.25f, 1.f))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
			.ToolTipText(LOCTEXT("AnimMismatchTip",
				"None of the skeletal meshes in the list share the skeleton used by this animation."))
			.Visibility_Lambda([Item]()
			{
				return (Item.IsValid() && Item->bHasMismatch) ? EVisibility::Visible : EVisibility::Collapsed;
			})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.Text(FText::FromString(TEXT("\xD7")))
			.ToolTipText(LOCTEXT("RemoveAnimTip", "Remove this animation from the list"))
			.OnClicked_Lambda([this, Item]() -> FReply { return OnRemoveAnimationClicked(Item); })
		]
	];
}

// =============================================================================
// Staged (add-picker) callbacks
// =============================================================================

void SBatchProcEditorWindow::OnStagedLevelChanged(const FAssetData& AssetData)
{
	StagedLevelPath = TEXT("");
	if (!AssetData.IsValid()) return;

	// Deduplicate
	for (const TSharedPtr<FBatchLevelItem>& Existing : LevelItems)
	{
		if (Existing.IsValid() && Existing->AssetData.GetSoftObjectPath() == AssetData.GetSoftObjectPath())
			return;
	}

	LevelItems.Add(MakeShared<FBatchLevelItem>(AssetData));
	RefreshAnchorStatuses();

	UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("[BatchProcEditor] Level added: %s"),
		*AssetData.AssetName.ToString());
}

void SBatchProcEditorWindow::OnStagedCharacterChanged(const FAssetData& AssetData)
{
	StagedCharacterBlueprintPath = TEXT("");
	if (!AssetData.IsValid()) return;

	for (const TSharedPtr<FBatchCharacterItem>& Existing : CharacterItems)
	{
		if (Existing.IsValid() && Existing->AssetData.GetSoftObjectPath() == AssetData.GetSoftObjectPath())
			return;
	}

	CharacterItems.Add(MakeShared<FBatchCharacterItem>(AssetData));
	if (CharacterListView.IsValid()) CharacterListView->RequestListRefresh();

	RefreshMismatchWarnings();
}

void SBatchProcEditorWindow::OnStagedAnimationChanged(const FAssetData& AssetData)
{
	StagedAnimationPath = TEXT("");
	if (!AssetData.IsValid()) return;

	for (const TSharedPtr<FBatchAnimationItem>& Existing : AnimationItems)
	{
		if (Existing.IsValid() && Existing->AssetData.GetSoftObjectPath() == AssetData.GetSoftObjectPath())
			return;
	}

	AnimationItems.Add(MakeShared<FBatchAnimationItem>(AssetData));
	if (AnimationListView.IsValid()) AnimationListView->RequestListRefresh();

	RefreshMismatchWarnings();
}

FString SBatchProcEditorWindow::GetStagedLevelPath() const              { return StagedLevelPath;             }
FString SBatchProcEditorWindow::GetStagedCharacterPath() const          { return StagedCharacterPath;         }
FString SBatchProcEditorWindow::GetStagedCharacterBlueprintPath() const { return StagedCharacterBlueprintPath; }
FString SBatchProcEditorWindow::GetStagedAnimationPath() const          { return StagedAnimationPath;         }

// =============================================================================
// Remove callbacks
// =============================================================================

FReply SBatchProcEditorWindow::OnRemoveLevelClicked(TSharedPtr<FBatchLevelItem> Item)
{
	LevelItems.Remove(Item);
	RefreshAnchorStatuses();
	return FReply::Handled();
}

FReply SBatchProcEditorWindow::OnRemoveCharacterClicked(TSharedPtr<FBatchCharacterItem> Item)
{
	CharacterItems.Remove(Item);
	if (CharacterListView.IsValid()) CharacterListView->RequestListRefresh();
	RefreshMismatchWarnings();
	return FReply::Handled();
}

FReply SBatchProcEditorWindow::OnRemoveAnimationClicked(TSharedPtr<FBatchAnimationItem> Item)
{
	AnimationItems.Remove(Item);
	if (AnimationListView.IsValid()) AnimationListView->RequestListRefresh();
	RefreshMismatchWarnings();
	return FReply::Handled();
}

// =============================================================================
// Config asset slot callbacks
// =============================================================================

void SBatchProcEditorWindow::OnGeneratorConfigChanged(const FAssetData& AssetData)
{
	GeneratorConfig = Cast<UGeneratorStackConfig>(AssetData.GetAsset());
}

FString SBatchProcEditorWindow::GetGeneratorConfigPath() const
{
	return GeneratorConfig.IsValid() ? GeneratorConfig->GetPathName() : FString();
}

void SBatchProcEditorWindow::OnExporterConfigChanged(const FAssetData& AssetData)
{
	ExporterConfig = Cast<ULevelSeqExportConfig>(AssetData.GetAsset());
}

FString SBatchProcEditorWindow::GetExporterConfigPath() const
{
	return ExporterConfig.IsValid() ? ExporterConfig->GetPathName() : FString();
}

void SBatchProcEditorWindow::OnBatchConfigChanged(const FAssetData& AssetData)
{
	UBatchProcConfig* Config = Cast<UBatchProcConfig>(AssetData.GetAsset());
	LoadedBatchConfig = Config;
	if (Config)
	{
		PopulateFromConfig(Config);
	}
}

FString SBatchProcEditorWindow::GetBatchConfigPath() const
{
	return LoadedBatchConfig.IsValid() ? LoadedBatchConfig->GetPathName() : FString();
}

// =============================================================================
// Action buttons
// =============================================================================

FReply SBatchProcEditorWindow::OnSaveBatchConfigClicked()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	auto DoWrite = [this](UBatchProcConfig* Config)
	{
		WriteToConfig(Config);

		FNotificationInfo Info(LOCTEXT("BatchConfigSaved", "Batch proc config saved"));
		Info.ExpireDuration       = 3.f;
		Info.bUseSuccessFailIcons = true;
		FSlateNotificationManager::Get().AddNotification(Info);
	};

	if (LoadedBatchConfig.IsValid())
	{
		DoWrite(LoadedBatchConfig.Get());
	}
	else
	{
		UBatchProcConfigFactory* Factory = NewObject<UBatchProcConfigFactory>();
		UObject* NewObj = AssetTools.CreateAssetWithDialog(
			TEXT("BatchProcConfig"), TEXT("/Game"),
			UBatchProcConfig::StaticClass(), Factory);

		if (UBatchProcConfig* NewConfig = Cast<UBatchProcConfig>(NewObj))
		{
			DoWrite(NewConfig);
			LoadedBatchConfig = NewConfig;
		}
	}

	return FReply::Handled();
}

FReply SBatchProcEditorWindow::OnStartBatchProcClicked()
{
	if (ActiveBatchService && ActiveBatchService->IsRunning())
		return FReply::Handled();

	if (!GeneratorConfig.IsValid())
	{
		FNotificationInfo Info(LOCTEXT("BatchNoGenConfig", "Please assign a Generator Config before starting the batch."));
		Info.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FReply::Handled();
	}

	// ── Build input ──────────────────────────────────────────────────────────
	FBatchProcInput Input;

	for (const TSharedPtr<FBatchLevelItem>& Item : LevelItems)
		if (Item.IsValid()) Input.Levels.Add(Item->AssetData);

	for (const TSharedPtr<FBatchCharacterItem>& Item : CharacterItems)
		if (Item.IsValid()) Input.Characters.Add(Item->AssetData);

	for (const TSharedPtr<FBatchAnimationItem>& Item : AnimationItems)
		if (Item.IsValid()) Input.Animations.Add(Item->AssetData);

	Input.GeneratorConfig = GeneratorConfig;
	Input.ExporterConfig  = ExporterConfig;

	// ── Create and start service ─────────────────────────────────────────────
	BatchProgress      = 0;
	BatchProgressTotal = 0;
	BatchLog.Empty();

	ActiveBatchService = UCDGBatchProcExecService::Create(GetTransientPackage(), Input);
	ActiveBatchService->AddToRoot(); // prevent GC while running

	ActiveBatchService->OnProgressUpdated.AddLambda(
		[this](int32 Completed, int32 Total)
		{
			BatchProgress      = Completed;
			BatchProgressTotal = Total;
		});

	ActiveBatchService->OnLogMessage.AddLambda(
		[this](const FString& Msg)
		{
			BatchLog.Add(Msg);
			// Keep the last 200 lines to avoid unbounded growth
			if (BatchLog.Num() > 200) BatchLog.RemoveAt(0, BatchLog.Num() - 200);
		});

	ActiveBatchService->OnBatchCompleted.AddLambda(
		[this](bool bSuccess)
		{
			if (ActiveBatchService) ActiveBatchService->RemoveFromRoot();
		});

	ActiveBatchService->Start();

	FNotificationInfo Info(LOCTEXT("BatchStarted", "Batch processing started."));
	Info.ExpireDuration = 3.f;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

FReply SBatchProcEditorWindow::OnCancelBatchProcClicked()
{
	if (ActiveBatchService && ActiveBatchService->IsRunning())
	{
		ActiveBatchService->Cancel();
	}
	return FReply::Handled();
}

// =============================================================================
// Helpers
// =============================================================================

void SBatchProcEditorWindow::RefreshMismatchWarnings()
{
	// ── Helper: collect all skeletons from a character item ─────────────────
	// Handles both raw USkeletalMesh assets and Blueprint character classes.
	//
	// For Blueprints, skeletons are gathered from ALL SkeletalMeshComponent SCS
	// node templates across the full Blueprint parent chain, rather than only
	// the root-closest component. This is correct because a level sequence can
	// target any named component (Body, Face, etc.) in the BP, and an animation
	// should be considered compatible as long as its skeleton matches any one of
	// them. Restricting to root-closest depth incorrectly rejects face/secondary
	// animations and produces false mismatch warnings.
	//
	// CDO->GetComponents() is intentionally NOT used: MetaHumans suppress the
	// native ACharacter::Mesh via DoNotCreateDefaultSubobject, so the CDO carries
	// no SkeletalMeshComponents — Body/Face etc. are SCS-added component
	// templates, never in the CDO's OwnedComponents list.
	auto GetSkeletonsForCharItem = [](const TSharedPtr<FBatchCharacterItem>& CharItem, TSet<USkeleton*>& OutSkeletons)
	{
		if (!CharItem.IsValid()) return;
		UObject* Asset = CharItem->AssetData.GetAsset();

		// Case 1: raw skeletal mesh asset
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset))
		{
			if (USkeleton* Skel = Mesh->GetSkeleton())
				OutSkeletons.Add(Skel);
			return;
		}

		// Case 2: Blueprint character class — walk the SCS parent chain and
		// collect skeletons from every SkeletalMeshComponent template found.
		if (UBlueprint* BP = Cast<UBlueprint>(Asset))
		{
			for (UBlueprint* CurrentBP = BP; CurrentBP; )
			{
				if (USimpleConstructionScript* SCS = CurrentBP->SimpleConstructionScript)
				{
					for (USCS_Node* Node : SCS->GetAllNodes())
					{
						if (!Node || !Node->ComponentClass) continue;
						if (!Node->ComponentClass->IsChildOf(USkeletalMeshComponent::StaticClass())) continue;

						if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate))
						{
							if (USkeletalMesh* Mesh = SkelComp->GetSkeletalMeshAsset())
							{
								if (USkeleton* Skel = Mesh->GetSkeleton())
									OutSkeletons.Add(Skel);
							}
						}
					}
				}
				UClass* ParentClass = CurrentBP->ParentClass;
				CurrentBP = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
			}
		}
	};

	// ── Collect all character skeletons ──────────────────────────────────────
	TSet<USkeleton*> CharSkeletons;
	for (const TSharedPtr<FBatchCharacterItem>& CharItem : CharacterItems)
	{
		GetSkeletonsForCharItem(CharItem, CharSkeletons);
	}

	// ── Collect all animation skeletons ──────────────────────────────────────
	TSet<USkeleton*> AnimSkeletons;
	for (const TSharedPtr<FBatchAnimationItem>& AnimItem : AnimationItems)
	{
		if (!AnimItem.IsValid()) continue;
		if (UAnimationAsset* Anim = Cast<UAnimationAsset>(AnimItem->AssetData.GetAsset()))
		{
			if (USkeleton* Skel = Anim->GetSkeleton())
				AnimSkeletons.Add(Skel);
		}
	}

	// ── Compatibility helper: USkeleton::IsCompatibleForEditor handles retargeting
	// chains in addition to direct pointer equality, matching the engine's own
	// animation picker filter logic (ShouldFilterAsset / IsCompatibleForEditor).
	auto SkeletonsCompatible = [](USkeleton* A, USkeleton* B) -> bool
	{
		if (!A || !B) return false;
		return A->IsCompatibleForEditor(B);
	};

	// ── Per-character mismatch: warn when animations exist but none match ────
	const bool bHasAnims = AnimationItems.Num() > 0;
	for (const TSharedPtr<FBatchCharacterItem>& CharItem : CharacterItems)
	{
		if (!CharItem.IsValid()) continue;
		if (bHasAnims)
		{
			TSet<USkeleton*> ThisSkeletons;
			GetSkeletonsForCharItem(CharItem, ThisSkeletons);

			// Mismatch only when skeletons could be resolved but none were compatible.
			bool bAnyMatch = false;
			for (USkeleton* CharSkel : ThisSkeletons)
			{
				for (USkeleton* AnimSkel : AnimSkeletons)
				{
					if (SkeletonsCompatible(CharSkel, AnimSkel)) { bAnyMatch = true; break; }
				}
				if (bAnyMatch) break;
			}
			CharItem->bHasMismatch = !ThisSkeletons.IsEmpty() && !bAnyMatch;
		}
		else
		{
			CharItem->bHasMismatch = false;
		}
	}

	// ── Per-animation mismatch: warn when characters exist but none match ────
	const bool bHasChars = CharacterItems.Num() > 0;
	for (const TSharedPtr<FBatchAnimationItem>& AnimItem : AnimationItems)
	{
		if (!AnimItem.IsValid()) continue;
		if (bHasChars)
		{
			if (UAnimationAsset* Anim = Cast<UAnimationAsset>(AnimItem->AssetData.GetAsset()))
			{
				USkeleton* AnimSkel = Anim->GetSkeleton();
				if (AnimSkel)
				{
					bool bAnyMatch = false;
					for (USkeleton* CharSkel : CharSkeletons)
					{
						if (SkeletonsCompatible(AnimSkel, CharSkel)) { bAnyMatch = true; break; }
					}
					AnimItem->bHasMismatch = !bAnyMatch;
				}
				else
				{
					AnimItem->bHasMismatch = false;
				}
			}
		}
		else
		{
			AnimItem->bHasMismatch = false;
		}
	}

	if (CharacterListView.IsValid()) CharacterListView->RequestListRefresh();
	if (AnimationListView.IsValid())  AnimationListView->RequestListRefresh();
}

void SBatchProcEditorWindow::PopulateFromConfig(UBatchProcConfig* Config)
{
	check(Config);

	LevelItems.Empty();
	CharacterItems.Empty();
	AnimationItems.Empty();

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	for (const FSoftObjectPath& Path : Config->Levels)
	{
		FAssetData Data = AR.GetAssetByObjectPath(Path);
		if (Data.IsValid())
			LevelItems.Add(MakeShared<FBatchLevelItem>(Data));
	}

	for (const FSoftObjectPath& Path : Config->SkeletalMeshes)
	{
		FAssetData Data = AR.GetAssetByObjectPath(Path);
		if (Data.IsValid())
			CharacterItems.Add(MakeShared<FBatchCharacterItem>(Data));
	}

	for (const FSoftObjectPath& Path : Config->Animations)
	{
		FAssetData Data = AR.GetAssetByObjectPath(Path);
		if (Data.IsValid())
			AnimationItems.Add(MakeShared<FBatchAnimationItem>(Data));
	}

	GeneratorConfig = Config->GeneratorConfig;
	ExporterConfig  = Config->ExporterConfig;

	if (LevelListView.IsValid())     LevelListView->RequestListRefresh();
	if (CharacterListView.IsValid()) CharacterListView->RequestListRefresh();
	if (AnimationListView.IsValid()) AnimationListView->RequestListRefresh();

	RefreshMismatchWarnings();
	RefreshAnchorStatuses();

	UE_LOG(LogCameraDatasetGenEditor, Log,
		TEXT("[BatchProcEditor] Loaded from config: %d levels, %d characters, %d animations"),
		LevelItems.Num(), CharacterItems.Num(), AnimationItems.Num());
}

void SBatchProcEditorWindow::WriteToConfig(UBatchProcConfig* Config) const
{
	check(Config);
	Config->Modify();

	Config->Levels.Empty();
	for (const TSharedPtr<FBatchLevelItem>& Item : LevelItems)
	{
		if (Item.IsValid())
			Config->Levels.Add(Item->AssetData.GetSoftObjectPath());
	}

	Config->SkeletalMeshes.Empty();
	for (const TSharedPtr<FBatchCharacterItem>& Item : CharacterItems)
	{
		if (Item.IsValid())
			Config->SkeletalMeshes.Add(Item->AssetData.GetSoftObjectPath());
	}

	Config->Animations.Empty();
	for (const TSharedPtr<FBatchAnimationItem>& Item : AnimationItems)
	{
		if (Item.IsValid())
			Config->Animations.Add(Item->AssetData.GetSoftObjectPath());
	}

	Config->GeneratorConfig = GeneratorConfig.IsValid() ? GeneratorConfig.Get() : nullptr;
	Config->ExporterConfig  = ExporterConfig.IsValid()  ? ExporterConfig.Get()  : nullptr;

	Config->MarkPackageDirty();

	UPackage* Package = Config->GetOutermost();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, Config, *PackageFilename, SaveArgs);
}

// =============================================================================
// HasAnyWarnings / GetWarningMessage
// =============================================================================

bool SBatchProcEditorWindow::HasAnyWarnings() const
{
	// Block execution when required lists are empty.
	if (LevelItems.IsEmpty() || CharacterItems.IsEmpty() || AnimationItems.IsEmpty())
		return true;

	for (const TSharedPtr<FBatchLevelItem>& Item : LevelItems)
	{
		if (Item.IsValid() && Item->bAnchorChecked && !Item->bHasAnchors)
			return true;
	}
	for (const TSharedPtr<FBatchCharacterItem>& Item : CharacterItems)
	{
		if (Item.IsValid() && Item->bHasMismatch)
			return true;
	}
	for (const TSharedPtr<FBatchAnimationItem>& Item : AnimationItems)
	{
		if (Item.IsValid() && Item->bHasMismatch)
			return true;
	}
	return false;
}

FText SBatchProcEditorWindow::GetWarningMessage() const
{
	// Empty-list message takes priority over item-level warnings.
	if (LevelItems.IsEmpty() || CharacterItems.IsEmpty() || AnimationItems.IsEmpty())
	{
		TArray<FString> Missing;
		if (LevelItems.IsEmpty())     Missing.Add(TEXT("level"));
		if (CharacterItems.IsEmpty()) Missing.Add(TEXT("character"));
		if (AnimationItems.IsEmpty()) Missing.Add(TEXT("animation"));

		const FString List = FString::Join(Missing, TEXT(", "));
		return FText::Format(
			LOCTEXT("EmptyListWarning",
				"Add at least one {0} before saving or running the batch process."),
			FText::FromString(List));
	}

	return LOCTEXT("WarningBannerText",
		"All warnings must be resolved before batch execution.");
}

// =============================================================================
// RefreshAnchorStatuses
// =============================================================================

void SBatchProcEditorWindow::RefreshAnchorStatuses()
{
	// Build a package-name → UWorld map from every world currently loaded in the engine.
	// This covers the editor world, all sub-levels and any PIE worlds reliably.
	TMap<FName, UWorld*> LoadedWorlds;
	if (GEngine)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (UWorld* W = Ctx.World())
			{
				LoadedWorlds.Add(W->GetOutermost()->GetFName(), W);
			}
		}
	}

	for (const TSharedPtr<FBatchLevelItem>& LevelItem : LevelItems)
	{
		if (!LevelItem.IsValid()) continue;

		// Primary lookup: package name (e.g. /Game/Levels/EditorPaths)
		UWorld* World = nullptr;
		if (UWorld** Found = LoadedWorlds.Find(LevelItem->AssetData.PackageName))
		{
			World = *Found;
		}

		// Secondary lookup: full object path via FindObject (handles sub-level packages)
		if (!World)
		{
			const FString WorldPath = LevelItem->AssetData.PackageName.ToString()
				+ TEXT(".") + LevelItem->AssetData.AssetName.ToString();
			World = FindObject<UWorld>(nullptr, *WorldPath);
		}

		if (!World)
		{
			// Level not loaded — mark as unchecked (no false warnings for unloaded maps)
			LevelItem->bAnchorChecked = false;
			LevelItem->bHasAnchors    = true;
			continue;
		}

		LevelItem->bAnchorChecked = true;
		LevelItem->bHasAnchors    = false;

		for (TActorIterator<ACDGLevelSceneAnchor> It(World); It; ++It)
		{
			LevelItem->bHasAnchors = true;
			break;
		}
	}

	if (LevelListView.IsValid()) LevelListView->RequestListRefresh();
}

// =============================================================================
// CDGBatchProcEditor::OpenWindow
// =============================================================================

void CDGBatchProcEditor::OpenWindow()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Batch Proc Editor"))
		.ClientSize(FVector2D(900.f, 660.f))
		.MinWidth(640.f)
		.MinHeight(480.f);

	Window->SetContent(SNew(SBatchProcEditorWindow));

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
