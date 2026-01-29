// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Trajectory/CDGTrajectory.h"

class ACDGTrajectory;

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
 * Window for exporting trajectories to Level Sequence
 */
class SLevelSeqExporterWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLevelSeqExporterWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TArray<ACDGTrajectory*>& InTrajectories);

private:
    /** Generate a row for the list view */
    TSharedRef<ITableRow> GenerateTrajectoryRow(TSharedPtr<FTrajectoryExportItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    
    /** Handle selection change in the list view */
    void OnSelectionChanged(TSharedPtr<FTrajectoryExportItem> NewItem, ESelectInfo::Type SelectInfo);
    
    /** Button handlers */
    FReply OnCancelClicked();
    FReply OnExportClicked();
    FReply OnExportJSONClicked();
    
    /** Checkbox handlers */
    void OnToggleExport(ECheckBoxState NewState, TSharedPtr<FTrajectoryExportItem> Item);
    ECheckBoxState IsExportChecked(TSharedPtr<FTrajectoryExportItem> Item) const;

    /** Update the summary view based on selection */
    void UpdateSummary();

private:
    /** List of data items */
    TArray<TSharedPtr<FTrajectoryExportItem>> TrajectoryItems;
    
    /** The list view widget */
    TSharedPtr<SListView<TSharedPtr<FTrajectoryExportItem>>> TrajectoryListView;
    
    /** Currently selected item */
    TSharedPtr<FTrajectoryExportItem> SelectedItem;
    
    /** Summary widgets */
    TSharedPtr<STextBlock> SummaryNameText;
    TSharedPtr<STextBlock> SummaryDurationText;
    TSharedPtr<STextBlock> SummaryKeyframeCountText;
    TSharedPtr<SMultiLineEditableTextBox> SummaryPromptText;
    TSharedPtr<STextBlock> SummaryInfoText; // General status or instructions

    /** Export Settings */
    TSharedPtr<SSpinBox<int32>> FPSInput;
    TSharedPtr<SCheckBox> ClearSequenceCheckBox;
};

/**
 * Utility to open the exporter window
 */
class CDGLevelSeqExporter
{
public:
    /** Open the exporter window with all trajectories in the current world */
    static void OpenWindow();
};

