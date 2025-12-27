// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FTopButton;
class FCDGKeyframeContextMenu;
class FCDGCameraPreviewContextMenu;

class FCameraDatasetGenEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the camera preview context menu instance (for CDGEditorState integration) */
	TSharedPtr<FCDGCameraPreviewContextMenu> GetCameraPreviewContextMenu() const { return CameraPreviewContextMenu; }

private:
	/** Callback for when modules are loaded - used to register the context menu after LevelEditor is loaded */
	void OnModulesChanged(FName InModuleName, EModuleChangeReason InChangeReason);

	/** The top toolbar button instance */
	TUniquePtr<FTopButton> TopButton;

	/** The keyframe context menu handler */
	TSharedPtr<FCDGKeyframeContextMenu> KeyframeContextMenu;

	/** The camera preview context menu handler */
	TSharedPtr<FCDGCameraPreviewContextMenu> CameraPreviewContextMenu;

	/** Handle to the module loaded delegate */
	FDelegateHandle ModuleLoadedDelegateHandle;
};

