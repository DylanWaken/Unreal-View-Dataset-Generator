// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/KeyFrameEditor/CDGKeyframeContextMenu.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "CDGKeyframeContextMenu"

void FCDGKeyframeContextMenu::Initialize()
{
	// Register the context menu extender with the Level Editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();

	MenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateSP(
		this, &FCDGKeyframeContextMenu::GetLevelViewportContextMenuExtender));
	
	LevelViewportExtenderHandle = MenuExtenders.Last().GetHandle();
	
	UE_LOG(LogTemp, Log, TEXT("CDGKeyframeContextMenu: Registered viewport context menu extender (Handle: %d)"), LevelViewportExtenderHandle.IsValid());
}

void FCDGKeyframeContextMenu::Shutdown()
{
	// Unregister the context menu extender
	if (LevelViewportExtenderHandle.IsValid())
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule)
		{
			typedef FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors DelegateType;
			LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll(
				[this](const DelegateType& In) { return In.GetHandle() == LevelViewportExtenderHandle; });
		}
	}
}

TSharedRef<FExtender> FCDGKeyframeContextMenu::GetLevelViewportContextMenuExtender(
	const TSharedRef<FUICommandList> CommandList, 
	const TArray<AActor*> InActors)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

	// Filter out CDGKeyframe actors from the selection
	TArray<ACDGKeyframe*> SelectedKeyframes;
	for (AActor* Actor : InActors)
	{
		if (ACDGKeyframe* Keyframe = Cast<ACDGKeyframe>(Actor))
		{
			SelectedKeyframes.Add(Keyframe);
		}
	}

	// Debug logging
	UE_LOG(LogTemp, Log, TEXT("CDGKeyframeContextMenu: Context menu triggered. Total actors: %d, Keyframes: %d"), 
		InActors.Num(), SelectedKeyframes.Num());

	// Only add the menu if we have CDGKeyframe actors selected
	if (SelectedKeyframes.Num() > 0)
	{
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedRef<FUICommandList> LevelEditorCommandBindings = LevelEditor.GetGlobalLevelEditorActions();

		UE_LOG(LogTemp, Log, TEXT("CDGKeyframeContextMenu: Adding menu extension for %d keyframe(s)"), SelectedKeyframes.Num());

		// Add menu extension BEFORE ActorOptions section
		// ActorOptions comes after AssetOptions and ViewOptions in the menu hierarchy
		// Using Before will place our menu above ActorOptions
		Extender->AddMenuExtension(
			"ActorOptions", 
			EExtensionHook::Before, 
			LevelEditorCommandBindings, 
			FMenuExtensionDelegate::CreateSP(this, &FCDGKeyframeContextMenu::FillKeyframeContextMenu, SelectedKeyframes));
	}

	return Extender;
}

