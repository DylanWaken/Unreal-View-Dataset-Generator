// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MRQInterfaceWindow/CDGMRQInterfaceWindow.h"
#include "Trajectory/CDGTrajectory.h"
#include "MRQInterface/CDGMRQInterface.h"
#include "LogCameraDatasetGenEditor.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "PropertyCustomizationHelpers.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "CDGMRQInterfaceWindow"

void SMRQInterfaceWindow::Construct(const FArguments& InArgs)
{
	// Initialize output format options
	OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::PNG_Sequence));
	OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::EXR_Sequence));
	OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::BMP_Sequence));
	OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::H264_Video));
	OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::WAV_Audio));
	OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::CommandLineEncoder));
	OutputFormatOptions.Add(MakeShared<ECDGRenderOutputFormat>(ECDGRenderOutputFormat::FinalCutProXML));
	
	SelectedOutputFormat = OutputFormatOptions[0]; // Default to PNG

	// Default output directory
	FString DefaultOutputDir = FPaths::ProjectSavedDir() / TEXT("MovieRenders");

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10.0f))
		[
			SNew(SVerticalBox)
			
			// Title
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 10)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WindowTitle", "Movie Render Queue Interface"))
				.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
			]
			
			// Level Sequence Selection
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 10, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LevelSequenceLabel", "Level Sequence:"))
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					MakeSequencePickerWidget()
				]
			]
			
			// Validation Message
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor_Lambda([this]() -> FSlateColor
				{
					// Show orange/yellow for FFmpeg warning, red for other errors, green for success
					if (!bIsSequenceValid)
					{
						if (DoesFormatRequireFFmpeg() && !IsFFmpegAvailable())
						{
							return FSlateColor(FLinearColor(1.0f, 0.5f, 0.0f, 0.3f)); // Orange warning
						}
						return FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.3f)); // Red error
					}
					return FSlateColor(FLinearColor(0.0f, 1.0f, 0.0f, 0.2f)); // Green success
				})
				.Padding(FMargin(10.0f, 8.0f))
				[
					SAssignNew(ValidationText, STextBlock)
					.Text(this, &SMRQInterfaceWindow::GetValidationMessage)
					.AutoWrapText(true)
					.ColorAndOpacity_Lambda([this]() -> FSlateColor
					{
						// Show orange for FFmpeg warning, red for other errors, green for success
						if (!bIsSequenceValid)
						{
							if (DoesFormatRequireFFmpeg() && !IsFFmpegAvailable())
							{
								return FSlateColor(FLinearColor(1.0f, 0.6f, 0.0f)); // Orange warning text
							}
							return FSlateColor(FLinearColor::Red);
						}
						return FSlateColor(FLinearColor::Green);
					})
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				]
			]
			
			// Separator
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 10)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Separator"))
				.Padding(FMargin(0, 2))
			]
			
			// Output Settings Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OutputSettingsTitle", "Output Settings"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
			]
			
			// Output Directory
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 10, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OutputDirLabel", "Output Directory:"))
					.MinDesiredWidth(120.0f)
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
					.OnClicked(this, &SMRQInterfaceWindow::OnBrowseOutputDirClicked)
				]
			]
			
			// Output Resolution
			+ SVerticalBox::Slot()
			.AutoHeight()
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
					.MinDesiredWidth(120.0f)
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
			
			// Output Framerate
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 10, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FramerateLabel", "Framerate (FPS):"))
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(FramerateInput, SSpinBox<int32>)
					.MinValue(1)
					.MaxValue(240)
					.Value(30)
					.MinSliderValue(1)
					.MaxSliderValue(120)
					.MinDesiredWidth(100.0f)
				]
			]
			
			// Output Format
			+ SVerticalBox::Slot()
			.AutoHeight()
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
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(OutputFormatComboBox, SComboBox<TSharedPtr<ECDGRenderOutputFormat>>)
					.OptionsSource(&OutputFormatOptions)
					.OnGenerateWidget(this, &SMRQInterfaceWindow::MakeOutputFormatWidget)
					.OnSelectionChanged(this, &SMRQInterfaceWindow::OnOutputFormatChanged)
					.InitiallySelectedItem(SelectedOutputFormat)
					[
						SNew(STextBlock)
						.Text(this, &SMRQInterfaceWindow::GetOutputFormatText)
					]
				]
			]
			
			// Export Index JSON Checkbox
			+ SVerticalBox::Slot()
			.AutoHeight()
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
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(ExportIndexJSONCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
				]
			]
			
			// Overwrite Existing Checkbox
			+ SVerticalBox::Slot()
			.AutoHeight()
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
					.MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(OverwriteExistingCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Unchecked)
				]
			]
			
			// Separator
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 10)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Separator"))
				.Padding(FMargin(0, 2))
			]
			
			// Quality Settings Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("QualitySettingsTitle", "Quality Settings"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
			]
			
			// Spatial Sample Count
			+ SVerticalBox::Slot()
			.AutoHeight()
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
					.MinDesiredWidth(120.0f)
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
			
			// Temporal Sample Count
			+ SVerticalBox::Slot()
			.AutoHeight()
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
					.MinDesiredWidth(120.0f)
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
			
			// Spacer
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBox)
			]
			
			// Bottom Buttons
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
					.OnClicked(this, &SMRQInterfaceWindow::OnCancelClicked)
				]
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("RenderButton", "Render"))
					.OnClicked(this, &SMRQInterfaceWindow::OnRenderClicked)
					.IsEnabled(this, &SMRQInterfaceWindow::IsRenderButtonEnabled)
					.ToolTipText(this, &SMRQInterfaceWindow::GetValidationMessage)
				]
			]
		]
	];
}

