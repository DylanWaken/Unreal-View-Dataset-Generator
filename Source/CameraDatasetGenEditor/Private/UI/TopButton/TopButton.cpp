// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TopButton/TopButton.h"
#include "UI/TopButton/TopButtonStyle.h"
#include "ToolMenus.h"
#include "LogCameraDatasetGenEditor.h"

#define LOCTEXT_NAMESPACE "TopButton"

FTopButton::FTopButton(const FString& IconStyleName)
	: CurrentIconStyleName(IconStyleName)
{
	ExtendLevelEditorToolbar();
}

FTopButton::~FTopButton()
{
	// Cleanup is handled automatically by UToolMenus
}

void FTopButton::ExtendLevelEditorToolbar()
{
	// Extend the PlayToolBar menu to add our button after the play controls
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	if (!Menu)
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Failed to extend PlayToolBar menu!"));
		return;
	}
	
	// Add our button in a new section
	FToolMenuSection& Section = Menu->AddSection("CameraDatasetGen", FText(), FToolMenuInsert("Play", EToolMenuInsertType::After));
	
	FString IconName = FString::Printf(TEXT("TopButton.%s"), *CurrentIconStyleName);
	
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		"CameraDatasetGenButton",
		FUIAction(FExecuteAction::CreateRaw(this, &FTopButton::OnButtonClicked)),
		LOCTEXT("TopButton_Label", "Camera Dataset"),
		LOCTEXT("TopButton_Tooltip", "Open Camera Dataset Generator"),
		FSlateIcon(FTopButtonStyle::GetStyleSetName(), *IconName)
	));
}

void FTopButton::OnButtonClicked()
{

}

#undef LOCTEXT_NAMESPACE

