// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/LevelSeqExporterWindow/CDGLevelSeqExporter.h"
#include "Config/LevelSeqExportConfig.h"
#include "Config/LevelSeqExportConfigFactory.h"
#include "Trajectory/CDGKeyframe.h"
#include "Anchor/CDGCharacterAnchor.h"
#include "LevelSequenceInterface/CDGLevelSeqSubsystem.h"
#include "MRQInterface/CDGMRQInterface.h"
#include "IO/TrajectorySL.h"
#include "LogCameraDatasetGenEditor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "EditorStyleSet.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "CineCameraSettings.h"
#include "Components/ActorComponent.h"
#include "Factories/Factory.h"
#include "UObject/SavePackage.h"
#include "UniversalObjectLocatorResolveParams.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/PlatformFileManager.h"

#define LOCTEXT_NAMESPACE "CDGLevelSeqExporter"

// ---------------------------------------------------------------------------
// FTrajectoryExportItem
// ---------------------------------------------------------------------------

FTrajectoryExportItem::FTrajectoryExportItem(ACDGTrajectory* InTrajectory)
    : Trajectory(InTrajectory)
    , bExport(true)
{
    if (InTrajectory)
    {
        Name = InTrajectory->TrajectoryName.ToString();
        Duration = InTrajectory->GetTrajectoryDuration();
    }
}

// ---------------------------------------------------------------------------
// SLevelSeqExporterWindow::Construct
// ---------------------------------------------------------------------------

void SLevelSeqExporterWindow::Construct(const FArguments& InArgs, const TArray<ACDGTrajectory*>& InTrajectories)
{
    TrajectoryItems.Empty();
    for (ACDGTrajectory* Trajectory : InTrajectories)
    {
        if (Trajectory)
        {
            TrajectoryItems.Add(MakeShared<FTrajectoryExportItem>(Trajectory));
        }
    }

    bIsBaseLevelSequenceValid = true;
    BaseLevelSequenceValidationError.Empty();
    BaseShotDurationSeconds = 0.0;
    BaseShotSequence.Reset();

    // Output format options (same set as the standalone MRQ window)
    OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::PNG_Sequence));
    OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::EXR_Sequence));
    OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::BMP_Sequence));
    OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::H264_Video));
    OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::WAV_Audio));
    OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::CommandLineEncoder));
    OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::FinalCutProXML));
    SelectedOutputFormat = OutputFormatOptions[0];

    FString DefaultOutputDir = FPaths::ProjectSavedDir() / TEXT("MovieRenders");

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(FMargin(10.0f))
        [
            SNew(SVerticalBox)

            // ----------------------------------------------------------------
            // TOP: Trajectory list (left) | Summary (right)
            // ----------------------------------------------------------------
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .MaxHeight(340.0f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Horizontal)

                // LEFT: Trajectory List
                + SSplitter::Slot()
                .Value(0.4f)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
                    .Padding(FMargin(5.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 5)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("TrajectoryListTitle", "Trajectories"))
                            .Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
                        ]
                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SAssignNew(TrajectoryListView, SListView<TSharedPtr<FTrajectoryExportItem>>)
                            .ListItemsSource(&TrajectoryItems)
                            .OnGenerateRow(this, &SLevelSeqExporterWindow::GenerateTrajectoryRow)
                            .OnSelectionChanged(this, &SLevelSeqExporterWindow::OnSelectionChanged)
                            .SelectionMode(ESelectionMode::Single)
                        ]
                    ]
                ]

                // RIGHT: Summary
                + SSplitter::Slot()
                .Value(0.6f)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
                    .Padding(FMargin(10.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 10)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("SummaryTitle", "Summary Information"))
                            .Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
                        ]
                        // Name
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 2)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0, 0, 10, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("SummaryNameLabel", "Name:"))
                                .Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            [
                                SAssignNew(SummaryNameText, STextBlock)
                                .Text(LOCTEXT("NoSelection", "None"))
                            ]
                        ]
                        // Duration
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 2)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0, 0, 10, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("SummaryDurationLabel", "Duration:"))
                                .Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            [
                                SAssignNew(SummaryDurationText, STextBlock)
                                .Text(LOCTEXT("ZeroDuration", "0.0s"))
                            ]
                        ]
                        // Keyframes
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 2)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0, 0, 10, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("SummaryKeyframesLabel", "Keyframes:"))
                                .Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            [
                                SAssignNew(SummaryKeyframeCountText, STextBlock)
                                .Text(LOCTEXT("ZeroKeyframes", "0"))
                            ]
                        ]
                        // Text Prompt
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 10, 0, 2)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("SummaryPromptLabel", "Text Prompt:"))
                            .Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 2)
                        .MaxHeight(100.0f)
                        [
                            SAssignNew(SummaryPromptText, SMultiLineEditableTextBox)
                            .HintText(LOCTEXT("PromptHint", "Enter text prompt here..."))
                            .OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
                            {
                                if (SelectedItem.IsValid() && SelectedItem->Trajectory.IsValid())
                                {
                                    ACDGTrajectory* Traj = SelectedItem->Trajectory.Get();
                                    Traj->Modify();
                                    Traj->TextPrompt = NewText.ToString();
                                }
                            })
                        ]
                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        .Padding(0, 20, 0, 0)
                        [
                            SAssignNew(SummaryInfoText, STextBlock)
                            .Text(LOCTEXT("SelectInstruction", "Select a trajectory to view details."))
                            .AutoWrapText(true)
                            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                        ]
                    ]
                ]
            ]

            // ----------------------------------------------------------------
            // BOTTOM: Scrollable settings
            // ----------------------------------------------------------------
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(0, 8, 0, 0)
            [
                SNew(SScrollBox)

                // ---- Config Save / Load ----
                + SScrollBox::Slot()
                .Padding(0, 4, 0, 0)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 10, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ConfigAssetLabel", "Config Asset:"))
                        .MinDesiredWidth(130.0f)
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(0, 0, 5, 0)
                    [
                        SNew(SObjectPropertyEntryBox)
                        .AllowedClass(ULevelSeqExportConfig::StaticClass())
                        .ObjectPath(this, &SLevelSeqExporterWindow::GetLoadedConfigPath)
                        .OnObjectChanged(this, &SLevelSeqExporterWindow::OnLoadConfigChanged)
                        .AllowClear(true)
                        .DisplayUseSelected(true)
                        .DisplayBrowse(true)
                        .ToolTipText(LOCTEXT("ConfigAssetTooltip", "Select a saved export config to load its settings, or leave empty to use current values"))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("SaveConfigButton", "Save Config"))
                        .OnClicked(this, &SLevelSeqExporterWindow::OnSaveConfigClicked)
                        .ToolTipText(LOCTEXT("SaveConfigTooltip", "Save current settings to the selected config asset, or create a new one"))
                    ]
                ]

                // ---- Separator ----
                + SScrollBox::Slot()
                .Padding(0, 8)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("Menu.Separator"))
                    .Padding(FMargin(0, 2))
                ]

                // ---- Export Settings ----
                + SScrollBox::Slot()
                .Padding(0, 4, 0, 0)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ExportSettingsTitle", "Export Settings"))
                    .Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
                ]
                + SScrollBox::Slot()
                .Padding(0, 6)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 10, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("FPSLabel", "FPS:"))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 20, 0)
                    [
                        SAssignNew(FPSInput, SSpinBox<int32>)
                        .MinValue(1)
                        .MaxValue(240)
                        .Value(30)
                        .MinSliderValue(1)
                        .MaxSliderValue(120)
                        .MinDesiredWidth(70.0f)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SAssignNew(ClearSequenceCheckBox, SCheckBox)
                        .IsChecked(ECheckBoxState::Checked)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("ClearSequenceLabel", "Clear Level Sequence"))
                        ]
                    ]
                ]
                + SScrollBox::Slot()
                .Padding(0, 4)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 10, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("BaseLevelSequenceLabel", "Base Level Sequence:"))
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SNew(SObjectPropertyEntryBox)
                        .AllowedClass(ULevelSequence::StaticClass())
                        .ObjectPath(this, &SLevelSeqExporterWindow::GetBaseLevelSequencePath)
                        .OnObjectChanged(this, &SLevelSeqExporterWindow::OnBaseLevelSequenceSelected)
                        .AllowClear(true)
                        .DisplayUseSelected(true)
                        .DisplayBrowse(true)
                    ]
                ]
                + SScrollBox::Slot()
                .Padding(0, 2, 0, 4)
                [
                    SNew(STextBlock)
                    .Text(this, &SLevelSeqExporterWindow::GetBaseLevelSequenceValidationMessage)
                    .ColorAndOpacity(FLinearColor(0.85f, 0.15f, 0.15f, 1.0f))
                    .AutoWrapText(true)
                ]

                // ---- Separator ----
                + SScrollBox::Slot()
                .Padding(0, 8)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("Menu.Separator"))
                    .Padding(FMargin(0, 2))
                ]

                // ---- Render Settings ----
                + SScrollBox::Slot()
                .Padding(0, 4, 0, 0)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("RenderSettingsTitle", "Render Settings"))
                    .Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
                ]
                // Output Directory
                + SScrollBox::Slot()
                .Padding(0, 6)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 10, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("OutputDirLabel", "Output Directory:"))
                        .MinDesiredWidth(130.0f)
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(0, 0, 5, 0)
                    [
                        SAssignNew(OutputDirTextBox, SEditableTextBox)
                        .Text(FText::FromString(DefaultOutputDir))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("BrowseButton", "Browse..."))
                        .OnClicked(this, &SLevelSeqExporterWindow::OnBrowseOutputDirClicked)
                    ]
                ]
                // Resolution
                + SScrollBox::Slot()
                .Padding(0, 5)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 10, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ResolutionLabel", "Resolution:"))
                        .MinDesiredWidth(130.0f)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(ResolutionWidthInput, SSpinBox<int32>)
                        .MinValue(1)
                        .MaxValue(7680)
                        .Value(1920)
                        .MinDesiredWidth(100.0f)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(5, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ResolutionX", "x"))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(ResolutionHeightInput, SSpinBox<int32>)
                        .MinValue(1)
                        .MaxValue(4320)
                        .Value(1080)
                        .MinDesiredWidth(100.0f)
                    ]
                ]
                // Export Format
                + SScrollBox::Slot()
                .Padding(0, 5)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 10, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("OutputFormatLabel", "Export Format:"))
                        .MinDesiredWidth(130.0f)
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SAssignNew(OutputFormatComboBox, SComboBox<TSharedPtr<ECDGRenderOutputFormat>>)
                        .OptionsSource(&OutputFormatOptions)
                        .OnGenerateWidget(this, &SLevelSeqExporterWindow::MakeOutputFormatWidget)
                        .OnSelectionChanged(this, &SLevelSeqExporterWindow::OnOutputFormatChanged)
                        .InitiallySelectedItem(SelectedOutputFormat)
                        [
                            SNew(STextBlock)
                            .Text(this, &SLevelSeqExporterWindow::GetOutputFormatText)
                        ]
                    ]
                ]
                // Export Index JSON
                + SScrollBox::Slot()
                .Padding(0, 5)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 10, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ExportJSONLabel", "Export Index JSON:"))
                        .MinDesiredWidth(130.0f)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(ExportIndexJSONCheckBox, SCheckBox)
                        .IsChecked(ECheckBoxState::Checked)
                    ]
                ]
                // Overwrite Existing
                + SScrollBox::Slot()
                .Padding(0, 5)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 10, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("OverwriteLabel", "Overwrite Existing:"))
                        .MinDesiredWidth(130.0f)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(OverwriteExistingCheckBox, SCheckBox)
                        .IsChecked(ECheckBoxState::Unchecked)
                    ]
                ]

                // ---- Separator ----
                + SScrollBox::Slot()
                .Padding(0, 8)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("Menu.Separator"))
                    .Padding(FMargin(0, 2))
                ]

                // ---- Quality Settings ----
                + SScrollBox::Slot()
                .Padding(0, 4, 0, 0)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("QualitySettingsTitle", "Quality Settings"))
                    .Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
                ]
                // Spatial Samples
                + SScrollBox::Slot()
                .Padding(0, 5)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 10, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("SpatialSampleLabel", "Spatial Samples:"))
                        .MinDesiredWidth(130.0f)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(SpatialSampleCountInput, SSpinBox<int32>)
                        .MinValue(1)
                        .MaxValue(32)
                        .Value(1)
                        .MinDesiredWidth(100.0f)
                        .ToolTipText(LOCTEXT("SpatialSampleTooltip", "Number of spatial samples for anti-aliasing"))
                    ]
                ]
                // Temporal Samples
                + SScrollBox::Slot()
                .Padding(0, 5)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 10, 0)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("TemporalSampleLabel", "Temporal Samples:"))
                        .MinDesiredWidth(130.0f)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(TemporalSampleCountInput, SSpinBox<int32>)
                        .MinValue(1)
                        .MaxValue(32)
                        .Value(1)
                        .MinDesiredWidth(100.0f)
                        .ToolTipText(LOCTEXT("TemporalSampleTooltip", "Number of temporal samples for motion blur"))
                    ]
                ]

                // ---- Separator ----
                + SScrollBox::Slot()
                .Padding(0, 8)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("Menu.Separator"))
                    .Padding(FMargin(0, 2))
                ]

                // Keep Exported Level Sequence toggle
                + SScrollBox::Slot()
                .Padding(0, 5)
                [
                    SAssignNew(KeepExportedSequenceCheckBox, SCheckBox)
                    .IsChecked(ECheckBoxState::Unchecked)
                    .ToolTipText(LOCTEXT("KeepSeqTooltip", "When off, the exported Level Sequence is deleted automatically after rendering completes"))
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("KeepSeqLabel", "Keep Exported Level Sequence"))
                    ]
                ]
            ]

            // ----------------------------------------------------------------
            // Buttons
            // ----------------------------------------------------------------
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 10, 0, 0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(5, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CancelButton", "Cancel"))
                    .OnClicked(this, &SLevelSeqExporterWindow::OnCancelClicked)
                    .ToolTipText(LOCTEXT("CancelButtonTooltip", "Close the window without exporting"))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(5, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("RenderButton", "Render"))
                    .OnClicked(this, &SLevelSeqExporterWindow::OnRenderClicked)
                    .IsEnabled(this, &SLevelSeqExporterWindow::IsExportButtonEnabled)
                    .ToolTipText(LOCTEXT("RenderButtonTooltip", "Export Level Sequence then start Movie Render Queue render"))
                ]
            ]
        ]
    ];
}