TSharedRef<SWidget> SMRQInterfaceWindow::MakeSequencePickerWidget()
{
	return SNew(SObjectPropertyEntryBox)
		.AllowedClass(ULevelSequence::StaticClass())
		.ObjectPath(this, &SMRQInterfaceWindow::GetSelectedSequencePath)
		.OnObjectChanged(this, &SMRQInterfaceWindow::OnSequenceSelected)
		.AllowClear(true)
		.DisplayUseSelected(true)
		.DisplayBrowse(true);
}

void SMRQInterfaceWindow::OnSequenceSelected(const FAssetData& AssetData)
{
	SelectedSequence = Cast<ULevelSequence>(AssetData.GetAsset());
	bIsSequenceValid = ValidateSequence();
}

FString SMRQInterfaceWindow::GetSelectedSequencePath() const
{
	if (SelectedSequence.IsValid())
	{
		return SelectedSequence->GetPathName();
	}
	return FString();
}

TSharedRef<SWidget> SMRQInterfaceWindow::MakeOutputFormatWidget(TSharedPtr<ECDGRenderOutputFormat> InItem)
{
	FText FormatText;
	switch (*InItem)
	{
	case ECDGRenderOutputFormat::PNG_Sequence:
		FormatText = LOCTEXT("PNG_Sequence", "PNG Sequence [8bit]");
		break;
	case ECDGRenderOutputFormat::EXR_Sequence:
		FormatText = LOCTEXT("EXR_Sequence", "EXR Sequence [16bit]");
		break;
	case ECDGRenderOutputFormat::BMP_Sequence:
		FormatText = LOCTEXT("BMP_Sequence", "BMP Sequence [8bit]");
		break;
	case ECDGRenderOutputFormat::H264_Video:
		FormatText = LOCTEXT("H264_Video", "H.264 MP4 [8bit]");
		break;
	case ECDGRenderOutputFormat::WAV_Audio:
		FormatText = LOCTEXT("WAV_Audio", "WAV Audio");
		break;
	case ECDGRenderOutputFormat::CommandLineEncoder:
		FormatText = LOCTEXT("CommandLineEncoder", "Command Line Encoder");
		break;
	case ECDGRenderOutputFormat::FinalCutProXML:
		FormatText = LOCTEXT("FinalCutProXML", "Final Cut Pro XML");
		break;
	}
	
	return SNew(STextBlock).Text(FormatText);
}