void FCDGKeyframeContextMenu::FillKeyframeContextMenu(FMenuBuilder& MenuBuilder, const TArray<ACDGKeyframe*> SelectedKeyframes)
{
	FText MenuLabel = SelectedKeyframes.Num() == 1 
		? FText::Format(LOCTEXT("KeyframeEditorSingular", "Keyframe: {0}"), FText::FromString(SelectedKeyframes[0]->GetActorLabel()))
		: FText::Format(LOCTEXT("KeyframeEditorPlural", "Edit {0} Keyframes"), FText::AsNumber(SelectedKeyframes.Num()));

	MenuBuilder.BeginSection("CDGKeyframeEditor", MenuLabel);
	{
		// Add submenu for Trajectory settings
		MenuBuilder.AddSubMenu(
			LOCTEXT("TrajectorySettings", "Trajectory"),
			LOCTEXT("TrajectorySettingsTooltip", "Edit trajectory assignment and order"),
			FNewMenuDelegate::CreateSP(this, &FCDGKeyframeContextMenu::FillTrajectorySubmenu, SelectedKeyframes),
			false,
			FSlateIcon("TopButtonStyle", "TopButton.CustomIcon.Small")
		);

		// Add submenu for Camera settings
		MenuBuilder.AddSubMenu(
			LOCTEXT("CameraSettings", "Camera"),
			LOCTEXT("CameraSettingsTooltip", "Edit camera lens and filmback settings"),
			FNewMenuDelegate::CreateSP(this, &FCDGKeyframeContextMenu::FillCameraSubmenu, SelectedKeyframes),
			false,
			FSlateIcon("TopButtonStyle", "TopButton.CustomIcon.Small")
		);

		// Add submenu for Interpolation settings
		MenuBuilder.AddSubMenu(
			LOCTEXT("InterpolationSettings", "Interpolation"),
			LOCTEXT("InterpolationSettingsTooltip", "Edit interpolation mode and tangent settings"),
			FNewMenuDelegate::CreateSP(this, &FCDGKeyframeContextMenu::FillInterpolationSubmenu, SelectedKeyframes),
			false,
			FSlateIcon("TopButtonStyle", "TopButton.CustomIcon.Small")
		);

		// Add submenu for Visualization settings
		MenuBuilder.AddSubMenu(
			LOCTEXT("VisualizationSettings", "Visualization"),
			LOCTEXT("VisualizationSettingsTooltip", "Edit frustum and trajectory line visualization"),
			FNewMenuDelegate::CreateSP(this, &FCDGKeyframeContextMenu::FillVisualizationSubmenu, SelectedKeyframes),
			false,
			FSlateIcon("TopButtonStyle", "TopButton.CustomIcon.Small")
		);

		// Quick action: Pilot this keyframe (for single selection)
		if (SelectedKeyframes.Num() == 1)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("PilotKeyframe", "Pilot Camera"),
				LOCTEXT("PilotKeyframeTooltip", "Pilot the viewport through this keyframe's camera"),
				FSlateIcon("TopButtonStyle", "TopButton.CustomIcon.Small"),
				FUIAction(
					FExecuteAction::CreateLambda([Keyframe = SelectedKeyframes[0]]()
					{
						if (Keyframe)
						{
							// Pilot the active viewport to this keyframe
							GEditor->MoveViewportCamerasToActor(*Keyframe, false);
						}
					})
				)
			);
		}

		// Open full details panel
		MenuBuilder.AddMenuEntry(
			LOCTEXT("OpenDetailsPanel", "Focus Details Panel"),
			LOCTEXT("OpenDetailsPanelTooltip", "Focus the details panel on the selected keyframes"),
			FSlateIcon("TopButtonStyle", "TopButton.CustomIcon.Small"),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					// The keyframes are already selected, just open/focus the details panel
					FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
					LevelEditorModule.SummonSelectionDetails();
				})
			)
		);
	}
	MenuBuilder.EndSection();
}