// ---------------------------------------------------------------------------
// Trajectory list helpers
// ---------------------------------------------------------------------------

TSharedRef<ITableRow> SLevelSeqExporterWindow::GenerateTrajectoryRow(TSharedPtr<FTrajectoryExportItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    typedef STableRow<TSharedPtr<FTrajectoryExportItem>> RowType;
    return SNew(RowType, OwnerTable)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(5.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(SCheckBox)
                .IsChecked(this, &SLevelSeqExporterWindow::IsExportChecked, Item)
                .OnCheckStateChanged(this, &SLevelSeqExporterWindow::OnToggleExport, Item)
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.7f)
            .Padding(FMargin(5.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->Name))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.3f)
            .Padding(FMargin(5.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.2fs"), Item->Duration)))
            ]
        ];
}

void SLevelSeqExporterWindow::OnSelectionChanged(TSharedPtr<FTrajectoryExportItem> NewItem, ESelectInfo::Type SelectInfo)
{
    SelectedItem = NewItem;
    UpdateSummary();
}

void SLevelSeqExporterWindow::UpdateSummary()
{
    if (SelectedItem.IsValid() && SelectedItem->Trajectory.IsValid())
    {
        ACDGTrajectory* Traj = SelectedItem->Trajectory.Get();
        SummaryNameText->SetText(FText::FromString(Traj->TrajectoryName.ToString()));
        SummaryDurationText->SetText(FText::FromString(FString::Printf(TEXT("%.2fs"), Traj->GetTrajectoryDuration())));
        SummaryKeyframeCountText->SetText(FText::AsNumber(Traj->GetKeyframeCount()));
        SummaryPromptText->SetText(FText::FromString(Traj->TextPrompt));
        SummaryPromptText->SetIsReadOnly(false);
        SummaryInfoText->SetText(FText::GetEmpty());
    }
    else
    {
        SummaryNameText->SetText(LOCTEXT("NoSelection", "None"));
        SummaryDurationText->SetText(LOCTEXT("ZeroDuration", "0.0s"));
        SummaryKeyframeCountText->SetText(LOCTEXT("ZeroKeyframes", "0"));
        SummaryPromptText->SetText(FText::GetEmpty());
        SummaryPromptText->SetIsReadOnly(true);
        SummaryInfoText->SetText(LOCTEXT("SelectInstruction", "Select a trajectory to view details."));
    }
}

