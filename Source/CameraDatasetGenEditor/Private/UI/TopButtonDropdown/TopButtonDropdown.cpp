// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TopButtonDropdown/TopButtonDropdown.h"
#include "UI/LevelSeqExporterWindow/CDGLevelSeqExporter.h"
#include "UI/MRQInterfaceWindow/CDGMRQInterfaceWindow.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "IO/TrajectorySL.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "LogCameraDatasetGenEditor.h"
#include "LevelSequenceInterface/CDGLevelSeqSubsystem.h"
#include "Engine/World.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"

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

	// Add "Open MRQ Interface" menu entry
	MenuBuilder.AddMenuEntry(
		LOCTEXT("OpenMRQInterface_Label", "Open MRQ Interface"),
		LOCTEXT("OpenMRQInterface_Tooltip", "Open Movie Render Queue Interface to render trajectories"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Cinematics"),
		FUIAction(FExecuteAction::CreateStatic(&FTopButtonDropdown::OnOpenMRQInterface))
	);

	MenuBuilder.AddMenuSeparator();

	// Add "Load Trajectories from JSON" menu entry
	MenuBuilder.AddMenuEntry(
		LOCTEXT("LoadTrajectoriesFromJSON_Label", "Load Trajectories from JSON"),
		LOCTEXT("LoadTrajectoriesFromJSON_Tooltip", "Load and spawn trajectories from a JSON file"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"),
		FUIAction(FExecuteAction::CreateStatic(&FTopButtonDropdown::OnLoadTrajectoriesFromJSON))
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

void FTopButtonDropdown::OnLoadTrajectoriesFromJSON()
{
	// Get Desktop Platform for file dialog
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Failed to get Desktop Platform module"));
		return;
	}

	// Get parent window for the file dialog
	void* ParentWindowHandle = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWindow> MainWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (MainWindow.IsValid() && MainWindow->GetNativeWindow().IsValid())
		{
			ParentWindowHandle = MainWindow->GetNativeWindow()->GetOSWindowHandle();
		}
	}

	// Default open location
	FString DefaultPath = FPaths::ProjectSavedDir() / TEXT("Trajectories");

	// Show file open dialog
	TArray<FString> OutFiles;
	bool bFileSelected = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Load Trajectories from JSON"),
		DefaultPath,
		TEXT(""),
		TEXT("JSON Files (*.json)|*.json|All Files (*.*)|*.*"),
		EFileDialogFlags::None,
		OutFiles
	);

	if (!bFileSelected || OutFiles.Num() == 0)
	{
		// User cancelled
		return;
	}

	FString FilePath = OutFiles[0];

	// Load trajectories from JSON
	bool bSuccess = TrajectorySL::LoadAllTrajectories(FilePath);

	// Show notification
	FNotificationInfo Info(bSuccess 
		? FText::Format(LOCTEXT("LoadJSONSuccess", "Trajectories loaded from:\n{0}"), FText::FromString(FilePath))
		: LOCTEXT("LoadJSONFailed", "Failed to load trajectories from JSON"));
	
	Info.ExpireDuration = 5.0f;
	Info.bUseLargeFont = false;
	Info.bUseSuccessFailIcons = true;
	
	FSlateNotificationManager::Get().AddNotification(Info);

	if (bSuccess)
	{
		UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("Trajectories loaded from JSON: %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Failed to load trajectories from JSON: %s"), *FilePath);
	}
}

void FTopButtonDropdown::OnOpenMRQInterface()
{
	CDGMRQInterfaceWindow::OpenWindow();
}

#undef LOCTEXT_NAMESPACE