void FCDGKeyframeContextMenu::FillTrajectorySubmenu(FMenuBuilder& MenuBuilder, const TArray<ACDGKeyframe*> SelectedKeyframes)
{
	MenuBuilder.BeginSection("TrajectorySettings", LOCTEXT("TrajectorySettingsSection", "Trajectory Assignment"));
	{
		// Trajectory Name Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TrajectoryNameLabel", "Trajectory Name:"))
					.MinDesiredWidth(100.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SEditableTextBox)
					.MinDesiredWidth(150.0f)
					.Text_Lambda([SelectedKeyframes]()
					{
						if (SelectedKeyframes.Num() == 0)
							return FText::GetEmpty();
						
						FName FirstName = SelectedKeyframes[0]->TrajectoryName;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (Keyframe->TrajectoryName != FirstName)
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return LOCTEXT("MultipleValues", "(Multiple)");
						return FText::FromName(FirstName);
					})
				.HintText(LOCTEXT("TrajectoryNameHint", "Enter trajectory name"))
				.OnTextCommitted_Lambda([SelectedKeyframes](const FText& NewText, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetTrajectoryName", "Set Trajectory Name"));
						FName NewName = FName(*NewText.ToString());
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							
							// Store old trajectory name before changing
							FName OldTrajectoryName = Keyframe->TrajectoryName;
							
							// Update trajectory name
							Keyframe->TrajectoryName = NewName;
							
							// Notify subsystem about trajectory name change
							if (UWorld* World = Keyframe->GetWorld())
							{
								if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
								{
									Subsystem->OnKeyframeTrajectoryNameChanged(Keyframe, OldTrajectoryName);
								}
							}
						}
					}
				})
				]
			],
			FText::GetEmpty()
		);

		// Order in Trajectory Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OrderLabel", "Order:"))
					.MinDesiredWidth(100.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<int32>)
					.MinDesiredValueWidth(150.0f)
					.AllowSpin(true)
					.MinValue(ACDGKeyframe::OrderInTrajectoryMin)
					.MaxValue(ACDGKeyframe::OrderInTrajectoryMax)
					.MinSliderValue(ACDGKeyframe::OrderInTrajectorySliderMin)
					.MaxSliderValue(ACDGKeyframe::OrderInTrajectorySliderMax)
					.Delta(1)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<int32>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<int32>();
						
						int32 FirstOrder = SelectedKeyframes[0]->OrderInTrajectory;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (Keyframe->OrderInTrajectory != FirstOrder)
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<int32>();
						return FirstOrder;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](int32 NewValue)
					{
						// Real-time update while dragging
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->OrderInTrajectory = FMath::Max(ACDGKeyframe::OrderInTrajectoryMin, NewValue);
							
							// Notify subsystem about order change for auto-reassignment
							if (UWorld* World = Keyframe->GetWorld())
							{
								if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
								{
									Subsystem->OnKeyframeOrderChanged(Keyframe);
								}
							}
						}
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](int32 NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetOrder", "Set Keyframe Order"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->OrderInTrajectory = FMath::Max(ACDGKeyframe::OrderInTrajectoryMin, NewValue);
							
							// Notify subsystem about order change for auto-reassignment
							if (UWorld* World = Keyframe->GetWorld())
							{
								if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
								{
									Subsystem->OnKeyframeOrderChanged(Keyframe);
								}
							}
						}
					})
				]
			],
			FText::GetEmpty()
		);

		// Time Hint Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TimeHintLabel", "Time (seconds):"))
					.MinDesiredWidth(100.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(150.0f)
					.AllowSpin(true)
					.MinValue(ACDGKeyframe::TimeHintMin)
					.MaxValue(ACDGKeyframe::TimeHintMax)
					.MinSliderValue(ACDGKeyframe::TimeHintSliderMin)
					.MaxSliderValue(ACDGKeyframe::TimeHintSliderMax)
					.Delta(0.1f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstTime = SelectedKeyframes[0]->TimeHint;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->TimeHint, FirstTime))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstTime;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						// Real-time update while dragging
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->TimeHint = FMath::Max(ACDGKeyframe::TimeHintMin, NewValue);
						}
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetTimeHint", "Set Time Hint"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->TimeHint = FMath::Max(ACDGKeyframe::TimeHintMin, NewValue);
						}
					})
				]
			],
			FText::GetEmpty()
		);

		MenuBuilder.AddSeparator();

		// Increment order
		MenuBuilder.AddMenuEntry(
			LOCTEXT("IncrementOrder", "Increment Order (+1)"),
			LOCTEXT("IncrementOrderTooltip", "Increase the order in trajectory by 1"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("IncrementKeyframeOrder", "Increment Keyframe Order"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->OrderInTrajectory++;
						
						// Notify subsystem about order change for auto-reassignment
						if (UWorld* World = Keyframe->GetWorld())
						{
							if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
							{
								Subsystem->OnKeyframeOrderChanged(Keyframe);
							}
						}
					}
				})
			)
		);

		// Decrement order
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DecrementOrder", "Decrement Order (-1)"),
			LOCTEXT("DecrementOrderTooltip", "Decrease the order in trajectory by 1"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Minus"),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("DecrementKeyframeOrder", "Decrement Keyframe Order"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->OrderInTrajectory = FMath::Max(ACDGKeyframe::OrderInTrajectoryMin, Keyframe->OrderInTrajectory - 1);
						
						// Notify subsystem about order change for auto-reassignment
						if (UWorld* World = Keyframe->GetWorld())
						{
							if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
							{
								Subsystem->OnKeyframeOrderChanged(Keyframe);
							}
						}
					}
				})
			)
		);

		MenuBuilder.AddSeparator();

		// Create new trajectory for selected keyframe(s)
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NewTrajectory", "New Trajectory"),
			LOCTEXT("NewTrajectoryTooltip", "Move this keyframe to its own new trajectory"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("NewTrajectoryAssignment", "New Trajectory"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						
						// Store old trajectory name before changing
						FName OldTrajectoryName = Keyframe->TrajectoryName;
						
						// Generate a unique trajectory name
						if (UWorld* World = Keyframe->GetWorld())
						{
							if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
							{
								Keyframe->TrajectoryName = Subsystem->GenerateUniqueTrajectoryName();
								Keyframe->OrderInTrajectory = 0;
								
								// Notify subsystem about trajectory name change
								Subsystem->OnKeyframeTrajectoryNameChanged(Keyframe, OldTrajectoryName);
							}
						}
					}
				})
			)
		);
	}
	MenuBuilder.EndSection();
}