FText SMRQInterfaceWindow::GetOutputFormatText() const
{
	if (SelectedOutputFormat.IsValid())
	{
		switch (*SelectedOutputFormat)
		{
		case ECDGRenderOutputFormat::PNG_Sequence:
			return LOCTEXT("PNG_Sequence", "PNG Sequence [8bit]");
		case ECDGRenderOutputFormat::EXR_Sequence:
			return LOCTEXT("EXR_Sequence", "EXR Sequence [16bit]");
		case ECDGRenderOutputFormat::BMP_Sequence:
			return LOCTEXT("BMP_Sequence", "BMP Sequence [8bit]");
		case ECDGRenderOutputFormat::H264_Video:
			return LOCTEXT("H264_Video", "H.264 MP4 [8bit]");
		case ECDGRenderOutputFormat::WAV_Audio:
			return LOCTEXT("WAV_Audio", "WAV Audio");
		case ECDGRenderOutputFormat::CommandLineEncoder:
			return LOCTEXT("CommandLineEncoder", "Command Line Encoder");
		case ECDGRenderOutputFormat::FinalCutProXML:
			return LOCTEXT("FinalCutProXML", "Final Cut Pro XML");
		}
	}
	return LOCTEXT("PNG_Sequence", "PNG Sequence [8bit]");
}

void SMRQInterfaceWindow::OnOutputFormatChanged(TSharedPtr<ECDGRenderOutputFormat> NewFormat, ESelectInfo::Type SelectInfo)
{
	SelectedOutputFormat = NewFormat;
	
	// Re-validate when format changes to update FFmpeg warning
	if (SelectedSequence.IsValid())
	{
		bIsSequenceValid = ValidateSequence();
	}
}

bool SMRQInterfaceWindow::ValidateSequence()
{
	if (!SelectedSequence.IsValid())
	{
		ValidationErrorMessage = TEXT("No level sequence selected");
		return false;
	}
	
	// Check FFmpeg availability for video formats
	if (DoesFormatRequireFFmpeg() && !IsFFmpegAvailable())
	{
		ValidationErrorMessage = TEXT("⚠️ FFmpeg not found - MP4 video encoding unavailable\n\n")
			TEXT("HOW TO ENABLE VIDEO ENCODING:\n")
			TEXT("1. Download FFmpeg from: https://github.com/BtbN/FFmpeg-Builds/releases/latest\n")
			TEXT("2. Download: ffmpeg-master-latest-win64-gpl.zip\n")
			TEXT("3. Extract the zip completely (includes bin folder with ffmpeg.exe)\n")
			TEXT("4. Copy the entire extracted folder to: Engine/Binaries/ThirdParty/FFmpeg/Win64/\n")
			TEXT("   Final path should be: Engine/Binaries/ThirdParty/FFmpeg/Win64/bin/ffmpeg.exe\n")
			TEXT("5. Restart Unreal Editor\n\n")
			TEXT("PNG image sequence will be rendered instead of MP4 video.");
		return false;
	}
	
	// Get all trajectories in the world
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
#endif
	
	if (!World)
	{
		ValidationErrorMessage = TEXT("No valid world context found");
		return false;
	}
	
	TArray<ACDGTrajectory*> Trajectories;
	for (TActorIterator<ACDGTrajectory> It(World); It; ++It)
	{
		Trajectories.Add(*It);
	}
	
	if (Trajectories.Num() == 0)
	{
		ValidationErrorMessage = TEXT("No trajectories found in the world");
		return false;
	}
	
	// Get level name
	FString LevelName = World->GetMapName();
	LevelName.RemoveFromStart(World->StreamingLevelsPrefix);
	
	// Validate the master sequence
	bool bValid = CDGMRQInterface::Internal::ValidateMasterSequence(SelectedSequence.Get(), Trajectories, LevelName);
	
	if (!bValid)
	{
		ValidationErrorMessage = TEXT("Level sequence validation failed. Please ensure:\n")
			TEXT("1. All trajectories have corresponding shot sequences\n")
			TEXT("2. Shot sequences match trajectory data (duration, keyframes)\n")
			TEXT("3. Re-export the level sequence if trajectories have changed");
		return false;
	}
	
	ValidationErrorMessage = TEXT("✓ Validation passed - Ready to render");
	return true;
}

