// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/MultiBox/MultiBoxExtender.h"

class FUICommandList;
class AActor;
class ACDGKeyframe;
class FMenuBuilder;

/**
 * Context menu handler for CDGKeyframe actors in the level viewport
 * Provides quick access to edit keyframe properties from the right-click menu
 */
class FCDGKeyframeContextMenu : public TSharedFromThis<FCDGKeyframeContextMenu>
{
public:
	/** Initialize and register the context menu extender */
	void Initialize();

	/** Cleanup and unregister the context menu extender */
	void Shutdown();

private:
	/** 
	 * Create the context menu extender for the level viewport
	 * Called when right-clicking on actors in the viewport
	 */
	TSharedRef<FExtender> GetLevelViewportContextMenuExtender(
		const TSharedRef<FUICommandList> CommandList, 
		const TArray<AActor*> InActors);

	/**
	 * Populate the context menu with keyframe editing options
	 */
	void FillKeyframeContextMenu(FMenuBuilder& MenuBuilder, const TArray<ACDGKeyframe*> SelectedKeyframes);

	/**
	 * Create submenu for trajectory settings
	 */
	void FillTrajectorySubmenu(FMenuBuilder& MenuBuilder, const TArray<ACDGKeyframe*> SelectedKeyframes);

	/**
	 * Create submenu for camera settings
	 */
	void FillCameraSubmenu(FMenuBuilder& MenuBuilder, const TArray<ACDGKeyframe*> SelectedKeyframes);

	/**
	 * Create submenu for interpolation settings
	 */
	void FillInterpolationSubmenu(FMenuBuilder& MenuBuilder, const TArray<ACDGKeyframe*> SelectedKeyframes);

	/**
	 * Create submenu for visualization settings
	 */
	void FillVisualizationSubmenu(FMenuBuilder& MenuBuilder, const TArray<ACDGKeyframe*> SelectedKeyframes);

	/** Handle to the registered menu extender */
	FDelegateHandle LevelViewportExtenderHandle;
};