void FCDGKeyframeContextMenu::FillCameraSubmenu(FMenuBuilder& MenuBuilder, const TArray<ACDGKeyframe*> SelectedKeyframes)
{
	MenuBuilder.BeginSection("CameraProperties", LOCTEXT("CameraPropertiesSection", "Camera Properties"));
	{
		// Focal Length Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FocalLengthLabel", "Focal Length (mm):"))
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(130.0f)
					.AllowSpin(true)
					.MinValue(FCDGCameraLensSettings::FocalLengthMin)
					.MaxValue(FCDGCameraLensSettings::FocalLengthMax)
					.MinSliderValue(FCDGCameraLensSettings::FocalLengthSliderMin)
					.MaxSliderValue(FCDGCameraLensSettings::FocalLengthSliderMax)
					.Delta(1.0f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstValue = SelectedKeyframes[0]->LensSettings.FocalLength;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->LensSettings.FocalLength, FirstValue))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						// Real-time update while dragging
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->LensSettings.FocalLength = FMath::Clamp(NewValue, FCDGCameraLensSettings::FocalLengthMin, FCDGCameraLensSettings::FocalLengthMax);
							// FOV is always synchronized with focal length
							Keyframe->UpdateFOVFromFocalLength();
							Keyframe->UpdateVisualizer();
						}
						GEditor->RedrawLevelEditingViewports();
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetFocalLength", "Set Focal Length"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->LensSettings.FocalLength = FMath::Clamp(NewValue, FCDGCameraLensSettings::FocalLengthMin, FCDGCameraLensSettings::FocalLengthMax);
							// FOV is always synchronized with focal length
							Keyframe->UpdateFOVFromFocalLength();
							Keyframe->UpdateVisualizer();
						}
					})
				]
			],
			FText::GetEmpty()
		);

		// FOV Editor with Lock Toggle
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FOVLabel", "Field of View (°):"))
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(100.0f)
					.AllowSpin(true)
					.MinValue(FCDGCameraLensSettings::FieldOfViewMin)
					.MaxValue(FCDGCameraLensSettings::FieldOfViewMax)
					.MinSliderValue(FCDGCameraLensSettings::FieldOfViewSliderMin)
					.MaxSliderValue(FCDGCameraLensSettings::FieldOfViewSliderMax)
					.Delta(1.0f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstValue = SelectedKeyframes[0]->LensSettings.FieldOfView;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->LensSettings.FieldOfView, FirstValue))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						// Real-time update while dragging
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->LensSettings.FieldOfView = FMath::Clamp(NewValue, FCDGCameraLensSettings::FieldOfViewMin, FCDGCameraLensSettings::FieldOfViewMax);
							// Focal length is always synchronized with FOV
							Keyframe->UpdateFocalLengthFromFOV();
							Keyframe->UpdateVisualizer();
						}
						GEditor->RedrawLevelEditingViewports();
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetFOV", "Set Field of View"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->LensSettings.FieldOfView = FMath::Clamp(NewValue, FCDGCameraLensSettings::FieldOfViewMin, FCDGCameraLensSettings::FieldOfViewMax);
							// Focal length is always synchronized with FOV
							Keyframe->UpdateFocalLengthFromFOV();
							Keyframe->UpdateVisualizer();
						}
					})
				]
				// Lock button removed - FOV and Focal Length are now always locked
			],
			FText::GetEmpty()
		);

		// Aperture (f-stop) Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ApertureLabel", "Aperture (f-stop):"))
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(130.0f)
					.AllowSpin(true)
					.MinValue(FCDGCameraLensSettings::ApertureMin)
					.MaxValue(FCDGCameraLensSettings::ApertureMax)
					.MinSliderValue(FCDGCameraLensSettings::ApertureMin)
					.MaxSliderValue(FCDGCameraLensSettings::ApertureMax)
					.Delta(0.1f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstValue = SelectedKeyframes[0]->LensSettings.Aperture;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->LensSettings.Aperture, FirstValue))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						// Real-time update while dragging
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->LensSettings.Aperture = FMath::Clamp(NewValue, FCDGCameraLensSettings::ApertureMin, FCDGCameraLensSettings::ApertureMax);
						}
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetAperture", "Set Aperture"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->LensSettings.Aperture = FMath::Clamp(NewValue, FCDGCameraLensSettings::ApertureMin, FCDGCameraLensSettings::ApertureMax);
						}
					})
				]
			],
			FText::GetEmpty()
		);

		// Focus Distance Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FocusDistanceLabel", "Focus Distance (cm):"))
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(130.0f)
					.AllowSpin(true)
					.MinValue(FCDGCameraLensSettings::FocusDistanceMin)
					.MaxValue(FCDGCameraLensSettings::FocusDistanceMax)
					.MinSliderValue(FCDGCameraLensSettings::FocusDistanceSliderMin)
					.MaxSliderValue(FCDGCameraLensSettings::FocusDistanceSliderMax)
					.Delta(100.0f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstValue = SelectedKeyframes[0]->LensSettings.FocusDistance;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->LensSettings.FocusDistance, FirstValue))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						// Real-time update while dragging
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->LensSettings.FocusDistance = FMath::Max(FCDGCameraLensSettings::FocusDistanceMin, NewValue);
						}
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetFocusDistance", "Set Focus Distance"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->LensSettings.FocusDistance = FMath::Max(FCDGCameraLensSettings::FocusDistanceMin, NewValue);
						}
					})
				]
			],
			FText::GetEmpty()
		);

		// Diaphragm Blade Count Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DiaphragmBladeCountLabel", "Diaphragm Blades:"))
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<int32>)
					.MinDesiredValueWidth(130.0f)
					.AllowSpin(true)
					.MinValue(FCDGCameraLensSettings::DiaphragmBladeCountMin)
					.MaxValue(FCDGCameraLensSettings::DiaphragmBladeCountMax)
					.Delta(1)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<int32>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<int32>();
						
						int32 FirstValue = SelectedKeyframes[0]->LensSettings.DiaphragmBladeCount;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (Keyframe->LensSettings.DiaphragmBladeCount != FirstValue)
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<int32>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](int32 NewValue)
					{
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->LensSettings.DiaphragmBladeCount = FMath::Clamp(NewValue, FCDGCameraLensSettings::DiaphragmBladeCountMin, FCDGCameraLensSettings::DiaphragmBladeCountMax);
						}
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](int32 NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetDiaphragmBladeCount", "Set Diaphragm Blade Count"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->LensSettings.DiaphragmBladeCount = FMath::Clamp(NewValue, FCDGCameraLensSettings::DiaphragmBladeCountMin, FCDGCameraLensSettings::DiaphragmBladeCountMax);
						}
					})
				]
			],
			FText::GetEmpty()
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("FilmbackProperties", LOCTEXT("FilmbackPropertiesSection", "Filmback Settings"));
	{
		// Sensor Width Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SensorWidthLabel", "Sensor Width (mm):"))
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(130.0f)
					.AllowSpin(true)
					.MinValue(FCDGCameraFilmbackSettings::SensorWidthMin)
					.MaxValue(FCDGCameraFilmbackSettings::SensorWidthMax)
					.Delta(0.1f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstValue = SelectedKeyframes[0]->FilmbackSettings.SensorWidth;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->FilmbackSettings.SensorWidth, FirstValue))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->FilmbackSettings.SensorWidth = FMath::Max(FCDGCameraFilmbackSettings::SensorWidthMin, NewValue);
							Keyframe->UpdateVisualizer();
						}
						GEditor->RedrawLevelEditingViewports();
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetSensorWidth", "Set Sensor Width"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->FilmbackSettings.SensorWidth = FMath::Max(FCDGCameraFilmbackSettings::SensorWidthMin, NewValue);
							Keyframe->UpdateVisualizer();
						}
					})
				]
			],
			FText::GetEmpty()
		);

		// Sensor Height Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SensorHeightLabel", "Sensor Height (mm):"))
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(130.0f)
					.AllowSpin(true)
					.MinValue(FCDGCameraFilmbackSettings::SensorHeightMin)
					.MaxValue(FCDGCameraFilmbackSettings::SensorHeightMax)
					.Delta(0.1f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstValue = SelectedKeyframes[0]->FilmbackSettings.SensorHeight;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->FilmbackSettings.SensorHeight, FirstValue))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->FilmbackSettings.SensorHeight = FMath::Max(FCDGCameraFilmbackSettings::SensorHeightMin, NewValue);
							Keyframe->UpdateVisualizer();
						}
						GEditor->RedrawLevelEditingViewports();
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetSensorHeight", "Set Sensor Height"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->FilmbackSettings.SensorHeight = FMath::Max(FCDGCameraFilmbackSettings::SensorHeightMin, NewValue);
							Keyframe->UpdateVisualizer();
						}
					})
				]
			],
			FText::GetEmpty()
		);

		// Sensor Aspect Ratio Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SensorAspectRatioLabel", "Sensor Aspect Ratio:"))
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(130.0f)
					.AllowSpin(true)
					.MinValue(FCDGCameraFilmbackSettings::SensorAspectRatioMin)
					.MaxValue(FCDGCameraFilmbackSettings::SensorAspectRatioMax)
					.Delta(0.01f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstValue = SelectedKeyframes[0]->FilmbackSettings.SensorAspectRatio;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->FilmbackSettings.SensorAspectRatio, FirstValue))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->FilmbackSettings.SensorAspectRatio = FMath::Max(FCDGCameraFilmbackSettings::SensorAspectRatioMin, NewValue);
							Keyframe->UpdateVisualizer();
						}
						GEditor->RedrawLevelEditingViewports();
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetSensorAspectRatio", "Set Sensor Aspect Ratio"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->FilmbackSettings.SensorAspectRatio = FMath::Max(FCDGCameraFilmbackSettings::SensorAspectRatioMin, NewValue);
							Keyframe->UpdateVisualizer();
						}
					})
				]
			],
			FText::GetEmpty()
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("CameraPresets", LOCTEXT("CameraPresetsSection", "Quick Presets"));
	{
		// Wide angle preset
		MenuBuilder.AddMenuEntry(
			LOCTEXT("WideAnglePreset", "Wide Angle (24mm)"),
			LOCTEXT("WideAnglePresetTooltip", "Set focal length to 24mm for wide angle shots"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("SetWideAngle", "Set Wide Angle"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->LensSettings.FocalLength = 24.0f;
						Keyframe->UpdateFOVFromFocalLength();
					}
				})
			)
		);

		// Normal preset
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NormalPreset", "Normal (35mm)"),
			LOCTEXT("NormalPresetTooltip", "Set focal length to 35mm for normal field of view"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("SetNormal", "Set Normal FOV"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->LensSettings.FocalLength = 35.0f;
						Keyframe->UpdateFOVFromFocalLength();
					}
				})
			)
		);

		// Portrait preset
		MenuBuilder.AddMenuEntry(
			LOCTEXT("PortraitPreset", "Portrait (50mm)"),
			LOCTEXT("PortraitPresetTooltip", "Set focal length to 50mm for portrait shots"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("SetPortrait", "Set Portrait"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->LensSettings.FocalLength = 50.0f;
						Keyframe->UpdateFOVFromFocalLength();
					}
				})
			)
		);

		// Telephoto preset
		MenuBuilder.AddMenuEntry(
			LOCTEXT("TelephotoPreset", "Telephoto (85mm)"),
			LOCTEXT("TelephotoPresetTooltip", "Set focal length to 85mm for telephoto shots"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("SetTelephoto", "Set Telephoto"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->LensSettings.FocalLength = 85.0f;
						Keyframe->UpdateFOVFromFocalLength();
					}
				})
			)
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("DepthOfField", LOCTEXT("DepthOfFieldSection", "Depth of Field"));
	{
		// Shallow DOF
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShallowDOF", "Shallow DOF (f/1.4)"),
			LOCTEXT("ShallowDOFTooltip", "Set aperture to f/1.4 for shallow depth of field"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("SetShallowDOF", "Set Shallow DOF"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->LensSettings.Aperture = 1.4f;
					}
				})
			)
		);

		// Medium DOF
		MenuBuilder.AddMenuEntry(
			LOCTEXT("MediumDOF", "Medium DOF (f/2.8)"),
			LOCTEXT("MediumDOFTooltip", "Set aperture to f/2.8 for medium depth of field"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("SetMediumDOF", "Set Medium DOF"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->LensSettings.Aperture = 2.8f;
					}
				})
			)
		);

		// Deep DOF
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeepDOF", "Deep DOF (f/8)"),
			LOCTEXT("DeepDOFTooltip", "Set aperture to f/8 for deep depth of field"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("SetDeepDOF", "Set Deep DOF"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->LensSettings.Aperture = 8.0f;
					}
				})
			)
		);
	}
	MenuBuilder.EndSection();
}

