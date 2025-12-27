// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CameraPreviewEditor/CDGCameraPreviewContextMenu.h"
#include "Trajectory/CDGKeyframe.h"
#include "CDGEditorState.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "TimerManager.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "CDGCameraPreviewContextMenu"

// ==================== SCDGCameraPreviewContextMenu ====================

void SCDGCameraPreviewContextMenu::Construct(const FArguments& InArgs, ACDGKeyframe* InKeyframe)
{
	Keyframe = InKeyframe;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(400.0f)
			.MaxDesiredWidth(500.0f)
			.MaxDesiredHeight(600.0f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)

				+ SScrollBox::Slot()
				.Padding(0, 0, 0, 8)
				[
					BuildCameraSettingsContent()
				]
			]
		]
	];
}

void SCDGCameraPreviewContextMenu::SetKeyframe(ACDGKeyframe* InKeyframe)
{
	Keyframe = InKeyframe;
}

TSharedRef<SWidget> SCDGCameraPreviewContextMenu::BuildCameraSettingsContent()
{
	return SNew(SVerticalBox)

		// Header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CameraSettingsTitle", "Camera Settings"))
			.Font(FAppStyle::GetFontStyle("NormalFontBold"))
			.Justification(ETextJustify::Center)
		]

		// ==================== LENS PROPERTIES ====================

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LensPropertiesHeader", "Lens Properties"))
			.Font(FAppStyle::GetFontStyle("SmallFontBold"))
		]

		// Focal Length
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.MinDesiredWidth(140.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FocalLengthLabel", "Focal Length (mm):"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.MinValue(FCDGCameraLensSettings::FocalLengthMin)
				.MaxValue(FCDGCameraLensSettings::FocalLengthMax)
				.MinSliderValue(FCDGCameraLensSettings::FocalLengthSliderMin)
				.MaxSliderValue(FCDGCameraLensSettings::FocalLengthSliderMax)
				.Delta(1.0f)
				.Value_Lambda([this]() -> TOptional<float>
				{
					if (Keyframe.IsValid())
					{
						return Keyframe->LensSettings.FocalLength;
					}
					return TOptional<float>();
				})
				.OnValueChanged_Lambda([this](float NewValue)
				{
					if (Keyframe.IsValid())
					{
						Keyframe->LensSettings.FocalLength = FMath::Clamp(NewValue, FCDGCameraLensSettings::FocalLengthMin, FCDGCameraLensSettings::FocalLengthMax);
						Keyframe->UpdateFOVFromFocalLength();
						Keyframe->UpdateVisualizer();
						
						// Sync viewport camera to reflect FOV change
						UWorld* World = Keyframe->GetWorld();
						UCDGEditorState* EditorState = World ? World->GetSubsystem<UCDGEditorState>() : nullptr;
						if (EditorState)
						{
							EditorState->UpdateViewportFromKeyframe();
						}
						
						GEditor->RedrawLevelEditingViewports();
					}
				})
				.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetFocalLength", "Set Focal Length"));
						Keyframe->Modify();
						Keyframe->LensSettings.FocalLength = FMath::Clamp(NewValue, FCDGCameraLensSettings::FocalLengthMin, FCDGCameraLensSettings::FocalLengthMax);
						Keyframe->UpdateFOVFromFocalLength();
						Keyframe->UpdateVisualizer();
					}
				})
			]
		]

		// Field of View
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.MinDesiredWidth(140.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FOVLabel", "Field of View (°):"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.MinValue(FCDGCameraLensSettings::FieldOfViewMin)
				.MaxValue(FCDGCameraLensSettings::FieldOfViewMax)
				.MinSliderValue(FCDGCameraLensSettings::FieldOfViewSliderMin)
				.MaxSliderValue(FCDGCameraLensSettings::FieldOfViewSliderMax)
				.Delta(1.0f)
				.Value_Lambda([this]() -> TOptional<float>
				{
					if (Keyframe.IsValid())
					{
						return Keyframe->LensSettings.FieldOfView;
					}
					return TOptional<float>();
				})
				.OnValueChanged_Lambda([this](float NewValue)
				{
					if (Keyframe.IsValid())
					{
						Keyframe->LensSettings.FieldOfView = FMath::Clamp(NewValue, FCDGCameraLensSettings::FieldOfViewMin, FCDGCameraLensSettings::FieldOfViewMax);
						Keyframe->UpdateFocalLengthFromFOV();
						Keyframe->UpdateVisualizer();
						
						// Sync viewport camera to reflect FOV change
						UWorld* World = Keyframe->GetWorld();
						UCDGEditorState* EditorState = World ? World->GetSubsystem<UCDGEditorState>() : nullptr;
						if (EditorState)
						{
							EditorState->UpdateViewportFromKeyframe();
						}
						
						GEditor->RedrawLevelEditingViewports();
					}
				})
				.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetFOV", "Set Field of View"));
						Keyframe->Modify();
						Keyframe->LensSettings.FieldOfView = FMath::Clamp(NewValue, FCDGCameraLensSettings::FieldOfViewMin, FCDGCameraLensSettings::FieldOfViewMax);
						Keyframe->UpdateFocalLengthFromFOV();
						Keyframe->UpdateVisualizer();
					}
				})
			]
		]

		// Aperture
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.MinDesiredWidth(140.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ApertureLabel", "Aperture (f-stop):"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.MinValue(FCDGCameraLensSettings::ApertureMin)
				.MaxValue(FCDGCameraLensSettings::ApertureMax)
				.MinSliderValue(FCDGCameraLensSettings::ApertureMin)
				.MaxSliderValue(FCDGCameraLensSettings::ApertureMax)
				.Delta(0.1f)
				.Value_Lambda([this]() -> TOptional<float>
				{
					if (Keyframe.IsValid())
					{
						return Keyframe->LensSettings.Aperture;
					}
					return TOptional<float>();
				})
				.OnValueChanged_Lambda([this](float NewValue)
				{
					if (Keyframe.IsValid())
					{
						Keyframe->LensSettings.Aperture = FMath::Clamp(NewValue, FCDGCameraLensSettings::ApertureMin, FCDGCameraLensSettings::ApertureMax);
					}
				})
				.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetAperture", "Set Aperture"));
						Keyframe->Modify();
						Keyframe->LensSettings.Aperture = FMath::Clamp(NewValue, FCDGCameraLensSettings::ApertureMin, FCDGCameraLensSettings::ApertureMax);
					}
				})
			]
		]

		// Focus Distance
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.MinDesiredWidth(140.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FocusDistanceLabel", "Focus Distance (cm):"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.MinValue(FCDGCameraLensSettings::FocusDistanceMin)
				.MaxValue(FCDGCameraLensSettings::FocusDistanceMax)
				.MinSliderValue(FCDGCameraLensSettings::FocusDistanceSliderMin)
				.MaxSliderValue(FCDGCameraLensSettings::FocusDistanceSliderMax)
				.Delta(100.0f)
				.Value_Lambda([this]() -> TOptional<float>
				{
					if (Keyframe.IsValid())
					{
						return Keyframe->LensSettings.FocusDistance;
					}
					return TOptional<float>();
				})
				.OnValueChanged_Lambda([this](float NewValue)
				{
					if (Keyframe.IsValid())
					{
						Keyframe->LensSettings.FocusDistance = FMath::Max(FCDGCameraLensSettings::FocusDistanceMin, NewValue);
					}
				})
				.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetFocusDistance", "Set Focus Distance"));
						Keyframe->Modify();
						Keyframe->LensSettings.FocusDistance = FMath::Max(FCDGCameraLensSettings::FocusDistanceMin, NewValue);
					}
				})
			]
		]

		// Diaphragm Blade Count
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.MinDesiredWidth(140.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DiaphragmBladeCountLabel", "Diaphragm Blades:"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<int32>)
				.AllowSpin(true)
				.MinValue(FCDGCameraLensSettings::DiaphragmBladeCountMin)
				.MaxValue(FCDGCameraLensSettings::DiaphragmBladeCountMax)
				.Delta(1)
				.Value_Lambda([this]() -> TOptional<int32>
				{
					if (Keyframe.IsValid())
					{
						return Keyframe->LensSettings.DiaphragmBladeCount;
					}
					return TOptional<int32>();
				})
				.OnValueChanged_Lambda([this](int32 NewValue)
				{
					if (Keyframe.IsValid())
					{
						Keyframe->LensSettings.DiaphragmBladeCount = FMath::Clamp(NewValue, FCDGCameraLensSettings::DiaphragmBladeCountMin, FCDGCameraLensSettings::DiaphragmBladeCountMax);
					}
				})
				.OnValueCommitted_Lambda([this](int32 NewValue, ETextCommit::Type CommitType)
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetDiaphragmBladeCount", "Set Diaphragm Blade Count"));
						Keyframe->Modify();
						Keyframe->LensSettings.DiaphragmBladeCount = FMath::Clamp(NewValue, FCDGCameraLensSettings::DiaphragmBladeCountMin, FCDGCameraLensSettings::DiaphragmBladeCountMax);
					}
				})
			]
		]

		// ==================== FILMBACK PROPERTIES ====================

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 12, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FilmbackPropertiesHeader", "Filmback Settings"))
			.Font(FAppStyle::GetFontStyle("SmallFontBold"))
		]

		// Sensor Width
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.MinDesiredWidth(140.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SensorWidthLabel", "Sensor Width (mm):"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.MinValue(FCDGCameraFilmbackSettings::SensorWidthMin)
				.MaxValue(FCDGCameraFilmbackSettings::SensorWidthMax)
				.Delta(0.1f)
				.Value_Lambda([this]() -> TOptional<float>
				{
					if (Keyframe.IsValid())
					{
						return Keyframe->FilmbackSettings.SensorWidth;
					}
					return TOptional<float>();
				})
				.OnValueChanged_Lambda([this](float NewValue)
				{
					if (Keyframe.IsValid())
					{
						Keyframe->FilmbackSettings.SensorWidth = FMath::Max(FCDGCameraFilmbackSettings::SensorWidthMin, NewValue);
						if (Keyframe->FilmbackSettings.SensorAspectRatio > 0.0f)
						{
							Keyframe->FilmbackSettings.SensorHeight = Keyframe->FilmbackSettings.SensorWidth / Keyframe->FilmbackSettings.SensorAspectRatio;
						}
						Keyframe->UpdateVisualizer();
						GEditor->RedrawLevelEditingViewports();
					}
				})
				.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetSensorWidth", "Set Sensor Width"));
						Keyframe->Modify();
						Keyframe->FilmbackSettings.SensorWidth = FMath::Max(FCDGCameraFilmbackSettings::SensorWidthMin, NewValue);
						if (Keyframe->FilmbackSettings.SensorAspectRatio > 0.0f)
						{
							Keyframe->FilmbackSettings.SensorHeight = Keyframe->FilmbackSettings.SensorWidth / Keyframe->FilmbackSettings.SensorAspectRatio;
						}
						Keyframe->UpdateVisualizer();
					}
				})
			]
		]

		// Sensor Height (Read-only display)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.MinDesiredWidth(140.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SensorHeightLabel", "Sensor Height (mm):"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (Keyframe.IsValid())
					{
						return FText::AsNumber(Keyframe->FilmbackSettings.SensorHeight);
					}
					return FText::FromString(TEXT("0.0"));
				})
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.ToolTipText(LOCTEXT("SensorHeightTooltip", "Calculated from Sensor Width / Aspect Ratio (read-only)"))
			]
		]

		// Sensor Aspect Ratio
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.MinDesiredWidth(140.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SensorAspectRatioLabel", "Sensor Aspect Ratio:"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.MinValue(FCDGCameraFilmbackSettings::SensorAspectRatioMin)
				.MaxValue(FCDGCameraFilmbackSettings::SensorAspectRatioMax)
				.Delta(0.01f)
				.Value_Lambda([this]() -> TOptional<float>
				{
					if (Keyframe.IsValid())
					{
						return Keyframe->FilmbackSettings.SensorAspectRatio;
					}
					return TOptional<float>();
				})
				.OnValueChanged_Lambda([this](float NewValue)
				{
					if (Keyframe.IsValid())
					{
						Keyframe->FilmbackSettings.SensorAspectRatio = FMath::Max(FCDGCameraFilmbackSettings::SensorAspectRatioMin, NewValue);
						if (Keyframe->FilmbackSettings.SensorAspectRatio > 0.0f)
						{
							Keyframe->FilmbackSettings.SensorHeight = Keyframe->FilmbackSettings.SensorWidth / Keyframe->FilmbackSettings.SensorAspectRatio;
						}
						Keyframe->UpdateVisualizer();
						GEditor->RedrawLevelEditingViewports();
					}
				})
				.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetSensorAspectRatio", "Set Sensor Aspect Ratio"));
						Keyframe->Modify();
						Keyframe->FilmbackSettings.SensorAspectRatio = FMath::Max(FCDGCameraFilmbackSettings::SensorAspectRatioMin, NewValue);
						if (Keyframe->FilmbackSettings.SensorAspectRatio > 0.0f)
						{
							Keyframe->FilmbackSettings.SensorHeight = Keyframe->FilmbackSettings.SensorWidth / Keyframe->FilmbackSettings.SensorAspectRatio;
						}
						Keyframe->UpdateVisualizer();
					}
				})
			]
		]

		// ==================== QUICK PRESETS ====================

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 12, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("QuickPresetsHeader", "Quick Presets"))
			.Font(FAppStyle::GetFontStyle("SmallFontBold"))
		]

		// Preset buttons row 1
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("WideAnglePreset", "Wide (24mm)"))
				.ToolTipText(LOCTEXT("WideAnglePresetTooltip", "Set focal length to 24mm"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]()
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetWideAngle", "Set Wide Angle"));
						Keyframe->Modify();
						Keyframe->LensSettings.FocalLength = 24.0f;
						Keyframe->UpdateFOVFromFocalLength();
						Keyframe->UpdateVisualizer();
						
						// Sync viewport camera to reflect FOV change
						UWorld* World = Keyframe->GetWorld();
						UCDGEditorState* EditorState = World ? World->GetSubsystem<UCDGEditorState>() : nullptr;
						if (EditorState)
						{
							EditorState->UpdateViewportFromKeyframe();
						}
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("NormalPreset", "Normal (35mm)"))
				.ToolTipText(LOCTEXT("NormalPresetTooltip", "Set focal length to 35mm"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]()
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetNormal", "Set Normal FOV"));
						Keyframe->Modify();
						Keyframe->LensSettings.FocalLength = 35.0f;
						Keyframe->UpdateFOVFromFocalLength();
						Keyframe->UpdateVisualizer();
						
						// Sync viewport camera to reflect FOV change
						UWorld* World = Keyframe->GetWorld();
						UCDGEditorState* EditorState = World ? World->GetSubsystem<UCDGEditorState>() : nullptr;
						if (EditorState)
						{
							EditorState->UpdateViewportFromKeyframe();
						}
					}
					return FReply::Handled();
				})
			]
		]

		// Preset buttons row 2
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("PortraitPreset", "Portrait (50mm)"))
				.ToolTipText(LOCTEXT("PortraitPresetTooltip", "Set focal length to 50mm"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]()
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetPortrait", "Set Portrait"));
						Keyframe->Modify();
						Keyframe->LensSettings.FocalLength = 50.0f;
						Keyframe->UpdateFOVFromFocalLength();
						Keyframe->UpdateVisualizer();
						
						// Sync viewport camera to reflect FOV change
						UWorld* World = Keyframe->GetWorld();
						UCDGEditorState* EditorState = World ? World->GetSubsystem<UCDGEditorState>() : nullptr;
						if (EditorState)
						{
							EditorState->UpdateViewportFromKeyframe();
						}
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("TelephotoPreset", "Telephoto (85mm)"))
				.ToolTipText(LOCTEXT("TelephotoPresetTooltip", "Set focal length to 85mm"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]()
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetTelephoto", "Set Telephoto"));
						Keyframe->Modify();
						Keyframe->LensSettings.FocalLength = 85.0f;
						Keyframe->UpdateFOVFromFocalLength();
						Keyframe->UpdateVisualizer();
						
						// Sync viewport camera to reflect FOV change
						UWorld* World = Keyframe->GetWorld();
						UCDGEditorState* EditorState = World ? World->GetSubsystem<UCDGEditorState>() : nullptr;
						if (EditorState)
						{
							EditorState->UpdateViewportFromKeyframe();
						}
					}
					return FReply::Handled();
				})
			]
		]

		// ==================== DEPTH OF FIELD PRESETS ====================

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 12, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("DepthOfFieldHeader", "Depth of Field Presets"))
			.Font(FAppStyle::GetFontStyle("SmallFontBold"))
		]

		// DOF buttons
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ShallowDOF", "Shallow (f/1.4)"))
				.ToolTipText(LOCTEXT("ShallowDOFTooltip", "Set aperture to f/1.4"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]()
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetShallowDOF", "Set Shallow DOF"));
						Keyframe->Modify();
						Keyframe->LensSettings.Aperture = 1.4f;
						Keyframe->UpdateVisualizer();
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("MediumDOF", "Medium (f/2.8)"))
				.ToolTipText(LOCTEXT("MediumDOFTooltip", "Set aperture to f/2.8"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]()
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetMediumDOF", "Set Medium DOF"));
						Keyframe->Modify();
						Keyframe->LensSettings.Aperture = 2.8f;
						Keyframe->UpdateVisualizer();
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("DeepDOF", "Deep (f/8)"))
				.ToolTipText(LOCTEXT("DeepDOFTooltip", "Set aperture to f/8"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]()
				{
					if (Keyframe.IsValid())
					{
						const FScopedTransaction Transaction(LOCTEXT("SetDeepDOF", "Set Deep DOF"));
						Keyframe->Modify();
						Keyframe->LensSettings.Aperture = 8.0f;
						Keyframe->UpdateVisualizer();
					}
					return FReply::Handled();
				})
			]
		]

		// ==================== EXIT PREVIEW ====================

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 16, 0, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("ExitPreview", "Exit Preview"))
			.ToolTipText(LOCTEXT("ExitPreviewTooltip", "Exit camera preview mode and return to normal editing"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.OnClicked_Lambda([this]()
			{
				if (Keyframe.IsValid())
				{
					UWorld* World = Keyframe->GetWorld();
					UCDGEditorState* EditorState = World ? World->GetSubsystem<UCDGEditorState>() : nullptr;
					if (EditorState)
					{
						EditorState->ExitPreview();
					}
				}
				return FReply::Handled();
			})
		];
}

