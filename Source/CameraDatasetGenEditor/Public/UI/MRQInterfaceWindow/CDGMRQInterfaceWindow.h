// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "MRQInterface/CDGMRQInterface.h"

class ULevelSequence;
class ACDGTrajectory;

/**
 * Window for rendering trajectories via Movie Render Queue
 */
class SMRQInterfaceWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMRQInterfaceWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Button handlers */
	FReply OnCancelClicked();
	FReply OnBrowseOutputDirClicked();
	FReply OnRenderClicked();
	
	/** Validation */
	bool IsRenderButtonEnabled() const;
	FText GetValidationMessage() const;
	
	/** Level Sequence Selection */
	TSharedRef<SWidget> MakeSequencePickerWidget();
	void OnSequenceSelected(const FAssetData& AssetData);
	FString GetSelectedSequencePath() const;
	
	/** Output format combo box */
	TSharedRef<SWidget> MakeOutputFormatWidget(TSharedPtr<ECDGRenderOutputFormat> InItem);
	FText GetOutputFormatText() const;
	void OnOutputFormatChanged(TSharedPtr<ECDGRenderOutputFormat> NewFormat, ESelectInfo::Type SelectInfo);
	
	/** Validate the selected sequence against trajectories */
	bool ValidateSequence();
	
	/** Check if FFmpeg is available for video encoding */
	bool IsFFmpegAvailable() const;
	
	/** Check if current format requires FFmpeg */
	bool DoesFormatRequireFFmpeg() const;

private:
	/** Selected Level Sequence */
	TWeakObjectPtr<ULevelSequence> SelectedSequence;
	
	/** Validation status */
	bool bIsSequenceValid = false;
	FString ValidationErrorMessage;
	
	/** Render Configuration Widgets */
	TSharedPtr<SEditableTextBox> OutputDirTextBox;
	TSharedPtr<SSpinBox<int32>> ResolutionWidthInput;
	TSharedPtr<SSpinBox<int32>> ResolutionHeightInput;
	TSharedPtr<SSpinBox<int32>> FramerateInput;
	TSharedPtr<SComboBox<TSharedPtr<ECDGRenderOutputFormat>>> OutputFormatComboBox;
	TSharedPtr<SCheckBox> ExportIndexJSONCheckBox;
	TSharedPtr<SCheckBox> OverwriteExistingCheckBox;
	TSharedPtr<SSpinBox<int32>> SpatialSampleCountInput;
	TSharedPtr<SSpinBox<int32>> TemporalSampleCountInput;
	
	/** Status/Validation text */
	TSharedPtr<STextBlock> ValidationText;
	
	/** Output format options */
	TArray<TSharedPtr<ECDGRenderOutputFormat>> OutputFormatOptions;
	TSharedPtr<ECDGRenderOutputFormat> SelectedOutputFormat;
};

/**
 * Utility to open the MRQ Interface window
 */
class CDGMRQInterfaceWindow
{
public:
	/** Open the MRQ Interface window */
	static void OpenWindow();
};