void SLevelSeqExporterWindow::OnToggleExport(ECheckBoxState NewState, TSharedPtr<FTrajectoryExportItem> Item)
{
    if (Item.IsValid())
    {
        Item->bExport = (NewState == ECheckBoxState::Checked);
    }
}

ECheckBoxState SLevelSeqExporterWindow::IsExportChecked(TSharedPtr<FTrajectoryExportItem> Item) const
{
    return (Item.IsValid() && Item->bExport) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

// ---------------------------------------------------------------------------
// Base Level Sequence validation
// ---------------------------------------------------------------------------

void SLevelSeqExporterWindow::OnBaseLevelSequenceSelected(const FAssetData& AssetData)
{
    BaseLevelSequence = Cast<ULevelSequence>(AssetData.GetAsset());
    ValidateBaseLevelSequence();
}

FString SLevelSeqExporterWindow::GetBaseLevelSequencePath() const
{
    return BaseLevelSequence.IsValid() ? BaseLevelSequence->GetPathName() : FString();
}

bool SLevelSeqExporterWindow::ValidateBaseLevelSequence()
{
    bIsBaseLevelSequenceValid = true;
    BaseLevelSequenceValidationError.Empty();
    BaseShotSequence.Reset();
    BaseShotDurationSeconds = 0.0;

    if (!BaseLevelSequence.IsValid())
    {
        return true;
    }

    UMovieScene* BaseMasterMovieScene = BaseLevelSequence->GetMovieScene();
    if (!BaseMasterMovieScene)
    {
        bIsBaseLevelSequenceValid = false;
        BaseLevelSequenceValidationError = TEXT("Selected base sequence has no MovieScene.");
        return false;
    }

    for (UMovieSceneTrack* Track : BaseMasterMovieScene->GetTracks())
    {
        if (!Track) continue;
        for (UMovieSceneSection* Section : Track->GetAllSections())
        {
            if (Cast<UMovieSceneSubSection>(Section))
            {
                bIsBaseLevelSequenceValid = false;
                BaseLevelSequenceValidationError = TEXT("Base Level Sequence must not contain nested sub sequences.");
                return false;
            }
        }
    }

    const FFrameRate MasterTickResolution = BaseMasterMovieScene->GetTickResolution();
    const double MasterTicksPerSecond = MasterTickResolution.AsDecimal();
    if (MasterTicksPerSecond <= KINDA_SMALL_NUMBER)
    {
        bIsBaseLevelSequenceValid = false;
        BaseLevelSequenceValidationError = TEXT("Base level sequence tick resolution is invalid.");
        return false;
    }

    const TRange<FFrameNumber> MasterPlaybackRange = BaseMasterMovieScene->GetPlaybackRange();
    if (!MasterPlaybackRange.GetLowerBound().IsOpen() && !MasterPlaybackRange.GetUpperBound().IsOpen())
    {
        const int32 DurationInTicks = MasterPlaybackRange.GetUpperBoundValue().Value - MasterPlaybackRange.GetLowerBoundValue().Value;
        BaseShotDurationSeconds = static_cast<double>(DurationInTicks) / MasterTicksPerSecond;
    }

    if (BaseShotDurationSeconds <= KINDA_SMALL_NUMBER)
    {
        bIsBaseLevelSequenceValid = false;
        BaseLevelSequenceValidationError = TEXT("Base level sequence duration must be greater than zero.");
        return false;
    }

    BaseShotSequence = BaseLevelSequence;
    return true;
}

bool SLevelSeqExporterWindow::IsExportButtonEnabled() const
{
    return bIsBaseLevelSequenceValid;
}

FText SLevelSeqExporterWindow::GetBaseLevelSequenceValidationMessage() const
{
    return bIsBaseLevelSequenceValid ? FText::GetEmpty() : FText::FromString(BaseLevelSequenceValidationError);
}

// ---------------------------------------------------------------------------
// Output format combo
// ---------------------------------------------------------------------------

TSharedRef<SWidget> SLevelSeqExporterWindow::MakeOutputFormatWidget(TSharedPtr<ECDGRenderOutputFormat> InItem)
{
    FText FormatText;
    switch (*InItem)
    {
    case ECDGRenderOutputFormat::PNG_Sequence:     FormatText = LOCTEXT("PNG_Sequence",       "PNG Sequence [8bit]");     break;
    case ECDGRenderOutputFormat::EXR_Sequence:     FormatText = LOCTEXT("EXR_Sequence",       "EXR Sequence [16bit]");    break;
    case ECDGRenderOutputFormat::BMP_Sequence:     FormatText = LOCTEXT("BMP_Sequence",       "BMP Sequence [8bit]");     break;
    case ECDGRenderOutputFormat::H264_Video:       FormatText = LOCTEXT("H264_Video",         "H.264 MP4 [8bit]");        break;
    case ECDGRenderOutputFormat::WAV_Audio:        FormatText = LOCTEXT("WAV_Audio",           "WAV Audio");               break;
    case ECDGRenderOutputFormat::CommandLineEncoder: FormatText = LOCTEXT("CommandLineEncoder", "Command Line Encoder");   break;
    case ECDGRenderOutputFormat::FinalCutProXML:   FormatText = LOCTEXT("FinalCutProXML",     "Final Cut Pro XML");       break;
    default: break;
    }
    return SNew(STextBlock).Text(FormatText);
}

FText SLevelSeqExporterWindow::GetOutputFormatText() const
{
    if (SelectedOutputFormat.IsValid())
    {
        switch (*SelectedOutputFormat)
        {
        case ECDGRenderOutputFormat::PNG_Sequence:       return LOCTEXT("PNG_Sequence",       "PNG Sequence [8bit]");
        case ECDGRenderOutputFormat::EXR_Sequence:       return LOCTEXT("EXR_Sequence",       "EXR Sequence [16bit]");
        case ECDGRenderOutputFormat::BMP_Sequence:       return LOCTEXT("BMP_Sequence",       "BMP Sequence [8bit]");
        case ECDGRenderOutputFormat::H264_Video:         return LOCTEXT("H264_Video",         "H.264 MP4 [8bit]");
        case ECDGRenderOutputFormat::WAV_Audio:          return LOCTEXT("WAV_Audio",           "WAV Audio");
        case ECDGRenderOutputFormat::CommandLineEncoder: return LOCTEXT("CommandLineEncoder", "Command Line Encoder");
        case ECDGRenderOutputFormat::FinalCutProXML:     return LOCTEXT("FinalCutProXML",     "Final Cut Pro XML");
        default: break;
        }
    }
    return LOCTEXT("PNG_Sequence", "PNG Sequence [8bit]");
}

void SLevelSeqExporterWindow::OnOutputFormatChanged(TSharedPtr<ECDGRenderOutputFormat> NewFormat, ESelectInfo::Type)
{
    SelectedOutputFormat = NewFormat;
}

bool SLevelSeqExporterWindow::IsFFmpegAvailable() const
{
    TArray<FString> PossiblePaths = {
        FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/FFmpeg/Win64/bin/ffmpeg.exe")),
        FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/FFmpeg/Win64/ffmpeg.exe")),
        FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/ThirdParty/FFmpeg/Win64/bin/ffmpeg.exe"))
    };
    for (const FString& Path : PossiblePaths)
    {
        if (FPaths::FileExists(Path))
        {
            return true;
        }
    }
    return false;
}

bool SLevelSeqExporterWindow::DoesFormatRequireFFmpeg() const
{
    if (!SelectedOutputFormat.IsValid()) return false;
    return *SelectedOutputFormat == ECDGRenderOutputFormat::H264_Video ||
           *SelectedOutputFormat == ECDGRenderOutputFormat::CommandLineEncoder;
}

// ---------------------------------------------------------------------------
// CopyBaseShotNonCameraTracks (unchanged logic)
// ---------------------------------------------------------------------------

bool SLevelSeqExporterWindow::IsCameraBinding(UMovieScene* MovieScene, const FGuid& BindingGuid, const FString& BindingName)
{
    if (!MovieScene) return false;

    if (BindingName.Contains(TEXT("camera"), ESearchCase::IgnoreCase))
    {
        return true;
    }

    if (const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(BindingGuid))
    {
        const UClass* PossessedClass = Possessable->GetPossessedObjectClass();
        if (PossessedClass && (PossessedClass->IsChildOf(ACineCameraActor::StaticClass()) || PossessedClass->IsChildOf(UCineCameraComponent::StaticClass())))
        {
            return true;
        }
    }

    if (const FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(BindingGuid))
    {
        if (const UObject* Template = Spawnable->GetObjectTemplate())
        {
            if (Template->IsA(ACineCameraActor::StaticClass()) || Template->IsA(UCineCameraComponent::StaticClass()))
            {
                return true;
            }
        }
    }

    return false;
}

void SLevelSeqExporterWindow::CopyBaseShotNonCameraTracks(UMovieScene* TargetShotMovieScene) const
{
    if (!TargetShotMovieScene || !BaseShotSequence.IsValid()) return;

    UMovieScene* SourceShotMovieScene = BaseShotSequence->GetMovieScene();
    if (!SourceShotMovieScene) return;

    ULevelSequence* TargetShotSequence = TargetShotMovieScene->GetTypedOuter<ULevelSequence>();
    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    for (UMovieSceneTrack* SourceTrack : SourceShotMovieScene->GetTracks())
    {
        if (!SourceTrack || SourceTrack->IsA(UMovieSceneCameraCutTrack::StaticClass())) continue;

        if (UMovieSceneTrack* DuplicatedTrack = DuplicateObject<UMovieSceneTrack>(SourceTrack, TargetShotMovieScene))
        {
            TargetShotMovieScene->AddGivenTrack(DuplicatedTrack);
        }
    }

    auto GetBindingName = [SourceShotMovieScene](const FGuid& BindingGuid) -> FString
    {
        if (const FMovieScenePossessable* Possessable = SourceShotMovieScene->FindPossessable(BindingGuid))
            return Possessable->GetName();
        if (const FMovieSceneSpawnable* Spawnable = SourceShotMovieScene->FindSpawnable(BindingGuid))
            return Spawnable->GetName();
        return FString();
    };

    auto ResolveSourceBindingObjects = [this, EditorWorld, SourceShotMovieScene](const FGuid& SourceBindingGuid, TArray<UObject*, TInlineAllocator<1>>& OutObjects)
    {
        OutObjects.Reset();
        if (!BaseShotSequence.IsValid()) return;

        BaseShotSequence->LocateBoundObjects(
            SourceBindingGuid,
            UE::UniversalObjectLocator::FResolveParams(EditorWorld),
            nullptr,
            OutObjects);

        const FMovieScenePossessable* SourcePossessable = SourceShotMovieScene->FindPossessable(SourceBindingGuid);
        if (OutObjects.Num() > 0 || !SourcePossessable || !SourcePossessable->GetParent().IsValid()) return;

        TArray<UObject*, TInlineAllocator<1>> ResolvedParentObjects;
        BaseShotSequence->LocateBoundObjects(
            SourcePossessable->GetParent(),
            UE::UniversalObjectLocator::FResolveParams(EditorWorld),
            nullptr,
            ResolvedParentObjects);

        for (UObject* ParentObject : ResolvedParentObjects)
        {
            if (!ParentObject) continue;
            TArray<UObject*, TInlineAllocator<1>> ObjectsResolvedWithParentContext;
            BaseShotSequence->LocateBoundObjects(
                SourceBindingGuid,
                UE::UniversalObjectLocator::FResolveParams(ParentObject),
                nullptr,
                ObjectsResolvedWithParentContext);
            for (UObject* ResolvedObject : ObjectsResolvedWithParentContext)
            {
                if (ResolvedObject) OutObjects.AddUnique(ResolvedObject);
            }
        }
    };

    TMap<FGuid, FGuid> BindingGuidMap;
    const UMovieScene* ConstSourceShotMovieScene = SourceShotMovieScene;

    for (const FMovieSceneBinding& SourceBinding : ConstSourceShotMovieScene->GetBindings())
    {
        const FGuid SourceBindingGuid = SourceBinding.GetObjectGuid();
        if (IsCameraBinding(SourceShotMovieScene, SourceBindingGuid, GetBindingName(SourceBindingGuid))) continue;

        FGuid TargetBindingGuid;
        if (const FMovieScenePossessable* SourcePossessable = SourceShotMovieScene->FindPossessable(SourceBindingGuid))
        {
            TargetBindingGuid = TargetShotMovieScene->AddPossessable(
                SourcePossessable->GetName(),
                const_cast<UClass*>(SourcePossessable->GetPossessedObjectClass()));

            if (TargetShotSequence)
            {
                TArray<UObject*, TInlineAllocator<1>> ResolvedSourceObjects;
                ResolveSourceBindingObjects(SourceBindingGuid, ResolvedSourceObjects);
                for (UObject* ResolvedObject : ResolvedSourceObjects)
                {
                    if (!ResolvedObject) continue;
                    UObject* BindingContext = EditorWorld;
                    if (SourcePossessable->GetParent().IsValid())
                    {
                        if (const FGuid* TargetParentGuid = BindingGuidMap.Find(SourcePossessable->GetParent()))
                        {
                            TArray<UObject*, TInlineAllocator<1>> TargetParentObjects;
                            TargetShotSequence->LocateBoundObjects(*TargetParentGuid, UE::UniversalObjectLocator::FResolveParams(EditorWorld), nullptr, TargetParentObjects);
                            if (TargetParentObjects.Num() > 0 && TargetParentObjects[0]) BindingContext = TargetParentObjects[0];
                        }
                    }
                    if (UActorComponent* ActorComponent = Cast<UActorComponent>(ResolvedObject))
                    {
                        if (AActor* OwnerActor = ActorComponent->GetOwner()) BindingContext = OwnerActor;
                    }
                    TargetShotSequence->BindPossessableObject(TargetBindingGuid, *ResolvedObject, BindingContext);
                }
            }
        }
        else if (const FMovieSceneSpawnable* SourceSpawnable = SourceShotMovieScene->FindSpawnable(SourceBindingGuid))
        {
            const UObject* SpawnableTemplate = SourceSpawnable->GetObjectTemplate();
            if (!SpawnableTemplate) continue;
            TargetBindingGuid = TargetShotMovieScene->AddSpawnable(SourceSpawnable->GetName(), *const_cast<UObject*>(SpawnableTemplate));
        }
        else
        {
            continue;
        }

        BindingGuidMap.Add(SourceBindingGuid, TargetBindingGuid);
        for (UMovieSceneTrack* SourceTrack : SourceBinding.GetTracks())
        {
            if (!SourceTrack || SourceTrack->IsA(UMovieSceneCameraCutTrack::StaticClass())) continue;
            if (UMovieSceneTrack* DuplicatedTrack = DuplicateObject<UMovieSceneTrack>(SourceTrack, TargetShotMovieScene))
            {
                TargetShotMovieScene->AddGivenTrack(DuplicatedTrack, TargetBindingGuid);
            }
        }
    }

    for (const FMovieSceneBinding& SourceBinding : ConstSourceShotMovieScene->GetBindings())
    {
        const FGuid SourceBindingGuid = SourceBinding.GetObjectGuid();
        if (IsCameraBinding(SourceShotMovieScene, SourceBindingGuid, GetBindingName(SourceBindingGuid))) continue;

        const FMovieScenePossessable* SourcePossessable = SourceShotMovieScene->FindPossessable(SourceBindingGuid);
        if (!SourcePossessable) continue;

        const FGuid* TargetBindingGuid = BindingGuidMap.Find(SourceBindingGuid);
        const FGuid* TargetParentGuid  = BindingGuidMap.Find(SourcePossessable->GetParent());
        if (!TargetBindingGuid || !TargetParentGuid) continue;

        if (FMovieScenePossessable* TargetPossessable = TargetShotMovieScene->FindPossessable(*TargetBindingGuid))
        {
            TargetPossessable->SetParent(*TargetParentGuid, TargetShotMovieScene);
        }
    }

    // Final rebinding pass after parent hierarchy is established
    if (TargetShotSequence)
    {
        for (const FMovieSceneBinding& SourceBinding : ConstSourceShotMovieScene->GetBindings())
        {
            const FGuid SourceBindingGuid = SourceBinding.GetObjectGuid();
            if (IsCameraBinding(SourceShotMovieScene, SourceBindingGuid, GetBindingName(SourceBindingGuid))) continue;

            const FMovieScenePossessable* SourcePossessable = SourceShotMovieScene->FindPossessable(SourceBindingGuid);
            const FGuid* TargetBindingGuid = BindingGuidMap.Find(SourceBindingGuid);
            if (!SourcePossessable || !TargetBindingGuid) continue;

            TargetShotSequence->UnbindPossessableObjects(*TargetBindingGuid);
            TArray<UObject*, TInlineAllocator<1>> ResolvedSourceObjects;
            ResolveSourceBindingObjects(SourceBindingGuid, ResolvedSourceObjects);
            for (UObject* ResolvedObject : ResolvedSourceObjects)
            {
                if (!ResolvedObject) continue;
                UObject* BindingContext = EditorWorld;
                if (SourcePossessable->GetParent().IsValid())
                {
                    if (const FGuid* TargetParentGuid = BindingGuidMap.Find(SourcePossessable->GetParent()))
                    {
                        TArray<UObject*, TInlineAllocator<1>> TargetParentObjects;
                        TargetShotSequence->LocateBoundObjects(*TargetParentGuid, UE::UniversalObjectLocator::FResolveParams(EditorWorld), nullptr, TargetParentObjects);
                        if (TargetParentObjects.Num() > 0 && TargetParentObjects[0]) BindingContext = TargetParentObjects[0];
                    }
                }
                if (UActorComponent* ActorComponent = Cast<UActorComponent>(ResolvedObject))
                {
                    if (AActor* OwnerActor = ActorComponent->GetOwner()) BindingContext = OwnerActor;
                }
                TargetShotSequence->BindPossessableObject(*TargetBindingGuid, *ResolvedObject, BindingContext);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// PerformExport - core export logic, returns MasterSequence or nullptr
// ---------------------------------------------------------------------------

ULevelSequence* SLevelSeqExporterWindow::PerformExport()
{
    if (!ValidateBaseLevelSequence()) return nullptr;

    TArray<ACDGTrajectory*> TrajectoriesToExport;
    for (const auto& Item : TrajectoryItems)
    {
        if (Item->bExport && Item->Trajectory.IsValid())
        {
            TrajectoriesToExport.Add(Item->Trajectory.Get());
        }
    }

    if (TrajectoriesToExport.Num() == 0) return nullptr;

    const int32 FPS = FPSInput->GetValue();
    const bool bClearSequence = ClearSequenceCheckBox->IsChecked();
    const FFrameRate FrameRate(FPS, 1);
    const double TickResolution = 24000.0;

    ULevelSequence* MasterSequence = nullptr;
    if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
    {
        if (UCDGLevelSeqSubsystem* LevelSeqSubsystem = World->GetSubsystem<UCDGLevelSeqSubsystem>())
        {
            LevelSeqSubsystem->InitLevelSequence();
            MasterSequence = LevelSeqSubsystem->GetActiveLevelSequence();
        }
    }

    if (!MasterSequence) return nullptr;

    UMovieScene* MasterMovieScene = MasterSequence->GetMovieScene();
    MasterMovieScene->SetDisplayRate(FrameRate);
    MasterMovieScene->SetTickResolutionDirectly(FFrameRate(TickResolution, 1));

    if (bClearSequence)
    {
        for (UMovieSceneTrack* Track : MasterMovieScene->GetTracks())
        {
            MasterMovieScene->RemoveTrack(*Track);
        }
        for (int32 i = MasterMovieScene->GetSpawnableCount() - 1; i >= 0; --i)
        {
            MasterMovieScene->RemoveSpawnable(MasterMovieScene->GetSpawnable(i).GetGuid());
        }
        for (int32 i = MasterMovieScene->GetPossessableCount() - 1; i >= 0; --i)
        {
            MasterMovieScene->RemovePossessable(MasterMovieScene->GetPossessable(i).GetGuid());
        }
    }

    UMovieSceneCinematicShotTrack* ShotTrack = Cast<UMovieSceneCinematicShotTrack>(MasterMovieScene->FindTrack(UMovieSceneCinematicShotTrack::StaticClass()));
    if (!ShotTrack)
    {
        ShotTrack = MasterMovieScene->AddTrack<UMovieSceneCinematicShotTrack>();
    }

    FFrameNumber StartFrame = 0;
    if (!bClearSequence)
    {
        TRange<FFrameNumber> PlaybackRange = MasterMovieScene->GetPlaybackRange();
        if (!PlaybackRange.GetUpperBound().IsOpen())
        {
            StartFrame = PlaybackRange.GetUpperBound().GetValue();
        }
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    FString MasterPackagePath = FPackageName::GetLongPackagePath(MasterSequence->GetOutermost()->GetName());

    for (ACDGTrajectory* Trajectory : TrajectoriesToExport)
    {
        const double TrajectoryDuration = Trajectory->GetTrajectoryDuration();
        const bool bUseBaseShotTiming = BaseShotSequence.IsValid() && BaseShotDurationSeconds > KINDA_SMALL_NUMBER;
        const double OutputShotDuration = bUseBaseShotTiming ? BaseShotDurationSeconds : TrajectoryDuration;
        const double TrajectoryTimeScale = (TrajectoryDuration > KINDA_SMALL_NUMBER) ? (OutputShotDuration / TrajectoryDuration) : 1.0;
        const int32 DurationInTicks = FMath::Max(1, FMath::RoundToInt(OutputShotDuration * TickResolution));

        const FString MasterSequenceName = FPackageName::GetShortName(MasterSequence->GetOutermost()->GetName());
        FString ShotName = FString::Printf(TEXT("%s_Shot_%s"), *MasterSequenceName, *Trajectory->TrajectoryName.ToString());
        FString PackageName = MasterPackagePath / ShotName;

        ULevelSequence* ShotSequence = nullptr;
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(PackageName + TEXT(".") + ShotName));
        if (AssetData.IsValid())
        {
            ShotSequence = Cast<ULevelSequence>(AssetData.GetAsset());
        }

        if (!ShotSequence)
        {
            UFactory* Factory = nullptr;
            for (TObjectIterator<UClass> It; It; ++It)
            {
                UClass* CurrentClass = *It;
                if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !CurrentClass->HasAnyClassFlags(CLASS_Abstract))
                {
                    UFactory* TestFactory = Cast<UFactory>(CurrentClass->GetDefaultObject());
                    if (TestFactory && TestFactory->CanCreateNew() && TestFactory->SupportedClass == ULevelSequence::StaticClass())
                    {
                        Factory = TestFactory;
                        break;
                    }
                }
            }
            if (Factory)
            {
                ShotSequence = Cast<ULevelSequence>(AssetTools.CreateAsset(ShotName, MasterPackagePath, ULevelSequence::StaticClass(), Factory));
            }
        }

        if (!ShotSequence)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create shot sequence asset: %s"), *ShotName);
            continue;
        }

        UMovieScene* ShotMovieScene = ShotSequence->GetMovieScene();
        if (!ShotMovieScene)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to get movie scene for shot sequence: %s"), *ShotName);
            continue;
        }

        ShotSequence->Modify();
        ShotMovieScene->Modify();
        ShotMovieScene->SetDisplayRate(FrameRate);
        ShotMovieScene->SetTickResolutionDirectly(FFrameRate(TickResolution, 1));

        {
            TArray<UMovieSceneTrack*> ExistingTracks = ShotMovieScene->GetTracks();
            for (UMovieSceneTrack* Track : ExistingTracks) ShotMovieScene->RemoveTrack(*Track);
            for (int32 i = ShotMovieScene->GetSpawnableCount() - 1; i >= 0; --i)
                ShotMovieScene->RemoveSpawnable(ShotMovieScene->GetSpawnable(i).GetGuid());
            for (int32 i = ShotMovieScene->GetPossessableCount() - 1; i >= 0; --i)
                ShotMovieScene->RemovePossessable(ShotMovieScene->GetPossessable(i).GetGuid());
        }

        CopyBaseShotNonCameraTracks(ShotMovieScene);

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) { UE_LOG(LogTemp, Error, TEXT("No valid world context.")); continue; }

        FString CameraName = FString::Printf(TEXT("Cam_%s"), *Trajectory->TrajectoryName.ToString());
        ACineCameraActor* CameraActor = nullptr;

        for (TActorIterator<ACineCameraActor> It(World); It; ++It)
        {
            if (It->GetActorLabel() == CameraName)
            {
                World->EditorDestroyActor(*It, true);
                break;
            }
        }

        FActorSpawnParameters SpawnParams;
        CameraActor = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (CameraActor) CameraActor->SetActorLabel(CameraName);
        if (!CameraActor) { UE_LOG(LogTemp, Error, TEXT("Failed to spawn camera actor: %s"), *CameraName); continue; }

        FGuid CameraGuid = ShotMovieScene->AddPossessable(CameraActor->GetActorLabel(), CameraActor->GetClass());
        ShotSequence->BindPossessableObject(CameraGuid, *CameraActor, World);

        UMovieSceneCameraCutTrack* CameraCutTrack = ShotMovieScene->AddTrack<UMovieSceneCameraCutTrack>();
        if (CameraCutTrack)
        {
            UMovieSceneCameraCutSection* CutSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
            if (CutSection)
            {
                CutSection->SetRange(TRange<FFrameNumber>(0, DurationInTicks));
                CutSection->SetCameraGuid(CameraGuid);
                CameraCutTrack->AddSection(*CutSection);
            }
        }

        UMovieScene3DTransformTrack* TransformTrack = ShotMovieScene->AddTrack<UMovieScene3DTransformTrack>(CameraGuid);
        if (TransformTrack)
        {
            UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
            if (TransformSection)
            {
                TransformTrack->AddSection(*TransformSection);
                TransformSection->SetRange(TRange<FFrameNumber>(0, DurationInTicks));

                FMovieSceneChannelProxy& ChannelProxy = TransformSection->GetChannelProxy();
                TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = ChannelProxy.GetChannels<FMovieSceneDoubleChannel>();

                if (DoubleChannels.Num() >= 6)
                {
                    TArray<ACDGKeyframe*> TrajectoryKeyframes = Trajectory->GetSortedKeyframes();
                    double CurrentTimeSeconds = 0.0;

                    auto ConvertInterpMode = [](ECDGInterpolationMode Mode, ERichCurveInterpMode& OutInterpMode, ERichCurveTangentMode& OutTangentMode)
                    {
                        switch (Mode)
                        {
                        case ECDGInterpolationMode::Linear:
                            OutInterpMode = RCIM_Linear; OutTangentMode = RCTM_Auto; break;
                        case ECDGInterpolationMode::Constant:
                            OutInterpMode = RCIM_Constant; OutTangentMode = RCTM_Auto; break;
                        case ECDGInterpolationMode::Cubic:
                        case ECDGInterpolationMode::CubicClamped:
                            OutInterpMode = RCIM_Cubic; OutTangentMode = RCTM_Auto; break;
                        case ECDGInterpolationMode::CustomTangent:
                            OutInterpMode = RCIM_Cubic; OutTangentMode = RCTM_User; break;
                        default:
                            OutInterpMode = RCIM_Cubic; OutTangentMode = RCTM_Auto; break;
                        }
                    };

                    auto AddKeyToChannel = [&](FMovieSceneDoubleChannel* Channel, FFrameNumber Time, double Value, ECDGInterpolationMode Mode)
                    {
                        if (!Channel) return;
                        ERichCurveInterpMode InterpMode; ERichCurveTangentMode TangentMode;
                        ConvertInterpMode(Mode, InterpMode, TangentMode);
                        if (InterpMode == RCIM_Constant)       Channel->AddConstantKey(Time, Value);
                        else if (InterpMode == RCIM_Linear)    Channel->AddLinearKey(Time, Value);
                        else                                    Channel->AddCubicKey(Time, Value, TangentMode);
                    };

                    for (int32 k = 0; k < TrajectoryKeyframes.Num(); ++k)
                    {
                        ACDGKeyframe* Keyframe = TrajectoryKeyframes[k];
                        if (!Keyframe) continue;

                        if (k > 0) CurrentTimeSeconds += (Keyframe->TimeToCurrentFrame * TrajectoryTimeScale);
                        FFrameNumber KeyTime = FFrameNumber(static_cast<int32>(CurrentTimeSeconds * TickResolution));

                        FTransform KeyTransform = Keyframe->GetKeyframeTransform();
                        FVector Loc = KeyTransform.GetLocation();
                        FRotator Rot = KeyTransform.GetRotation().Rotator();
                        const bool bHasStay = Keyframe->TimeAtCurrentFrame > KINDA_SMALL_NUMBER;

                        ECDGInterpolationMode PosMode = bHasStay ? ECDGInterpolationMode::Constant : Keyframe->InterpolationSettings.PositionInterpMode;
                        ECDGInterpolationMode RotMode = bHasStay ? ECDGInterpolationMode::Constant : Keyframe->InterpolationSettings.RotationInterpMode;

                        AddKeyToChannel(DoubleChannels[0], KeyTime, Loc.X, PosMode);
                        AddKeyToChannel(DoubleChannels[1], KeyTime, Loc.Y, PosMode);
                        AddKeyToChannel(DoubleChannels[2], KeyTime, Loc.Z, PosMode);
                        AddKeyToChannel(DoubleChannels[3], KeyTime, Rot.Roll, RotMode);
                        AddKeyToChannel(DoubleChannels[4], KeyTime, Rot.Pitch, RotMode);
                        AddKeyToChannel(DoubleChannels[5], KeyTime, Rot.Yaw, RotMode);

                        if (bHasStay)
                        {
                            CurrentTimeSeconds += (Keyframe->TimeAtCurrentFrame * TrajectoryTimeScale);
                            FFrameNumber EndKeyTime = FFrameNumber(static_cast<int32>(CurrentTimeSeconds * TickResolution));
                            PosMode = Keyframe->InterpolationSettings.PositionInterpMode;
                            RotMode = Keyframe->InterpolationSettings.RotationInterpMode;
                            AddKeyToChannel(DoubleChannels[0], EndKeyTime, Loc.X, PosMode);
                            AddKeyToChannel(DoubleChannels[1], EndKeyTime, Loc.Y, PosMode);
                            AddKeyToChannel(DoubleChannels[2], EndKeyTime, Loc.Z, PosMode);
                            AddKeyToChannel(DoubleChannels[3], EndKeyTime, Rot.Roll, RotMode);
                            AddKeyToChannel(DoubleChannels[4], EndKeyTime, Rot.Pitch, RotMode);
                            AddKeyToChannel(DoubleChannels[5], EndKeyTime, Rot.Yaw, RotMode);
                        }
                    }
                }
            }
        }

        UCineCameraComponent* CameraComponent = CameraActor->GetCineCameraComponent();
        if (CameraComponent)
        {
            FGuid ComponentGuid = ShotMovieScene->AddPossessable(CameraComponent->GetName(), CameraComponent->GetClass());
            FMovieScenePossessable* ChildPossessable = ShotMovieScene->FindPossessable(ComponentGuid);
            if (ChildPossessable) ChildPossessable->SetParent(CameraGuid, ShotMovieScene);
            ShotSequence->BindPossessableObject(ComponentGuid, *CameraComponent, CameraActor);

            TArray<ACDGKeyframe*> TrajectoryKeyframes = Trajectory->GetSortedKeyframes();
            const bool bAnyFocusOverride = TrajectoryKeyframes.ContainsByPredicate([](const ACDGKeyframe* Keyframe)
            {
                return Keyframe && (Keyframe->LensSettings.AutofocusTargetActor.Get() != nullptr || Keyframe->LensSettings.bUseManualFocusDistance);
            });
            CameraComponent->FocusSettings.FocusMethod = bAnyFocusOverride ? ECameraFocusMethod::Manual : ECameraFocusMethod::Disable;

            auto AddLensKeyWithStay = [&](FMovieSceneFloatChannel& Channel, double& CurTime, const ACDGKeyframe* Keyframe, float Value)
            {
                const FFrameNumber KeyTime = FFrameNumber(static_cast<int32>(CurTime * TickResolution));
                const bool bHasStay = Keyframe->TimeAtCurrentFrame > KINDA_SMALL_NUMBER;
                if (bHasStay)
                {
                    Channel.AddConstantKey(KeyTime, Value);
                    CurTime += (Keyframe->TimeAtCurrentFrame * TrajectoryTimeScale);
                    Channel.AddLinearKey(FFrameNumber(static_cast<int32>(CurTime * TickResolution)), Value);
                }
                else
                {
                    Channel.AddLinearKey(KeyTime, Value);
                }
            };

            // Focal length
            if (UMovieSceneFloatTrack* FocalLengthTrack = ShotMovieScene->AddTrack<UMovieSceneFloatTrack>(ComponentGuid))
            {
                FocalLengthTrack->SetPropertyNameAndPath(GET_MEMBER_NAME_CHECKED(UCineCameraComponent, CurrentFocalLength), "CurrentFocalLength");
                UMovieSceneFloatSection* FocalSection = Cast<UMovieSceneFloatSection>(FocalLengthTrack->CreateNewSection());
                FocalLengthTrack->AddSection(*FocalSection);
                FocalSection->SetRange(TRange<FFrameNumber>(0, DurationInTicks));
                FMovieSceneFloatChannel& FocalChannel = FocalSection->GetChannel();
                double CurTime = 0.0;
                for (int32 k = 0; k < TrajectoryKeyframes.Num(); ++k)
                {
                    const ACDGKeyframe* Keyframe = TrajectoryKeyframes[k];
                    if (!Keyframe) continue;
                    if (k > 0) CurTime += (Keyframe->TimeToCurrentFrame * TrajectoryTimeScale);
                    AddLensKeyWithStay(FocalChannel, CurTime, Keyframe, Keyframe->LensSettings.FocalLength);
                }
            }

            // Aperture
            if (UMovieSceneFloatTrack* ApertureTrack = ShotMovieScene->AddTrack<UMovieSceneFloatTrack>(ComponentGuid))
            {
                ApertureTrack->SetPropertyNameAndPath(GET_MEMBER_NAME_CHECKED(UCineCameraComponent, CurrentAperture), "CurrentAperture");
                UMovieSceneFloatSection* ApertureSection = Cast<UMovieSceneFloatSection>(ApertureTrack->CreateNewSection());
                ApertureTrack->AddSection(*ApertureSection);
                ApertureSection->SetRange(TRange<FFrameNumber>(0, DurationInTicks));
                FMovieSceneFloatChannel& ApertureChannel = ApertureSection->GetChannel();
                double CurTime = 0.0;
                for (int32 k = 0; k < TrajectoryKeyframes.Num(); ++k)
                {
                    const ACDGKeyframe* Keyframe = TrajectoryKeyframes[k];
                    if (!Keyframe) continue;
                    if (k > 0) CurTime += (Keyframe->TimeToCurrentFrame * TrajectoryTimeScale);
                    AddLensKeyWithStay(ApertureChannel, CurTime, Keyframe, Keyframe->LensSettings.Aperture);
                }
            }

            // Manual focus distance
            if (UMovieSceneFloatTrack* ManualFocusTrack = ShotMovieScene->AddTrack<UMovieSceneFloatTrack>(ComponentGuid))
            {
                ManualFocusTrack->SetPropertyNameAndPath(TEXT("FocusSettings.ManualFocusDistance"), TEXT("FocusSettings.ManualFocusDistance"));
                UMovieSceneFloatSection* ManualFocusSection = Cast<UMovieSceneFloatSection>(ManualFocusTrack->CreateNewSection());
                ManualFocusTrack->AddSection(*ManualFocusSection);
                ManualFocusSection->SetRange(TRange<FFrameNumber>(0, DurationInTicks));
                FMovieSceneFloatChannel& ManualFocusChannel = ManualFocusSection->GetChannel();
                double CurTime = 0.0;
                for (int32 k = 0; k < TrajectoryKeyframes.Num(); ++k)
                {
                    const ACDGKeyframe* Keyframe = TrajectoryKeyframes[k];
                    if (!Keyframe) continue;
                    if (k > 0) CurTime += (Keyframe->TimeToCurrentFrame * TrajectoryTimeScale);

                    const FVector CameraLocation = Keyframe->GetKeyframeTransform().GetLocation();
                    float FocusDistance = 0.0f;
                    if (const AActor* AutofocusTargetActor = Keyframe->LensSettings.AutofocusTargetActor.Get())
                    {
                        FVector FocusTargetLocation = AutofocusTargetActor->GetActorLocation();
                        TArray<UCDGCharacterAnchor*> AnchorComponents;
                        AutofocusTargetActor->GetComponents<UCDGCharacterAnchor>(AnchorComponents);
                        for (const UCDGCharacterAnchor* AnchorComponent : AnchorComponents)
                        {
                            if (AnchorComponent && AnchorComponent->Type == Keyframe->LensSettings.AutofocusTargetAnchorType)
                            {
                                FocusTargetLocation = AnchorComponent->GetComponentLocation();
                                break;
                            }
                        }
                        FocusDistance = FVector::Distance(CameraLocation, FocusTargetLocation);
                    }
                    else if (Keyframe->LensSettings.bUseManualFocusDistance)
                    {
                        FocusDistance = Keyframe->LensSettings.FocusDistance;
                    }
                    AddLensKeyWithStay(ManualFocusChannel, CurTime, Keyframe, FocusDistance);
                }
            }
        }

        ShotMovieScene->SetPlaybackRange(TRange<FFrameNumber>(0, DurationInTicks));
        ShotSequence->MarkPackageDirty();

        if (ShotTrack)
        {
            UMovieSceneSubSection* ShotSection = ShotTrack->AddSequence(ShotSequence, StartFrame, DurationInTicks);
            if (ShotSection) ShotSection->Parameters.TimeScale = 1.0f;
        }

        StartFrame += FFrameNumber(DurationInTicks);
    }

    MasterMovieScene->SetPlaybackRange(TRange<FFrameNumber>(0, StartFrame));
    MasterSequence->MarkPackageDirty();

    return MasterSequence;
}

// ---------------------------------------------------------------------------
// Config save / load
// ---------------------------------------------------------------------------

void SLevelSeqExporterWindow::OnLoadConfigChanged(const FAssetData& AssetData)
{
    LoadedConfig = Cast<ULevelSeqExportConfig>(AssetData.GetAsset());
    if (LoadedConfig.IsValid())
    {
        ApplyConfigToUI(LoadedConfig.Get());
    }
}

FString SLevelSeqExporterWindow::GetLoadedConfigPath() const
{
    return LoadedConfig.IsValid() ? LoadedConfig->GetPathName() : FString();
}

FReply SLevelSeqExporterWindow::OnSaveConfigClicked()
{
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    if (LoadedConfig.IsValid())
    {
        // Update the already-loaded config in-place and write it to disk
        PopulateConfigFromUI(LoadedConfig.Get());
        LoadedConfig->MarkPackageDirty();

        UPackage* Package = LoadedConfig->GetOutermost();
        FString PackageFilename = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        UPackage::SavePackage(Package, LoadedConfig.Get(), *PackageFilename, SaveArgs);

        FNotificationInfo Info(LOCTEXT("ConfigSaved", "Export config saved"));
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info);
    }
    else
    {
        // No config loaded — create a new asset via the standard content browser dialog
        ULevelSeqExportConfigFactory* Factory = NewObject<ULevelSeqExportConfigFactory>();
        UObject* NewAssetObj = AssetTools.CreateAssetWithDialog(
            TEXT("LevelSeqExportConfig"), TEXT("/Game"),
            ULevelSeqExportConfig::StaticClass(), Factory);

        if (ULevelSeqExportConfig* NewConfig = Cast<ULevelSeqExportConfig>(NewAssetObj))
        {
            PopulateConfigFromUI(NewConfig);
            NewConfig->MarkPackageDirty();

            UPackage* Package = NewConfig->GetOutermost();
            FString PackageFilename = FPackageName::LongPackageNameToFilename(
                Package->GetName(), FPackageName::GetAssetPackageExtension());

            FSavePackageArgs SaveArgs;
            SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
            UPackage::SavePackage(Package, NewConfig, *PackageFilename, SaveArgs);

            LoadedConfig = NewConfig;

            FNotificationInfo Info(LOCTEXT("ConfigCreated", "Export config created and saved"));
            Info.ExpireDuration = 3.0f;
            FSlateNotificationManager::Get().AddNotification(Info);
        }
    }

    return FReply::Handled();
}

