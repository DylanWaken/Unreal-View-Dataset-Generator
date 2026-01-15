// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/LevelSeqExporterWindow/CDGLevelSeqExporter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "CDGLevelSeqExporter"

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

void SLevelSeqExporterWindow::Construct(const FArguments& InArgs, const TArray<ACDGTrajectory*>& InTrajectories)
{
    // Initialize data items
    TrajectoryItems.Empty();
    for (ACDGTrajectory* Trajectory : InTrajectories)
    {
        if (Trajectory)
        {
            TrajectoryItems.Add(MakeShared<FTrajectoryExportItem>(Trajectory));
        }
    }

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(FMargin(10.0f))
        [
            SNew(SVerticalBox)
            
            // Main Content: Splitter (List | Summary)
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
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
                        
                        // Trajectory Name
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

                        // Trajectory Duration
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

                        // Keyframe Count (Bonus info)
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

                         // Instructions/Status
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
            
            // Bottom Buttons: Cancel, Export
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 10, 0, 0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f) // Spacer
                
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
                    .Text(LOCTEXT("ExportButton", "Export"))
                    .OnClicked(this, &SLevelSeqExporterWindow::OnExportClicked)
                    .ToolTipText(LOCTEXT("ExportButtonTooltip", "Export selected trajectories"))
                ]
            ]
        ]
    ];
}

TSharedRef<ITableRow> SLevelSeqExporterWindow::GenerateTrajectoryRow(TSharedPtr<FTrajectoryExportItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    typedef STableRow<TSharedPtr<FTrajectoryExportItem>> RowType;
    return SNew(RowType, OwnerTable)
        [
            SNew(SHorizontalBox)
            
            // Checkbox
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(5.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(SCheckBox)
                .IsChecked(this, &SLevelSeqExporterWindow::IsExportChecked, Item)
                .OnCheckStateChanged(this, &SLevelSeqExporterWindow::OnToggleExport, Item)
            ]
            
            // Name
            + SHorizontalBox::Slot()
            .FillWidth(0.7f)
            .Padding(FMargin(5.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->Name))
            ]
            
            // Duration
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
        SummaryInfoText->SetText(FText::GetEmpty());
    }
    else
    {
        SummaryNameText->SetText(LOCTEXT("NoSelection", "None"));
        SummaryDurationText->SetText(LOCTEXT("ZeroDuration", "0.0s"));
        SummaryKeyframeCountText->SetText(LOCTEXT("ZeroKeyframes", "0"));
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

FReply SLevelSeqExporterWindow::OnCancelClicked()
{
    TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
    if (Window.IsValid())
    {
        Window->RequestDestroyWindow();
    }
    return FReply::Handled();
}

FReply SLevelSeqExporterWindow::OnExportClicked()
{
    // TODO: Implement Export Logic
    // Gather all items with bExport == true and process them
    
    // For now, hook is empty as requested
    
    // Optionally close window after export
    // OnCancelClicked(); 
    
    return FReply::Handled();
}

void CDGLevelSeqExporter::OpenWindow()
{
    // Find all trajectories in the world
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
        .Title(LOCTEXT("ExporterWindowTitle", "Export Trajectories to Level Sequence"))
        .ClientSize(FVector2D(800, 500));

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

