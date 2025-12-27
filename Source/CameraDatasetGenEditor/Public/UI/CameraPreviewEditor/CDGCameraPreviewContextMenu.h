// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class ACDGKeyframe;
class UCDGEditorState;
class SWindow;
class FMenuBuilder;

/**
 * Floating context menu that appears during camera preview mode
 * Provides quick access to camera settings for the previewed keyframe
 */
class SCDGCameraPreviewContextMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCDGCameraPreviewContextMenu) {}
	SLATE_END_ARGS()

	/** Construct the widget */
	void Construct(const FArguments& InArgs, ACDGKeyframe* InKeyframe);

	/** Update the keyframe being edited */
	void SetKeyframe(ACDGKeyframe* InKeyframe);

	/** Get the current keyframe */
	ACDGKeyframe* GetKeyframe() const { return Keyframe.Get(); }

private:
	/** The keyframe being previewed and edited */
	TWeakObjectPtr<ACDGKeyframe> Keyframe;

	/** Build the camera settings content */
	TSharedRef<SWidget> BuildCameraSettingsContent();
};

/**
 * Manager for the camera preview context menu
 * Handles showing/hiding the menu based on editor state
 */
class FCDGCameraPreviewContextMenu : public TSharedFromThis<FCDGCameraPreviewContextMenu>
{
public:
	/** Initialize and hook into the editor state system */
	void Initialize();

	/** Cleanup and shutdown */
	void Shutdown();

	/** Show the context menu for a keyframe */
	void ShowMenu(ACDGKeyframe* Keyframe);

	/** Hide the context menu */
	void HideMenu();

	/** Check if menu is currently visible */
	bool IsMenuVisible() const;

private:
	/** Reference to the menu widget */
	TSharedPtr<SCDGCameraPreviewContextMenu> MenuWidget;

	/** Reference to the popup window */
	TSharedPtr<SWindow> MenuWindow;

	/** Subscribe to editor state changes */
	void SubscribeToEditorState();

	/** Unsubscribe from editor state changes */
	void UnsubscribeFromEditorState();

	/** Timer handle for polling editor state */
	FTimerHandle StateCheckTimerHandle;

	/** Poll the editor state for changes */
	void CheckEditorState();

	/** Handle window closed event */
	void OnWindowClosed(const TSharedRef<SWindow>& Window);

	/** Last known editor state */
	TWeakObjectPtr<UCDGEditorState> EditorState;

	/** Flag to prevent infinite recursion when closing */
	bool bIsClosing = false;
};

