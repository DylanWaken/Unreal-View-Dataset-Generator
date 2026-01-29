// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/LevelSeqExporterWindow/CDGLevelSeqExporter.h"
#include "Trajectory/CDGKeyframe.h"
#include "LevelSequenceInterface/CDGLevelSeqSubsystem.h"
#include "IO/TrajectorySL.h"
#include "LogCameraDatasetGenEditor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
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
#include "Factories/Factory.h"
#include "UObject/SavePackage.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"

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
                            .OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type CommitType)
                            {
                                if (SelectedItem.IsValid() && SelectedItem->Trajectory.IsValid())
                                {
                                    ACDGTrajectory* Traj = SelectedItem->Trajectory.Get();
                                    Traj->Modify();
                                    Traj->TextPrompt = NewText.ToString();
                                }
                            })
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
            
            // Settings
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 10, 0, 0)
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
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SAssignNew(ClearSequenceCheckBox, SCheckBox)
                    .IsChecked(ECheckBoxState::Unchecked)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ClearSequenceLabel", "Clear Level Sequence"))
                    ]
                ]
            ]
            
            // Bottom Buttons: Cancel, Export JSON, Export
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
                    .Text(LOCTEXT("ExportJSONButton", "Export JSON"))
                    .OnClicked(this, &SLevelSeqExporterWindow::OnExportJSONClicked)
                    .ToolTipText(LOCTEXT("ExportJSONButtonTooltip", "Export all trajectories to JSON file"))
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
    // Gather selected items
    TArray<ACDGTrajectory*> TrajectoriesToExport;
    for (const auto& Item : TrajectoryItems)
    {
        if (Item->bExport && Item->Trajectory.IsValid())
        {
            TrajectoriesToExport.Add(Item->Trajectory.Get());
        }
    }

    if (TrajectoriesToExport.Num() == 0)
    {
        return FReply::Handled();
    }

    // Get Settings
    const int32 FPS = FPSInput->GetValue();
    const bool bClearSequence = ClearSequenceCheckBox->IsChecked();
    const FFrameRate FrameRate(FPS, 1);
    const double TickResolution = 24000.0; // Standard tick resolution

    // Create or Load Master Sequence
    ULevelSequence* MasterSequence = nullptr;
    
    // Try to get existing sequence from subsystem first
    if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
    {
        if (UCDGLevelSeqSubsystem* LevelSeqSubsystem = World->GetSubsystem<UCDGLevelSeqSubsystem>())
        {
            // Ensure sequence exists
            LevelSeqSubsystem->InitLevelSequence();
            MasterSequence = LevelSeqSubsystem->GetActiveLevelSequence();
        }
    }

    if (!MasterSequence)
    {
        // Fallback or error handling if subsystem fails
        return FReply::Handled();
    }

    UMovieScene* MasterMovieScene = MasterSequence->GetMovieScene();
    MasterMovieScene->SetDisplayRate(FrameRate);
    MasterMovieScene->SetTickResolutionDirectly(FFrameRate(TickResolution, 1));

    // Clear if requested
    if (bClearSequence)
    {
        // Remove all tracks
        const TArray<UMovieSceneTrack*> Tracks = MasterMovieScene->GetTracks();
        for (UMovieSceneTrack* Track : Tracks)
        {
            MasterMovieScene->RemoveTrack(*Track);
        }
        
        // Remove all Spawnables
        int32 SpawnableCount = MasterMovieScene->GetSpawnableCount();
        for (int32 i = SpawnableCount - 1; i >= 0; --i)
        {
            MasterMovieScene->RemoveSpawnable(MasterMovieScene->GetSpawnable(i).GetGuid());
        }

        // Remove all Possessables
        int32 PossessableCount = MasterMovieScene->GetPossessableCount();
        for (int32 i = PossessableCount - 1; i >= 0; --i)
        {
            MasterMovieScene->RemovePossessable(MasterMovieScene->GetPossessable(i).GetGuid());
        }
    }

    // Find or Add Cinematic Shot Track
    UMovieSceneCinematicShotTrack* ShotTrack = Cast<UMovieSceneCinematicShotTrack>(MasterMovieScene->FindTrack(UMovieSceneCinematicShotTrack::StaticClass()));
    if (!ShotTrack)
    {
        ShotTrack = MasterMovieScene->AddTrack<UMovieSceneCinematicShotTrack>();
    }

    // Calculate Start Frame (append to end)
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

    // Process Trajectories
    for (ACDGTrajectory* Trajectory : TrajectoriesToExport)
    {
        // Calculate Duration in Frames
        float Duration = Trajectory->GetTrajectoryDuration();
        int32 NumFrames = FMath::Max(1, FMath::RoundToInt(Duration * FPS));
        int32 DurationInTicks = NumFrames * (TickResolution / FPS);
        
        // Create Shot Sequence Asset
        FString ShotName = FString::Printf(TEXT("Shot_%s"), *Trajectory->TrajectoryName.ToString());
        FString PackageName = MasterPackagePath / ShotName;
        
        ULevelSequence* ShotSequence = nullptr;
        
        // Check if exists
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(PackageName + TEXT(".") + ShotName));
        
        if (AssetData.IsValid())
        {
             ShotSequence = Cast<ULevelSequence>(AssetData.GetAsset());
        }
        
        if (!ShotSequence)
        {
            // Find the Level Sequence Factory
            UFactory* Factory = nullptr;
            for (TObjectIterator<UClass> It; It; ++It)
            {
                UClass* CurrentClass = *It;
                if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
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
        if (!ShotMovieScene) {
            UE_LOG(LogTemp, Error, TEXT("Failed to get movie scene for shot sequence: %s"), *ShotName);
            continue;
        }

        // Notify that we are about to modify the sequence and movie scene
        ShotSequence->Modify();
        ShotMovieScene->Modify();

        ShotMovieScene->SetDisplayRate(FrameRate);
        ShotMovieScene->SetTickResolutionDirectly(FFrameRate(TickResolution, 1));
        
        // Clear existing tracks in shot (always overwrite shot content for simplicity)
        {
             // Create a copy of the tracks array to safely iterate while removing
             TArray<UMovieSceneTrack*> ExistingTracks = ShotMovieScene->GetTracks();
             for (UMovieSceneTrack* Track : ExistingTracks)
             {
                 ShotMovieScene->RemoveTrack(*Track);
             }
             
             // Remove all spawnables
             int32 SpawnableCount = ShotMovieScene->GetSpawnableCount();
             for (int32 i = SpawnableCount - 1; i >= 0; --i)
             {
                 ShotMovieScene->RemoveSpawnable(ShotMovieScene->GetSpawnable(i).GetGuid());
             }

             // Remove all possessables (to ensure orphaned bindings are cleared)
             int32 PossessableCount = ShotMovieScene->GetPossessableCount();
             for (int32 i = PossessableCount - 1; i >= 0; --i)
             {
                 ShotMovieScene->RemovePossessable(ShotMovieScene->GetPossessable(i).GetGuid());
             }
        }

        // Add Camera to Shot
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World)
        {
            UE_LOG(LogTemp, Error, TEXT("No valid world context found for spawning camera."));
            continue;
        }

        FString CameraName = FString::Printf(TEXT("Cam_%s"), *Trajectory->TrajectoryName.ToString());
        ACineCameraActor* CameraActor = nullptr;

        // Cleanup existing camera with the same name
        for (TActorIterator<ACineCameraActor> It(World); It; ++It)
        {
            if (It->GetActorLabel() == CameraName)
            {
                World->EditorDestroyActor(*It, true);
                break;
            }
        }

        // Spawn new persistent camera
        FActorSpawnParameters SpawnParams;
        CameraActor = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (CameraActor)
        {
            CameraActor->SetActorLabel(CameraName);
        }

        if (!CameraActor)
        {
             UE_LOG(LogTemp, Error, TEXT("Failed to spawn camera actor: %s"), *CameraName);
             continue;
        }

        // Add Possessable Binding for the Actor
        FGuid CameraGuid = ShotMovieScene->AddPossessable(CameraActor->GetActorLabel(), CameraActor->GetClass());
        ShotSequence->BindPossessableObject(CameraGuid, *CameraActor, World);

        // Camera Cut Track in Shot
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
        
            // Add Transform Track
            UMovieScene3DTransformTrack* TransformTrack = ShotMovieScene->AddTrack<UMovieScene3DTransformTrack>(CameraGuid);
            if (TransformTrack)
            {
                UMovieSceneSection* NewSection = TransformTrack->CreateNewSection();
                UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(NewSection);
                
                if (TransformSection)
                {
                    TransformTrack->AddSection(*TransformSection);
                    TransformSection->SetRange(TRange<FFrameNumber>(0, DurationInTicks));

                    // Get Channels
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
                                OutInterpMode = RCIM_Linear;
                                OutTangentMode = RCTM_Auto;
                                break;
                            case ECDGInterpolationMode::Constant:
                                OutInterpMode = RCIM_Constant;
                                OutTangentMode = RCTM_Auto;
                                break;
                            case ECDGInterpolationMode::Cubic:
                            case ECDGInterpolationMode::CubicClamped:
                                OutInterpMode = RCIM_Cubic;
                                OutTangentMode = RCTM_Auto;
                                break;
                            case ECDGInterpolationMode::CustomTangent:
                                OutInterpMode = RCIM_Cubic;
                                OutTangentMode = RCTM_User;
                                break;
                            default:
                                OutInterpMode = RCIM_Cubic;
                                OutTangentMode = RCTM_Auto;
                                break;
                            }
                        };

                        auto AddKeyToChannel = [&](FMovieSceneDoubleChannel* Channel, FFrameNumber Time, double Value, ECDGInterpolationMode Mode)
                        {
                            if (!Channel) return; // Null check for channel

                            ERichCurveInterpMode InterpMode;
                            ERichCurveTangentMode TangentMode;
                            ConvertInterpMode(Mode, InterpMode, TangentMode);

                            if (InterpMode == RCIM_Constant)
                            {
                                Channel->AddConstantKey(Time, Value);
                            }
                            else if (InterpMode == RCIM_Linear)
                            {
                                Channel->AddLinearKey(Time, Value);
                            }
                            else
                            {
                                Channel->AddCubicKey(Time, Value, TangentMode);
                            }
                        };

                        for (int32 k = 0; k < TrajectoryKeyframes.Num(); ++k)
                        {
                            ACDGKeyframe* Keyframe = TrajectoryKeyframes[k];
                            if (!Keyframe) continue;

                            if (k > 0)
                            {
                                CurrentTimeSeconds += Keyframe->TimeToCurrentFrame;
                            }

                            FFrameNumber KeyTime = FFrameNumber(static_cast<int32>(CurrentTimeSeconds * TickResolution));
                            
                            FTransform KeyTransform = Keyframe->GetKeyframeTransform();
                            FVector Loc = KeyTransform.GetLocation();
                            FRotator Rot = KeyTransform.GetRotation().Rotator();

                            bool bHasStay = Keyframe->TimeAtCurrentFrame > KINDA_SMALL_NUMBER;

                            // Add First Key (Arrival)
                            // If staying, the interpolation for this segment (during stay) should be Constant
                            ECDGInterpolationMode PosMode = bHasStay ? ECDGInterpolationMode::Constant : Keyframe->InterpolationSettings.PositionInterpMode;
                            ECDGInterpolationMode RotMode = bHasStay ? ECDGInterpolationMode::Constant : Keyframe->InterpolationSettings.RotationInterpMode;

                            AddKeyToChannel(DoubleChannels[0], KeyTime, Loc.X, PosMode);
                            AddKeyToChannel(DoubleChannels[1], KeyTime, Loc.Y, PosMode);
                            AddKeyToChannel(DoubleChannels[2], KeyTime, Loc.Z, PosMode);

                            AddKeyToChannel(DoubleChannels[3], KeyTime, Rot.Roll, RotMode);
                            AddKeyToChannel(DoubleChannels[4], KeyTime, Rot.Pitch, RotMode);
                            AddKeyToChannel(DoubleChannels[5], KeyTime, Rot.Yaw, RotMode);

                            // Add Second Key (Departure) if staying
                            if (bHasStay)
                            {
                                CurrentTimeSeconds += Keyframe->TimeAtCurrentFrame;
                                FFrameNumber EndKeyTime = FFrameNumber(static_cast<int32>(CurrentTimeSeconds * TickResolution));

                                // Use the actual interpolation settings for the departure key
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

        // Handle Camera Component Properties (Focal Length, etc.)
        UCineCameraComponent* CameraComponent = CameraActor->GetCineCameraComponent();

        if (CameraComponent)
        {
            // Create binding for component
            FGuid ComponentGuid = ShotMovieScene->AddPossessable(CameraComponent->GetName(), CameraComponent->GetClass());
            
            // Set Parent
            FMovieScenePossessable* ChildPossessable = ShotMovieScene->FindPossessable(ComponentGuid);
            if (ChildPossessable)
            {
                ChildPossessable->SetParent(CameraGuid, ShotMovieScene);
            }

            // Bind Component
            ShotSequence->BindPossessableObject(ComponentGuid, *CameraComponent, CameraActor);

            // Add Focal Length Track
            UMovieSceneFloatTrack* FocalLengthTrack = ShotMovieScene->AddTrack<UMovieSceneFloatTrack>(ComponentGuid);
            if (FocalLengthTrack)
            {
                FocalLengthTrack->SetPropertyNameAndPath(GET_MEMBER_NAME_CHECKED(UCineCameraComponent, CurrentFocalLength), "CurrentFocalLength");
                UMovieSceneFloatSection* FocalSection = Cast<UMovieSceneFloatSection>(FocalLengthTrack->CreateNewSection());
                FocalLengthTrack->AddSection(*FocalSection);
                FocalSection->SetRange(TRange<FFrameNumber>(0, DurationInTicks));
                
                FMovieSceneFloatChannel& FocalChannel = FocalSection->GetChannel();
                
                // Interpolate Focal Length from Keyframes
                TArray<ACDGKeyframe*> TrajectoryKeyframes = Trajectory->GetSortedKeyframes();
                double CurrentTimeSeconds = 0.0;
                
                for (int32 k = 0; k < TrajectoryKeyframes.Num(); ++k)
                {
                    ACDGKeyframe* Keyframe = TrajectoryKeyframes[k];
                    if (!Keyframe) continue;
                    
                    // Calculate time for this keyframe
                    if (k > 0)
                    {
                        CurrentTimeSeconds += Keyframe->TimeToCurrentFrame;
                    }
                    
                    // Add Key for Focal Length
                    FFrameNumber KeyTime = FFrameNumber(static_cast<int32>(CurrentTimeSeconds * TickResolution));
                    
                    bool bHasStay = Keyframe->TimeAtCurrentFrame > KINDA_SMALL_NUMBER;
                    
                    if (bHasStay)
                    {
                        FocalChannel.AddConstantKey(KeyTime, Keyframe->LensSettings.FocalLength);
                        
                        CurrentTimeSeconds += Keyframe->TimeAtCurrentFrame;
                        FFrameNumber EndKeyTime = FFrameNumber(static_cast<int32>(CurrentTimeSeconds * TickResolution));
                        FocalChannel.AddLinearKey(EndKeyTime, Keyframe->LensSettings.FocalLength);
                    }
                    else
                    {
                        FocalChannel.AddLinearKey(KeyTime, Keyframe->LensSettings.FocalLength);
                    }
                }
            }
        }
             
        ShotMovieScene->SetPlaybackRange(TRange<FFrameNumber>(0, DurationInTicks));
        ShotSequence->MarkPackageDirty();

        // Add Shot to Master Sequence
        if (ShotTrack)
        {
            // Use AddSequence to correctly initialize the section and parameters
            UMovieSceneSubSection* ShotSection = ShotTrack->AddSequence(ShotSequence, StartFrame, DurationInTicks);
            if (ShotSection)
            {
                // AddSequence sets the range, but we confirm it here or adjust if needed.
                // It uses the duration passed to it.
                // It also sets the row index automatically.
                
                // We don't need to manually set Parameters.StartFrameOffset or TimeScale as AddSequence defaults them correctly (TimeScale 1.0)
                // But we can ensure if needed.
                ShotSection->Parameters.TimeScale = 1.0f;
            }
        }

        // Advance StartFrame
        StartFrame += FFrameNumber(DurationInTicks);
    }
    
    // Set Master Sequence Playback Range to cover all shots
    MasterMovieScene->SetPlaybackRange(TRange<FFrameNumber>(0, StartFrame));
    
    // Mark Master Dirty
    MasterSequence->MarkPackageDirty();

    // Close window
    OnCancelClicked();
    
    return FReply::Handled();
}

FReply SLevelSeqExporterWindow::OnExportJSONClicked()
{
    // Get FPS setting from the UI
    const int32 FPS = FPSInput->GetValue();

    // Open file save dialog
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Failed to get Desktop Platform module"));
        return FReply::Handled();
    }

    // Get parent window for the file dialog
    TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
    void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) 
        ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() 
        : nullptr;

    // Setup default filename based on level name
    FString DefaultFileName = TEXT("Trajectories.json");
    if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
    {
        FString LevelName = World->GetMapName();
        LevelName.RemoveFromStart(World->StreamingLevelsPrefix); // Remove PIE prefix if present
        DefaultFileName = FString::Printf(TEXT("%s_Trajectories.json"), *LevelName);
    }

    // Default save location
    FString DefaultPath = FPaths::ProjectSavedDir() / TEXT("Trajectories");

    // Show file save dialog
    TArray<FString> OutFiles;
    bool bFileSelected = DesktopPlatform->SaveFileDialog(
        ParentWindowHandle,
        TEXT("Save Trajectories as JSON"),
        DefaultPath,
        DefaultFileName,
        TEXT("JSON Files (*.json)|*.json|All Files (*.*)|*.*"),
        EFileDialogFlags::None,
        OutFiles
    );

    if (!bFileSelected || OutFiles.Num() == 0)
    {
        // User cancelled
        return FReply::Handled();
    }

    FString FilePath = OutFiles[0];

    // Save trajectories to JSON
    bool bSuccess = TrajectorySL::SaveAllTrajectories(FilePath, FPS, true);

    // Show notification
    FNotificationInfo Info(bSuccess 
        ? FText::Format(LOCTEXT("ExportJSONSuccess", "Trajectories exported to:\n{0}"), FText::FromString(FilePath))
        : LOCTEXT("ExportJSONFailed", "Failed to export trajectories to JSON"));
    
    Info.ExpireDuration = 5.0f;
    Info.bUseLargeFont = false;
    Info.bUseSuccessFailIcons = true;
    
    FSlateNotificationManager::Get().AddNotification(Info);

    if (bSuccess)
    {
        UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("Trajectories exported to JSON: %s (FPS: %d)"), *FilePath, FPS);
    }
    else
    {
        UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Failed to export trajectories to JSON: %s"), *FilePath);
    }

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