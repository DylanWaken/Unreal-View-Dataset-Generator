// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TopButtonDropdown/TopButtonDropdown.h"
#include "UI/LevelSeqExporterWindow/CDGLevelSeqExporter.h"
#include "Trajectory/CDGKeyframe.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "LogCameraDatasetGenEditor.h"
#include "LevelSequenceInterface/CDGLevelSeqSubsystem.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "TopButtonDropdown"

TSharedRef<SWidget> FTopButtonDropdown::MakeDropdownMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	// Add "Add New Keyframe" menu entry
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddNewKeyframe_Label", "Add New Keyframe"),
		LOCTEXT("AddNewKeyframe_Tooltip", "Spawn a new CDGKeyframe actor at the viewport camera location"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
		FUIAction(FExecuteAction::CreateStatic(&FTopButtonDropdown::OnAddNewKeyframe))
	);

	// Add "Initialize Level Sequence" menu entry
	MenuBuilder.AddMenuEntry(
		LOCTEXT("InitLevelSequence_Label", "Initialize Level Sequence"),
		LOCTEXT("InitLevelSequence_Tooltip", "Create or load the CDG_<LevelName>_SEQ Level Sequence asset"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
		FUIAction(FExecuteAction::CreateStatic(&FTopButtonDropdown::OnInitLevelSequence))
	);

	// Add "Delete Level Sequence" menu entry
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeleteLevelSequence_Label", "Delete Level Sequence"),
		LOCTEXT("DeleteLevelSequence_Tooltip", "Delete the CDG_<LevelName>_SEQ Level Sequence asset"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
		FUIAction(FExecuteAction::CreateStatic(&FTopButtonDropdown::OnDeleteLevelSequence))
	);

	// Add "Export to Level Sequence" menu entry
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ExportToLevelSequence_Label", "Export to Level Sequence"),
		LOCTEXT("ExportToLevelSequence_Tooltip", "Open window to export trajectories to the Level Sequence"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Share"),
		FUIAction(FExecuteAction::CreateStatic(&FTopButtonDropdown::OnExportToLevelSequence))
	);

	return MenuBuilder.MakeWidget();
}

void FTopButtonDropdown::OnAddNewKeyframe()
{
	// Get the editor world
	if (!GEditor)
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("GEditor is null, cannot spawn keyframe"));
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Editor world is null, cannot spawn keyframe"));
		return;
	}

	// Get the active viewport camera location and rotation
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;

	// Try to get the active viewport's camera transform
	if (GCurrentLevelEditingViewportClient)
	{
		SpawnLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
		SpawnRotation = GCurrentLevelEditingViewportClient->GetViewRotation();
	}

	// Set up spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Name = MakeUniqueObjectName(World, ACDGKeyframe::StaticClass(), FName(TEXT("CDGKeyframe")));
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	// Spawn the keyframe actor
	ACDGKeyframe* NewKeyframe = World->SpawnActor<ACDGKeyframe>(ACDGKeyframe::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	
	if (NewKeyframe)
	{
		UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("Successfully spawned CDGKeyframe: %s at location %s"), 
			*NewKeyframe->GetName(), *SpawnLocation.ToString());
		
		// Select the newly spawned keyframe in the editor
		if (GEditor)
		{
			GEditor->SelectNone(false, true);
			GEditor->SelectActor(NewKeyframe, true, true);
		}
	}
	else
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Failed to spawn CDGKeyframe actor"));
	}
}

void FTopButtonDropdown::OnInitLevelSequence()
{
	if (!GEditor) return;
	
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return;

	if (UCDGLevelSeqSubsystem* SeqSubsystem = World->GetSubsystem<UCDGLevelSeqSubsystem>())
	{
		SeqSubsystem->InitLevelSequence();
	}
	else
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Failed to get CDGLevelSeqSubsystem"));
	}
}

void FTopButtonDropdown::OnDeleteLevelSequence()
{
	if (!GEditor) return;
	
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return;

	if (UCDGLevelSeqSubsystem* SeqSubsystem = World->GetSubsystem<UCDGLevelSeqSubsystem>())
	{
		SeqSubsystem->DeleteLevelSequence();
	}
	else
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Failed to get CDGLevelSeqSubsystem"));
	}
}

void FTopButtonDropdown::OnExportToLevelSequence()
{
	CDGLevelSeqExporter::OpenWindow();
}

#undef LOCTEXT_NAMESPACE
