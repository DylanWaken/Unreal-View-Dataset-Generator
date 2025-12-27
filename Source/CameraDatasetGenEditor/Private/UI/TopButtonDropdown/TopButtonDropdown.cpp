// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TopButtonDropdown/TopButtonDropdown.h"
#include "Trajectory/CDGKeyframe.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "LogCameraDatasetGenEditor.h"

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

#undef LOCTEXT_NAMESPACE

