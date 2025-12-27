// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TopButton/TopButton.h"
#include "UI/TopButton/TopButtonStyle.h"
#include "UI/TopButtonDropdown/TopButtonDropdown.h"
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
	
	// Use InitComboButton with no primary action - clicking the button opens the dropdown
	Section.AddEntry(FToolMenuEntry::InitComboButton(
		"CameraDatasetGenButton",
		FUIAction(),  // Empty action - the button only opens the menu
		FOnGetContent::CreateRaw(this, &FTopButton::OnGetMenuContent),
		LOCTEXT("TopButton_Label", "Camera Dataset"),
		LOCTEXT("TopButton_Tooltip", "Camera Dataset Generator Actions"),
		FSlateIcon(FTopButtonStyle::GetStyleSetName(), *IconName),
		false  // bInSimpleComboBox = false means no separate dropdown arrow
	));
}

TSharedRef<SWidget> FTopButton::OnGetMenuContent()
{
	return FTopButtonDropdown::MakeDropdownMenu();
}

#undef LOCTEXT_NAMESPACE