// ==================== FCDGCameraPreviewContextMenu ====================

void FCDGCameraPreviewContextMenu::Initialize()
{
	UE_LOG(LogTemp, Log, TEXT("CDGCameraPreviewContextMenu: Initializing"));
	SubscribeToEditorState();
}

void FCDGCameraPreviewContextMenu::Shutdown()
{
	UE_LOG(LogTemp, Log, TEXT("CDGCameraPreviewContextMenu: Shutting down"));
	UnsubscribeFromEditorState();
	HideMenu();
}

void FCDGCameraPreviewContextMenu::ShowMenu(ACDGKeyframe* Keyframe)
{
	if (!Keyframe)
	{
		UE_LOG(LogTemp, Warning, TEXT("CDGCameraPreviewContextMenu: Cannot show menu with null keyframe"));
		return;
	}

	// Hide existing menu if any
	HideMenu();

	// Reset the closing flag for the new menu
	bIsClosing = false;

	// Create menu widget
	MenuWidget = SNew(SCDGCameraPreviewContextMenu, Keyframe);

	// Create popup window
	MenuWindow = SNew(SWindow)
		.Title(LOCTEXT("CameraPreviewMenuTitle", "Camera Preview"))
		.ClientSize(FVector2D(450.0f, 650.0f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.IsTopmostWindow(true)
		.SizingRule(ESizingRule::UserSized)
		[
			MenuWidget.ToSharedRef()
		];

	// Set window close handler - when user closes the window, exit camera preview mode
	MenuWindow->SetOnWindowClosed(FOnWindowClosed::CreateSP(this, &FCDGCameraPreviewContextMenu::OnWindowClosed));

	// Add window to viewport
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().AddWindow(MenuWindow.ToSharedRef());
		UE_LOG(LogTemp, Log, TEXT("CDGCameraPreviewContextMenu: Menu shown for keyframe '%s'"), 
			*Keyframe->GetActorLabel());
	}
}

void FCDGCameraPreviewContextMenu::HideMenu()
{
	// Prevent re-entry during window destruction
	if (bIsClosing)
	{
		return;
	}

	bIsClosing = true;

	if (MenuWindow.IsValid())
	{
		MenuWindow->RequestDestroyWindow();
		MenuWindow.Reset();
	}

	MenuWidget.Reset();
	
	bIsClosing = false;
	
	UE_LOG(LogTemp, Log, TEXT("CDGCameraPreviewContextMenu: Menu hidden"));
}

bool FCDGCameraPreviewContextMenu::IsMenuVisible() const
{
	return MenuWindow.IsValid() && MenuWidget.IsValid();
}

void FCDGCameraPreviewContextMenu::SubscribeToEditorState()
{
	// Start polling timer to check editor state
	if (GEditor)
	{
		GEditor->GetTimerManager()->SetTimer(
			StateCheckTimerHandle,
			FTimerDelegate::CreateSP(this, &FCDGCameraPreviewContextMenu::CheckEditorState),
			0.1f, // Check every 100ms
			true
		);
		UE_LOG(LogTemp, Log, TEXT("CDGCameraPreviewContextMenu: Subscribed to editor state"));
	}
}

void FCDGCameraPreviewContextMenu::UnsubscribeFromEditorState()
{
	if (GEditor && StateCheckTimerHandle.IsValid())
	{
		GEditor->GetTimerManager()->ClearTimer(StateCheckTimerHandle);
		StateCheckTimerHandle.Invalidate();
		UE_LOG(LogTemp, Log, TEXT("CDGCameraPreviewContextMenu: Unsubscribed from editor state"));
	}
}

void FCDGCameraPreviewContextMenu::CheckEditorState()
{
	// Get the editor state from the editor world
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!EditorWorld)
	{
		return;
	}

	UCDGEditorState* CurrentEditorState = EditorWorld->GetSubsystem<UCDGEditorState>();
	if (!CurrentEditorState)
	{
		return;
	}

	EditorState = CurrentEditorState;

	// Check if we should show or hide the menu based on editor state
	if (CurrentEditorState->IsPreviewingCamera())
	{
		// Get the previewed keyframe
		if (CurrentEditorState->GetCurrentState() == ECDGEditorPreviewState::PREVIEW_CAMERA)
		{
			// We need access to the previewed keyframe - for now, check if menu should be visible
			if (!IsMenuVisible())
			{
				// Menu should be visible but isn't - the actual keyframe will be passed when EnterCameraPreview is called
				// This is handled by the direct integration with CDGEditorState
			}
		}
	}
	else
	{
		// Not in camera preview mode - hide menu if visible
		if (IsMenuVisible())
		{
			HideMenu();
		}
	}
}

void FCDGCameraPreviewContextMenu::OnWindowClosed(const TSharedRef<SWindow>& Window)
{
	// Prevent re-entry during window destruction
	if (bIsClosing)
	{
		return;
	}

	bIsClosing = true;

	// Get the keyframe from the menu widget
	ACDGKeyframe* Keyframe = MenuWidget.IsValid() ? MenuWidget->GetKeyframe() : nullptr;
	
	if (Keyframe)
	{
		UWorld* World = Keyframe->GetWorld();
		UCDGEditorState* CurrentEditorState = World ? World->GetSubsystem<UCDGEditorState>() : nullptr;
		if (CurrentEditorState && CurrentEditorState->IsPreviewingCamera())
		{
			CurrentEditorState->ExitPreview();
			UE_LOG(LogTemp, Log, TEXT("CDGCameraPreviewContextMenu: Window closed by user, exiting camera preview"));
		}
	}

	// Clean up references without calling RequestDestroyWindow again
	MenuWindow.Reset();
	MenuWidget.Reset();

	bIsClosing = false;
}

#undef LOCTEXT_NAMESPACE

