// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraDatasetGenEditor.h"
#include "UI/TopButton/TopButtonStyle.h"
#include "UI/TopButton/TopButton.h"
#include "UI/KeyFrameEditor/CDGKeyframeContextMenu.h"
#include "UI/CameraPreviewEditor/CDGCameraPreviewContextMenu.h"
#include "LogCameraDatasetGenEditor.h"

#define LOCTEXT_NAMESPACE "FCameraDatasetGenEditorModule"

void FCameraDatasetGenEditorModule::StartupModule()
{
	// Initialize the button style (icon management)
	FTopButtonStyle::Initialize();
	
	// Register the custom icon BEFORE creating the button
	FTopButtonStyle::SetSVGIcon("CustomIcon", "Icons/CamData");
	
	// Create the toolbar button with the custom icon
	TopButton = MakeUnique<FTopButton>("CustomIcon");

	// Register a callback to initialize the context menu once the LevelEditor module is loaded
	// This is necessary because LevelEditor may not be loaded yet during StartupModule
	ModuleLoadedDelegateHandle = FModuleManager::Get().OnModulesChanged().AddRaw(
		this, &FCameraDatasetGenEditorModule::OnModulesChanged);

	// If LevelEditor is already loaded, initialize the context menu immediately
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		OnModulesChanged("LevelEditor", EModuleChangeReason::ModuleLoaded);
	}
}

void FCameraDatasetGenEditorModule::ShutdownModule()
{
	// Unregister the module loaded delegate
	if (ModuleLoadedDelegateHandle.IsValid())
	{
		FModuleManager::Get().OnModulesChanged().Remove(ModuleLoadedDelegateHandle);
		ModuleLoadedDelegateHandle.Reset();
	}

	// Shutdown the camera preview context menu
	if (CameraPreviewContextMenu.IsValid())
	{
		CameraPreviewContextMenu->Shutdown();
		CameraPreviewContextMenu.Reset();
	}

	// Shutdown the keyframe context menu
	if (KeyframeContextMenu.IsValid())
	{
		KeyframeContextMenu->Shutdown();
		KeyframeContextMenu.Reset();
	}

	// Clean up the toolbar button
	TopButton.Reset();
	
	// Shutdown the button style
	FTopButtonStyle::Shutdown();
}

void FCameraDatasetGenEditorModule::OnModulesChanged(FName InModuleName, EModuleChangeReason InChangeReason)
{
	if (InModuleName == "LevelEditor" && InChangeReason == EModuleChangeReason::ModuleLoaded)
	{
		// Initialize the keyframe context menu now that LevelEditor is loaded
		if (!KeyframeContextMenu.IsValid())
		{
			KeyframeContextMenu = MakeShared<FCDGKeyframeContextMenu>();
			KeyframeContextMenu->Initialize();
			
			UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGKeyframe context menu registered successfully"));
		}

		// Initialize the camera preview context menu now that LevelEditor is loaded
		if (!CameraPreviewContextMenu.IsValid())
		{
			CameraPreviewContextMenu = MakeShared<FCDGCameraPreviewContextMenu>();
			CameraPreviewContextMenu->Initialize();
			
			UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGCameraPreview context menu registered successfully"));
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCameraDatasetGenEditorModule, CameraDatasetGenEditor)