void FCDGKeyframeContextMenu::FillInterpolationSubmenu(FMenuBuilder& MenuBuilder, const TArray<ACDGKeyframe*> SelectedKeyframes)
{
	MenuBuilder.BeginSection("InterpolationProperties", LOCTEXT("InterpolationPropertiesSection", "Interpolation Properties"));
	{
		// Tension Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TensionLabel", "Tension:"))
					.MinDesiredWidth(100.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(150.0f)
					.AllowSpin(true)
					.MinValue(FCDGSplineInterpolationSettings::TensionMin)
					.MaxValue(FCDGSplineInterpolationSettings::TensionMax)
					.MinSliderValue(FCDGSplineInterpolationSettings::TensionMin)
					.MaxSliderValue(FCDGSplineInterpolationSettings::TensionMax)
					.Delta(0.1f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstValue = SelectedKeyframes[0]->InterpolationSettings.Tension;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->InterpolationSettings.Tension, FirstValue))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						// Real-time update while dragging
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->InterpolationSettings.Tension = FMath::Clamp(NewValue, FCDGSplineInterpolationSettings::TensionMin, FCDGSplineInterpolationSettings::TensionMax);
							Keyframe->NotifyTrajectorySubsystem();
						}
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetTension", "Set Interpolation Tension"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->InterpolationSettings.Tension = FMath::Clamp(NewValue, FCDGSplineInterpolationSettings::TensionMin, FCDGSplineInterpolationSettings::TensionMax);
							Keyframe->NotifyTrajectorySubsystem();
						}
					})
				]
			],
			FText::GetEmpty()
		);

		// Bias Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BiasLabel", "Bias:"))
					.MinDesiredWidth(100.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(150.0f)
					.AllowSpin(true)
					.MinValue(FCDGSplineInterpolationSettings::BiasMin)
					.MaxValue(FCDGSplineInterpolationSettings::BiasMax)
					.MinSliderValue(FCDGSplineInterpolationSettings::BiasMin)
					.MaxSliderValue(FCDGSplineInterpolationSettings::BiasMax)
					.Delta(0.1f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstValue = SelectedKeyframes[0]->InterpolationSettings.Bias;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->InterpolationSettings.Bias, FirstValue))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						// Real-time update while dragging
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->InterpolationSettings.Bias = FMath::Clamp(NewValue, FCDGSplineInterpolationSettings::BiasMin, FCDGSplineInterpolationSettings::BiasMax);
							Keyframe->NotifyTrajectorySubsystem();
						}
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetBias", "Set Interpolation Bias"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->InterpolationSettings.Bias = FMath::Clamp(NewValue, FCDGSplineInterpolationSettings::BiasMin, FCDGSplineInterpolationSettings::BiasMax);
							Keyframe->NotifyTrajectorySubsystem();
						}
					})
				]
			],
			FText::GetEmpty()
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("InterpolationMode", LOCTEXT("InterpolationModeSection", "Interpolation Mode"));
	{
		// Linear interpolation
		MenuBuilder.AddMenuEntry(
			LOCTEXT("LinearInterp", "Linear"),
			LOCTEXT("LinearInterpTooltip", "Use linear interpolation"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("SetLinearInterp", "Set Linear Interpolation"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->InterpolationSettings.PositionInterpMode = ECDGInterpolationMode::Linear;
						Keyframe->InterpolationSettings.RotationInterpMode = ECDGInterpolationMode::Linear;
						Keyframe->NotifyTrajectorySubsystem();
					}
				})
			)
		);

		// Cubic smooth interpolation
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CubicInterp", "Cubic (Smooth)"),
			LOCTEXT("CubicInterpTooltip", "Use cubic interpolation for smooth curves"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("SetCubicInterp", "Set Cubic Interpolation"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->InterpolationSettings.PositionInterpMode = ECDGInterpolationMode::Cubic;
						Keyframe->InterpolationSettings.RotationInterpMode = ECDGInterpolationMode::Cubic;
						Keyframe->NotifyTrajectorySubsystem();
					}
				})
			)
		);

		// Constant (step) interpolation
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ConstantInterp", "Constant (Step)"),
			LOCTEXT("ConstantInterpTooltip", "Use constant interpolation (no smoothing)"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("SetConstantInterp", "Set Constant Interpolation"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->InterpolationSettings.PositionInterpMode = ECDGInterpolationMode::Constant;
						Keyframe->InterpolationSettings.RotationInterpMode = ECDGInterpolationMode::Constant;
						Keyframe->NotifyTrajectorySubsystem();
					}
				})
			)
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("QuaternionSettings", LOCTEXT("QuaternionSettingsSection", "Rotation Settings"));
	{
		// Get current quaternion setting
		bool bCurrentlyUsingQuaternion = SelectedKeyframes.Num() > 0 ? SelectedKeyframes[0]->InterpolationSettings.bUseQuaternionInterpolation : true;

		// Toggle quaternion interpolation
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleQuaternion", "Use Quaternion Interpolation"),
			LOCTEXT("ToggleQuaternionTooltip", "Prevents gimbal lock during rotation interpolation"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("ToggleQuaternionInterp", "Toggle Quaternion Interpolation"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->InterpolationSettings.bUseQuaternionInterpolation = !Keyframe->InterpolationSettings.bUseQuaternionInterpolation;
						Keyframe->NotifyTrajectorySubsystem();
					}
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([bCurrentlyUsingQuaternion]() { return bCurrentlyUsingQuaternion; })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();
}

void FCDGKeyframeContextMenu::FillVisualizationSubmenu(FMenuBuilder& MenuBuilder, const TArray<ACDGKeyframe*> SelectedKeyframes)
{
	MenuBuilder.BeginSection("VisualizationProperties", LOCTEXT("VisualizationPropertiesSection", "Visualization Properties"));
	{
		// Frustum Size Editor
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FrustumSizeLabel", "Frustum Size:"))
					.MinDesiredWidth(100.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinDesiredValueWidth(150.0f)
					.AllowSpin(true)
					.MinValue(ACDGKeyframe::FrustumSizeMin)
					.MaxValue(ACDGKeyframe::FrustumSizeMax)
					.MinSliderValue(ACDGKeyframe::FrustumSizeSliderMin)
					.MaxSliderValue(ACDGKeyframe::FrustumSizeSliderMax)
					.Delta(10.0f)
					.Value_Lambda([SelectedKeyframes]() -> TOptional<float>
					{
						if (SelectedKeyframes.Num() == 0)
							return TOptional<float>();
						
						float FirstValue = SelectedKeyframes[0]->FrustumSize;
						bool bAllSame = true;
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							if (!FMath::IsNearlyEqual(Keyframe->FrustumSize, FirstValue))
							{
								bAllSame = false;
								break;
							}
						}
						
						if (!bAllSame)
							return TOptional<float>();
						return FirstValue;
					})
					.OnValueChanged_Lambda([SelectedKeyframes](float NewValue)
					{
						// Real-time update while dragging
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->FrustumSize = FMath::Clamp(NewValue, ACDGKeyframe::FrustumSizeMin, ACDGKeyframe::FrustumSizeMax);
							Keyframe->UpdateVisualizer();
						}
					})
					.OnValueCommitted_Lambda([SelectedKeyframes](float NewValue, ETextCommit::Type CommitType)
					{
						const FScopedTransaction Transaction(LOCTEXT("SetFrustumSize", "Set Frustum Size"));
						for (ACDGKeyframe* Keyframe : SelectedKeyframes)
						{
							Keyframe->Modify();
							Keyframe->FrustumSize = FMath::Clamp(NewValue, ACDGKeyframe::FrustumSizeMin, ACDGKeyframe::FrustumSizeMax);
							Keyframe->UpdateVisualizer();
						}
					})
				]
			],
			FText::GetEmpty()
		);

		MenuBuilder.AddSeparator();

		// Get current visualization settings for toggles
		bool bShowFrustum = SelectedKeyframes.Num() > 0 ? SelectedKeyframes[0]->bShowCameraFrustum : true;
		bool bShowTrajectory = SelectedKeyframes.Num() > 0 ? SelectedKeyframes[0]->bShowTrajectoryLine : true;

		// Toggle camera frustum
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleFrustum", "Show Camera Frustum"),
			LOCTEXT("ToggleFrustumTooltip", "Toggle the camera frustum visualization"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visibility"),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("ToggleFrustum", "Toggle Camera Frustum"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->bShowCameraFrustum = !Keyframe->bShowCameraFrustum;
						Keyframe->UpdateVisualizer();
					}
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([bShowFrustum]() { return bShowFrustum; })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		// Toggle trajectory line
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleTrajectoryLine", "Show Trajectory Line"),
			LOCTEXT("ToggleTrajectoryLineTooltip", "Toggle the trajectory line to next keyframe"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Timeline"),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedKeyframes]()
				{
					const FScopedTransaction Transaction(LOCTEXT("ToggleTrajectoryLine", "Toggle Trajectory Line"));
					for (ACDGKeyframe* Keyframe : SelectedKeyframes)
					{
						Keyframe->Modify();
						Keyframe->bShowTrajectoryLine = !Keyframe->bShowTrajectoryLine;
						Keyframe->UpdateVisualizer();
					}
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([bShowTrajectory]() { return bShowTrajectory; })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE

