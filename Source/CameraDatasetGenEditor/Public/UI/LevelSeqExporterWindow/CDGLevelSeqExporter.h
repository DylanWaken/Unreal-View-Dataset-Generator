// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Trajectory/CDGTrajectory.h"
#include "MRQInterface/CDGMRQInterface.h"

class ACDGTrajectory;
class ULevelSequence;
class UMovieScene;
struct FAssetData;

/**
 * Data item for the trajectory list view
 */
struct FTrajectoryExportItem
{
    TWeakObjectPtr<ACDGTrajectory> Trajectory;
    FString Name;
    float Duration;
    bool bExport;

    FTrajectoryExportItem(ACDGTrajectory* InTrajectory);
};

/**
 * Merged window: export trajectories to Level Sequence and optionally render via MRQ
 */
class SLevelSeqExporterWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLevelSeqExporterWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TArray<ACDGTrajectory*>& InTrajectories);

private:
    // ----- Trajectory list -----
    TSharedRef<ITableRow> GenerateTrajectoryRow(TSharedPtr<FTrajectoryExportItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    void OnSelectionChanged(TSharedPtr<FTrajectoryExportItem> NewItem, ESelectInfo::Type SelectInfo);
    void OnToggleExport(ECheckBoxState NewState, TSharedPtr<FTrajectoryExportItem> Item);
    ECheckBoxState IsExportChecked(TSharedPtr<FTrajectoryExportItem> Item) const;
    void UpdateSummary();

    // ----- Base level sequence -----
    void OnBaseLevelSequenceSelected(const FAssetData& AssetData);
    FString GetBaseLevelSequencePath() const;
    bool ValidateBaseLevelSequence();
    bool IsExportButtonEnabled() const;
    FText GetBaseLevelSequenceValidationMessage() const;

    // ----- Export logic -----
    /** Performs the export and returns the resulting master sequence (nullptr on failure). Does NOT close the window. */
    ULevelSequence* PerformExport();

    /** Copy non-camera tracks from the base shot into a target shot movie scene */
    void CopyBaseShotNonCameraTracks(UMovieScene* TargetShotMovieScene) const;
    static bool IsCameraBinding(UMovieScene* MovieScene, const FGuid& BindingGuid, const FString& BindingName);

    // ----- Render / output format -----
    TSharedRef<SWidget> MakeOutputFormatWidget(TSharedPtr<ECDGRenderOutputFormat> InItem);
    FText GetOutputFormatText() const;
    void OnOutputFormatChanged(TSharedPtr<ECDGRenderOutputFormat> NewFormat, ESelectInfo::Type SelectInfo);
    bool IsFFmpegAvailable() const;
    bool DoesFormatRequireFFmpeg() const;

    // ----- Button handlers -----
    FReply OnCancelClicked();
    FReply OnExportClicked();
    FReply OnExportJSONClicked();
    FReply OnRenderClicked();
    FReply OnBrowseOutputDirClicked();

private:
    // Trajectory list
    TArray<TSharedPtr<FTrajectoryExportItem>> TrajectoryItems;
    TSharedPtr<SListView<TSharedPtr<FTrajectoryExportItem>>> TrajectoryListView;
    TSharedPtr<FTrajectoryExportItem> SelectedItem;

    // Summary widgets
    TSharedPtr<STextBlock> SummaryNameText;
    TSharedPtr<STextBlock> SummaryDurationText;
    TSharedPtr<STextBlock> SummaryKeyframeCountText;
    TSharedPtr<SMultiLineEditableTextBox> SummaryPromptText;
    TSharedPtr<STextBlock> SummaryInfoText;

    // Export Settings
    TSharedPtr<SSpinBox<int32>> FPSInput;
    TSharedPtr<SCheckBox> ClearSequenceCheckBox;

    // Optional base level sequence for cloning non-camera animation
    TWeakObjectPtr<ULevelSequence> BaseLevelSequence;
    TWeakObjectPtr<ULevelSequence> BaseShotSequence;
    double BaseShotDurationSeconds = 0.0;
    bool bIsBaseLevelSequenceValid = true;
    FString BaseLevelSequenceValidationError;

    // Render Settings
    TSharedPtr<SEditableTextBox> OutputDirTextBox;
    TSharedPtr<SSpinBox<int32>> ResolutionWidthInput;
    TSharedPtr<SSpinBox<int32>> ResolutionHeightInput;
    TSharedPtr<SComboBox<TSharedPtr<ECDGRenderOutputFormat>>> OutputFormatComboBox;
    TSharedPtr<SCheckBox> ExportIndexJSONCheckBox;
    TSharedPtr<SCheckBox> OverwriteExistingCheckBox;
    TSharedPtr<SSpinBox<int32>> SpatialSampleCountInput;
    TSharedPtr<SSpinBox<int32>> TemporalSampleCountInput;
    TSharedPtr<SCheckBox> KeepExportedSequenceCheckBox;

    // Output format combo data
    TArray<TSharedPtr<ECDGRenderOutputFormat>> OutputFormatOptions;
    TSharedPtr<ECDGRenderOutputFormat> SelectedOutputFormat;
};

/**
 * Utility to open the merged export & render window
 */
class CDGLevelSeqExporter
{
public:
    static void OpenWindow();
};
