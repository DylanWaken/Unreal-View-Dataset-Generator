// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CDGEditorState.generated.h"

class ACDGKeyframe;
class ACDGTrajectory;
class UCDGTrajectorySubsystem;
class FLevelEditorViewportClient;

/**
 * Editor preview state for CDG system
 */
UENUM(BlueprintType)
enum class ECDGEditorPreviewState : uint8
{
	/** No preview active - normal editor mode */
	DISABLED,
	
	/** Previewing a specific keyframe camera */
	PREVIEW_CAMERA,
	
	/** Previewing trajectory animation */
	PREVIEW_TRAJECTORY
};

/**
 * Cached viewport camera settings for restoration
 */
USTRUCT()
struct FCDGCachedViewportSettings
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	float FOV = 90.0f;

	bool bIsValid = false;
};

/**
 * UCDGEditorState
 * 
 * Manages editor preview states for the CDG system.
 * Handles state transitions and viewport camera control.
 * 
 * State Machine:
 * - DISABLED <-> PREVIEW_CAMERA
 * - DISABLED <-> PREVIEW_TRAJECTORY
 */
UCLASS()
class CAMERADATASETGENEDITOR_API UCDGEditorState : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ==================== SUBSYSTEM LIFECYCLE ====================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ==================== STATE MANAGEMENT ====================

	/** Get current preview state */
	UFUNCTION(BlueprintCallable, Category = "CDGEditorState")
	ECDGEditorPreviewState GetCurrentState() const { return CurrentState; }

	/** Check if in disabled state */
	UFUNCTION(BlueprintCallable, Category = "CDGEditorState")
	bool IsDisabled() const { return CurrentState == ECDGEditorPreviewState::DISABLED; }

	/** Check if in camera preview state */
	UFUNCTION(BlueprintCallable, Category = "CDGEditorState")
	bool IsPreviewingCamera() const { return CurrentState == ECDGEditorPreviewState::PREVIEW_CAMERA; }

	/** Check if in trajectory preview state */
	UFUNCTION(BlueprintCallable, Category = "CDGEditorState")
	bool IsPreviewingTrajectory() const { return CurrentState == ECDGEditorPreviewState::PREVIEW_TRAJECTORY; }

	// ==================== STATE TRANSITIONS ====================

	/**
	 * Enter camera preview mode
	 * Transitions: DISABLED -> PREVIEW_CAMERA
	 */
	UFUNCTION(BlueprintCallable, Category = "CDGEditorState")
	bool EnterCameraPreview(ACDGKeyframe* Keyframe);

	/**
	 * Enter trajectory preview mode
	 * Transitions: DISABLED -> PREVIEW_TRAJECTORY
	 */
	UFUNCTION(BlueprintCallable, Category = "CDGEditorState")
	bool EnterTrajectoryPreview(ACDGTrajectory* Trajectory);

	/**
	 * Exit preview mode and return to disabled state
	 * Transitions: PREVIEW_CAMERA/PREVIEW_TRAJECTORY -> DISABLED
	 */
	UFUNCTION(BlueprintCallable, Category = "CDGEditorState")
	bool ExitPreview();

	/**
	 * Manually sync viewport camera from keyframe properties
	 * Use this when keyframe camera properties (FOV, etc.) are changed programmatically
	 */
	UFUNCTION(BlueprintCallable, Category = "CDGEditorState")
	void UpdateViewportFromKeyframe();

	// ==================== SETTINGS ====================

protected:
	// ==================== INTERNAL DATA ====================

	/** Current preview state */
	UPROPERTY(Transient)
	ECDGEditorPreviewState CurrentState = ECDGEditorPreviewState::DISABLED;

	/** Cached viewport settings for restoration */
	UPROPERTY(Transient)
	FCDGCachedViewportSettings CachedViewportSettings;

	/** Currently previewed keyframe (if in PREVIEW_CAMERA state) */
	UPROPERTY(Transient)
	TObjectPtr<ACDGKeyframe> PreviewedKeyframe;

	/** Currently previewed trajectory (if in PREVIEW_TRAJECTORY state) */
	UPROPERTY(Transient)
	TObjectPtr<ACDGTrajectory> PreviewedTrajectory;

	/** Delegate handle for viewport overlay rendering */
	FDelegateHandle ViewportOverlayDelegateHandle;

	// ==================== INTERNAL METHODS ====================

	/** Cache current viewport settings */
	bool CacheViewportSettings();

	/** Restore cached viewport settings */
	bool RestoreViewportSettings();

	/** Apply keyframe camera settings to viewport */
	bool ApplyKeyframeCameraToViewport(ACDGKeyframe* Keyframe);

	/** Get the active level editor viewport client */
	FLevelEditorViewportClient* GetActiveViewportClient();

	/** Disable all visualizers and save their states */
	void DisableAllVisualizers();

	/** Restore all visualizer states */
	void RestoreAllVisualizers();

	/** Register viewport overlay rendering */
	void RegisterViewportOverlay();

	/** Unregister viewport overlay rendering */
	void UnregisterViewportOverlay();

	/** Draw viewport overlay (border and text) */
	void DrawViewportOverlay(UCanvas* Canvas, APlayerController* PC);

	/** Sync keyframe transform from viewport camera (called by timer) */
	void SyncKeyframeFromViewport();

	/** Sync viewport camera from keyframe (called when keyframe properties change) */
	void SyncViewportFromKeyframe();

	/** Start camera sync timer */
	void StartCameraSyncTimer();

	/** Stop camera sync timer */
	void StopCameraSyncTimer();

	/** Position viewport camera behind a keyframe (for exit preview) */
	void PositionCameraBehindKeyframe(ACDGKeyframe* Keyframe);

	/** Timer handle for camera synchronization */
	FTimerHandle CameraSyncTimerHandle;
};