void SLevelSeqExporterWindow::ApplyConfigToUI(const ULevelSeqExportConfig* Config)
{
    if (!Config) return;

    FPSInput->SetValue(Config->FPS);
    ClearSequenceCheckBox->SetIsChecked(Config->bClearLevelSequence ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

    OutputDirTextBox->SetText(FText::FromString(Config->OutputDirectory));
    ResolutionWidthInput->SetValue(Config->ResolutionWidth);
    ResolutionHeightInput->SetValue(Config->ResolutionHeight);

    for (const TSharedPtr<ECDGRenderOutputFormat>& Option : OutputFormatOptions)
    {
        if (Option.IsValid() && *Option == Config->ExportFormat)
        {
            SelectedOutputFormat = Option;
            OutputFormatComboBox->SetSelectedItem(Option);
            break;
        }
    }

    ExportIndexJSONCheckBox->SetIsChecked(Config->bExportIndexJSON ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    OverwriteExistingCheckBox->SetIsChecked(Config->bOverwriteExisting ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

    SpatialSampleCountInput->SetValue(Config->SpatialSampleCount);
    TemporalSampleCountInput->SetValue(Config->TemporalSampleCount);

    KeepExportedSequenceCheckBox->SetIsChecked(Config->bKeepExportedLevelSequence ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SLevelSeqExporterWindow::PopulateConfigFromUI(ULevelSeqExportConfig* Config) const
{
    if (!Config) return;

    Config->Modify();
    Config->FPS               = FPSInput->GetValue();
    Config->bClearLevelSequence = ClearSequenceCheckBox->IsChecked();

    Config->OutputDirectory   = OutputDirTextBox->GetText().ToString();
    Config->ResolutionWidth   = ResolutionWidthInput->GetValue();
    Config->ResolutionHeight  = ResolutionHeightInput->GetValue();
    Config->ExportFormat      = SelectedOutputFormat.IsValid() ? *SelectedOutputFormat : ECDGRenderOutputFormat::PNG_Sequence;
    Config->bExportIndexJSON  = ExportIndexJSONCheckBox->IsChecked();
    Config->bOverwriteExisting = OverwriteExistingCheckBox->IsChecked();

    Config->SpatialSampleCount  = SpatialSampleCountInput->GetValue();
    Config->TemporalSampleCount = TemporalSampleCountInput->GetValue();

    Config->bKeepExportedLevelSequence = KeepExportedSequenceCheckBox->IsChecked();
}

// ---------------------------------------------------------------------------
// Button handlers
// ---------------------------------------------------------------------------

FReply SLevelSeqExporterWindow::OnCancelClicked()
{
    TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
    if (Window.IsValid())
    {
        Window->RequestDestroyWindow();
    }
    return FReply::Handled();
}

FReply SLevelSeqExporterWindow::OnRenderClicked()
{
    // 1. Export level sequence
    ULevelSequence* MasterSequence = PerformExport();
    if (!MasterSequence)
    {
        FNotificationInfo Info(LOCTEXT("RenderExportFailed", "Export failed - cannot start render"));
        Info.ExpireDuration = 5.0f;
        Info.bUseSuccessFailIcons = true;
        FSlateNotificationManager::Get().AddNotification(Info);
        return FReply::Handled();
    }

    // 2. Build render config (FPS is shared with export)
    FTrajectoryRenderConfig Config;
    Config.DestinationRootDir     = OutputDirTextBox->GetText().ToString();
    Config.OutputResolutionOverride = FIntPoint(ResolutionWidthInput->GetValue(), ResolutionHeightInput->GetValue());
    Config.OutputFramerateOverride  = FPSInput->GetValue();
    Config.ExportFormat             = SelectedOutputFormat.IsValid() ? *SelectedOutputFormat : ECDGRenderOutputFormat::PNG_Sequence;
    Config.bExportIndexJSON         = ExportIndexJSONCheckBox->IsChecked();
    Config.bOverwriteExistingOutput = OverwriteExistingCheckBox->IsChecked();
    Config.SpatialSampleCount       = SpatialSampleCountInput->GetValue();
    Config.TemporalSampleCount      = TemporalSampleCountInput->GetValue();

    // 3. Gather trajectories
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("No valid world context found for render"));
        return FReply::Handled();
    }

    TArray<ACDGTrajectory*> Trajectories;
    for (TActorIterator<ACDGTrajectory> It(World); It; ++It)
    {
        Trajectories.Add(*It);
    }

    // 4. Determine whether to delete sequence after render
    const bool bKeepSequence = KeepExportedSequenceCheckBox->IsChecked();
    TFunction<void(bool)> OnCompleted = nullptr;
    if (!bKeepSequence)
    {
        TWeakObjectPtr<UCDGLevelSeqSubsystem> WeakSubsystem = World->GetSubsystem<UCDGLevelSeqSubsystem>();
        OnCompleted = [WeakSubsystem](bool /*bSuccess*/)
        {
            if (WeakSubsystem.IsValid())
            {
                WeakSubsystem->DeleteLevelSequence();
                UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGLevelSeqExporter: Deleted exported level sequence after render"));
            }
        };
    }

    // 5. Start render
    bool bSuccess = CDGMRQInterface::RenderTrajectoriesWithSequence(MasterSequence, Trajectories, Config, MoveTemp(OnCompleted));

    FNotificationInfo Info(bSuccess
        ? LOCTEXT("RenderStarted", "Movie Render Queue rendering started")
        : LOCTEXT("RenderFailed", "Failed to start rendering"));
    Info.ExpireDuration = 5.0f;
    Info.bUseLargeFont  = false;
    Info.bUseSuccessFailIcons = true;
    FSlateNotificationManager::Get().AddNotification(Info);

    if (bSuccess)
    {
        OnCancelClicked();
    }

    return FReply::Handled();
}

FReply SLevelSeqExporterWindow::OnBrowseOutputDirClicked()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform) return FReply::Handled();

    TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
    void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
        ? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
        : nullptr;

    FString SelectedFolder;
    if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, TEXT("Select Output Directory"), OutputDirTextBox->GetText().ToString(), SelectedFolder))
    {
        OutputDirTextBox->SetText(FText::FromString(SelectedFolder));
    }

    return FReply::Handled();
}

// ---------------------------------------------------------------------------
// CDGLevelSeqExporter::OpenWindow
// ---------------------------------------------------------------------------

void CDGLevelSeqExporter::OpenWindow()
{
    TArray<ACDGTrajectory*> Trajectories;
    if (GEditor)
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
            for (TActorIterator<ACDGTrajectory> It(World); It; ++It)
            {
                Trajectories.Add(*It);
            }
        }
    }

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("ExporterWindowTitle", "Export & Render Trajectories"))
        .ClientSize(FVector2D(900, 780));

    Window->SetContent(
        SNew(SLevelSeqExporterWindow, Trajectories)
    );

    IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
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