bool SMRQInterfaceWindow::IsFFmpegAvailable() const
{
	// Check multiple possible FFmpeg locations
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

bool SMRQInterfaceWindow::DoesFormatRequireFFmpeg() const
{
	if (!SelectedOutputFormat.IsValid())
	{
		return false;
	}
	
	return *SelectedOutputFormat == ECDGRenderOutputFormat::H264_Video ||
		   *SelectedOutputFormat == ECDGRenderOutputFormat::CommandLineEncoder;
}

bool SMRQInterfaceWindow::IsRenderButtonEnabled() const
{
	return bIsSequenceValid && SelectedSequence.IsValid();
}

FText SMRQInterfaceWindow::GetValidationMessage() const
{
	return FText::FromString(ValidationErrorMessage);
}

FReply SMRQInterfaceWindow::OnCancelClicked()
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (Window.IsValid())
	{
		Window->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SMRQInterfaceWindow::OnBrowseOutputDirClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}
	
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;
	
	FString SelectedFolder;
	bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
		ParentWindowHandle,
		TEXT("Select Output Directory"),
		OutputDirTextBox->GetText().ToString(),
		SelectedFolder
	);
	
	if (bFolderSelected)
	{
		OutputDirTextBox->SetText(FText::FromString(SelectedFolder));
	}
	
	return FReply::Handled();
}

FReply SMRQInterfaceWindow::OnRenderClicked()
{
	if (!SelectedSequence.IsValid())
	{
		return FReply::Handled();
	}
	
	// Build render configuration
	FTrajectoryRenderConfig Config;
	Config.DestinationRootDir = OutputDirTextBox->GetText().ToString();
	Config.OutputResolutionOverride = FIntPoint(ResolutionWidthInput->GetValue(), ResolutionHeightInput->GetValue());
	Config.OutputFramerateOverride = FramerateInput->GetValue();
	Config.ExportFormat = SelectedOutputFormat.IsValid() ? *SelectedOutputFormat : ECDGRenderOutputFormat::PNG_Sequence;
	Config.bExportIndexJSON = ExportIndexJSONCheckBox->IsChecked();
	Config.bOverwriteExistingOutput = OverwriteExistingCheckBox->IsChecked();
	Config.SpatialSampleCount = SpatialSampleCountInput->GetValue();
	Config.TemporalSampleCount = TemporalSampleCountInput->GetValue();
	
	// Get all trajectories
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
#endif
	
	if (!World)
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("No valid world context found"));
		return FReply::Handled();
	}
	
	TArray<ACDGTrajectory*> Trajectories;
	for (TActorIterator<ACDGTrajectory> It(World); It; ++It)
	{
		Trajectories.Add(*It);
	}
	
	// Call render function with the selected sequence
	bool bSuccess = CDGMRQInterface::RenderTrajectoriesWithSequence(SelectedSequence.Get(), Trajectories, Config);
	
	// Show notification
	FNotificationInfo Info(bSuccess
		? LOCTEXT("RenderStarted", "Movie Render Queue rendering started")
		: LOCTEXT("RenderFailed", "Failed to start rendering"));
	
	Info.ExpireDuration = 5.0f;
	Info.bUseLargeFont = false;
	Info.bUseSuccessFailIcons = true;
	
	FSlateNotificationManager::Get().AddNotification(Info);
	
	if (bSuccess)
	{
		// Close window on success
		OnCancelClicked();
	}
	
	return FReply::Handled();
}

void CDGMRQInterfaceWindow::OpenWindow()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("MRQInterfaceWindowTitle", "Movie Render Queue Interface"))
		.ClientSize(FVector2D(600, 700))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	Window->SetContent(
		SNew(SMRQInterfaceWindow)
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
