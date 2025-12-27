// Copyright Epic Games, Inc. All Rights Reserved.

#include "CDGEditorState.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "UI/CameraPreviewEditor/CDGCameraPreviewContextMenu.h"
#include "CameraDatasetGenEditor.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "LevelEditorViewport.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Debug/DebugDrawService.h"
#include "GlobalRenderResources.h"

// ==================== SUBSYSTEM LIFECYCLE ====================

void UCDGEditorState::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CurrentState = ECDGEditorPreviewState::DISABLED;
	CachedViewportSettings.bIsValid = false;
}

void UCDGEditorState::Deinitialize()
{
	// Exit preview mode if active
	if (CurrentState != ECDGEditorPreviewState::DISABLED)
	{
		ExitPreview();
	}

	// Ensure overlay is unregistered
	UnregisterViewportOverlay();

	Super::Deinitialize();
}

// ==================== STATE TRANSITIONS ====================

bool UCDGEditorState::EnterCameraPreview(ACDGKeyframe* Keyframe)
{
	if (!Keyframe)
	{
		UE_LOG(LogTemp, Error, TEXT("CDGEditorState: Cannot enter camera preview with null keyframe"));
		return false;
	}

	// Only allow transition from DISABLED state
	if (CurrentState != ECDGEditorPreviewState::DISABLED)
	{
		UE_LOG(LogTemp, Warning, TEXT("CDGEditorState: Cannot enter camera preview from state %d. Must be in DISABLED state."), 
			static_cast<int32>(CurrentState));
		return false;
	}

	// Cache current viewport settings
	if (!CacheViewportSettings())
	{
		UE_LOG(LogTemp, Error, TEXT("CDGEditorState: Failed to cache viewport settings"));
		return false;
	}

	// Disable all visualizers
	DisableAllVisualizers();

	// Apply keyframe camera to viewport
	if (!ApplyKeyframeCameraToViewport(Keyframe))
	{
		UE_LOG(LogTemp, Error, TEXT("CDGEditorState: Failed to apply keyframe camera to viewport"));
		RestoreViewportSettings();
		RestoreAllVisualizers();
		return false;
	}

	// Update state
	CurrentState = ECDGEditorPreviewState::PREVIEW_CAMERA;
	PreviewedKeyframe = Keyframe;

	// Register viewport overlay for visual feedback
	RegisterViewportOverlay();

	// Start camera synchronization timer
	StartCameraSyncTimer();

	// Show the camera preview context menu
	FCameraDatasetGenEditorModule& EditorModule = FModuleManager::GetModuleChecked<FCameraDatasetGenEditorModule>("CameraDatasetGenEditor");
	if (TSharedPtr<FCDGCameraPreviewContextMenu> ContextMenu = EditorModule.GetCameraPreviewContextMenu())
	{
		ContextMenu->ShowMenu(Keyframe);
	}

	UE_LOG(LogTemp, Log, TEXT("CDGEditorState: Entered PREVIEW_CAMERA state for keyframe '%s'"), 
		*Keyframe->GetActorLabel());

	return true;
}

bool UCDGEditorState::EnterTrajectoryPreview(ACDGTrajectory* Trajectory)
{
	if (!Trajectory)
	{
		UE_LOG(LogTemp, Error, TEXT("CDGEditorState: Cannot enter trajectory preview with null trajectory"));
		return false;
	}

	// Only allow transition from DISABLED state
	if (CurrentState != ECDGEditorPreviewState::DISABLED)
	{
		UE_LOG(LogTemp, Warning, TEXT("CDGEditorState: Cannot enter trajectory preview from state %d. Must be in DISABLED state."), 
			static_cast<int32>(CurrentState));
		return false;
	}

	// Cache current viewport settings
	if (!CacheViewportSettings())
	{
		UE_LOG(LogTemp, Error, TEXT("CDGEditorState: Failed to cache viewport settings"));
		return false;
	}

	// Disable all visualizers
	DisableAllVisualizers();

	// Update state
	CurrentState = ECDGEditorPreviewState::PREVIEW_TRAJECTORY;
	PreviewedTrajectory = Trajectory;

	UE_LOG(LogTemp, Log, TEXT("CDGEditorState: Entered PREVIEW_TRAJECTORY state for trajectory '%s'"), 
		*Trajectory->TrajectoryName.ToString());

	// TODO: Implement trajectory animation preview

	return true;
}

bool UCDGEditorState::ExitPreview()
{
	// Can only exit from preview states
	if (CurrentState == ECDGEditorPreviewState::DISABLED)
	{
		UE_LOG(LogTemp, Warning, TEXT("CDGEditorState: Already in DISABLED state"));
		return false;
	}

	// Stop camera synchronization timer
	StopCameraSyncTimer();

	// Store current state before changing it
	ECDGEditorPreviewState PreviousState = CurrentState;
	bool bPositionedCameraBehindKeyframe = false;

	// Rebuild trajectory spline if we were previewing a keyframe
	if (PreviousState == ECDGEditorPreviewState::PREVIEW_CAMERA && IsValid(PreviewedKeyframe))
	{
		FName TrajectoryName = PreviewedKeyframe->TrajectoryName;
		if (!TrajectoryName.IsNone())
		{
			UWorld* World = GetWorld();
			UCDGTrajectorySubsystem* Subsystem = World ? World->GetSubsystem<UCDGTrajectorySubsystem>() : nullptr;
			if (Subsystem)
			{
				Subsystem->RebuildTrajectorySpline(TrajectoryName);
				UE_LOG(LogTemp, Log, TEXT("CDGEditorState: Rebuilt trajectory spline '%s' after keyframe movement"), 
					*TrajectoryName.ToString());
			}
		}

		// Position viewport camera behind the keyframe
		PositionCameraBehindKeyframe(PreviewedKeyframe);
		bPositionedCameraBehindKeyframe = true;
	}

	// Unregister viewport overlay
	UnregisterViewportOverlay();

	// Hide the camera preview context menu
	FCameraDatasetGenEditorModule& EditorModule = FModuleManager::GetModuleChecked<FCameraDatasetGenEditorModule>("CameraDatasetGenEditor");
	if (TSharedPtr<FCDGCameraPreviewContextMenu> ContextMenu = EditorModule.GetCameraPreviewContextMenu())
	{
		ContextMenu->HideMenu();
	}

	// Restore viewport settings only if we didn't position camera behind keyframe
	if (!bPositionedCameraBehindKeyframe)
	{
		RestoreViewportSettings();
	}

	// Restore visualizers
	RestoreAllVisualizers();
	
	// Update state
	CurrentState = ECDGEditorPreviewState::DISABLED;
	PreviewedKeyframe = nullptr;
	PreviewedTrajectory = nullptr;

	UE_LOG(LogTemp, Log, TEXT("CDGEditorState: Exited preview mode (was in state %d), now DISABLED"), 
		static_cast<int32>(PreviousState));

	return true;
}

// ==================== INTERNAL METHODS ====================

bool UCDGEditorState::CacheViewportSettings()
{
	FLevelEditorViewportClient* ViewportClient = GetActiveViewportClient();
	if (!ViewportClient)
	{
		return false;
	}

	CachedViewportSettings.Location = ViewportClient->GetViewLocation();
	CachedViewportSettings.Rotation = ViewportClient->GetViewRotation();
	CachedViewportSettings.FOV = ViewportClient->ViewFOV;
	CachedViewportSettings.bIsValid = true;

	UE_LOG(LogTemp, Verbose, TEXT("CDGEditorState: Cached viewport settings (Loc: %s, Rot: %s, FOV: %.2f)"),
		*CachedViewportSettings.Location.ToString(),
		*CachedViewportSettings.Rotation.ToString(),
		CachedViewportSettings.FOV);

	return true;
}

bool UCDGEditorState::RestoreViewportSettings()
{
	if (!CachedViewportSettings.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("CDGEditorState: No valid cached viewport settings to restore"));
		return false;
	}

	FLevelEditorViewportClient* ViewportClient = GetActiveViewportClient();
	if (!ViewportClient)
	{
		return false;
	}

	ViewportClient->SetViewLocation(CachedViewportSettings.Location);
	ViewportClient->SetViewRotation(CachedViewportSettings.Rotation);
	ViewportClient->ViewFOV = CachedViewportSettings.FOV;
	ViewportClient->Invalidate();

	UE_LOG(LogTemp, Verbose, TEXT("CDGEditorState: Restored viewport settings (Loc: %s, Rot: %s, FOV: %.2f)"),
		*CachedViewportSettings.Location.ToString(),
		*CachedViewportSettings.Rotation.ToString(),
		CachedViewportSettings.FOV);

	CachedViewportSettings.bIsValid = false;

	return true;
}

bool UCDGEditorState::ApplyKeyframeCameraToViewport(ACDGKeyframe* Keyframe)
{
	if (!Keyframe)
	{
		return false;
	}

	FLevelEditorViewportClient* ViewportClient = GetActiveViewportClient();
	if (!ViewportClient)
	{
		return false;
	}

	// Apply keyframe transform and FOV
	ViewportClient->SetViewLocation(Keyframe->GetActorLocation());
	ViewportClient->SetViewRotation(Keyframe->GetActorRotation());
	ViewportClient->ViewFOV = Keyframe->LensSettings.FieldOfView;
	ViewportClient->Invalidate();

	UE_LOG(LogTemp, Verbose, TEXT("CDGEditorState: Applied keyframe camera to viewport (Loc: %s, Rot: %s, FOV: %.2f)"),
		*Keyframe->GetActorLocation().ToString(),
		*Keyframe->GetActorRotation().ToString(),
		Keyframe->LensSettings.FieldOfView);

	return true;
}

FLevelEditorViewportClient* UCDGEditorState::GetActiveViewportClient()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	if (!LevelEditor.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditor->GetActiveViewportInterface();
	if (!ActiveLevelViewport.IsValid())
	{
		return nullptr;
	}

	return &ActiveLevelViewport->GetLevelViewportClient();
}

void UCDGEditorState::DisableAllVisualizers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (Subsystem)
	{
		Subsystem->DisableAllVisualizers();
	}
}

void UCDGEditorState::RestoreAllVisualizers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (Subsystem)
	{
		Subsystem->RestoreVisualizerStates();
	}
}

void UCDGEditorState::RegisterViewportOverlay()
{
	// Unregister first to avoid duplicates
	UnregisterViewportOverlay();

	// Register delegate for viewport overlay rendering
	ViewportOverlayDelegateHandle = UDebugDrawService::Register(
		TEXT("Editor"),
		FDebugDrawDelegate::CreateUObject(this, &UCDGEditorState::DrawViewportOverlay)
	);

	UE_LOG(LogTemp, Verbose, TEXT("CDGEditorState: Registered viewport overlay"));
}

void UCDGEditorState::UnregisterViewportOverlay()
{
	if (ViewportOverlayDelegateHandle.IsValid())
	{
		UDebugDrawService::Unregister(ViewportOverlayDelegateHandle);
		ViewportOverlayDelegateHandle.Reset();

		UE_LOG(LogTemp, Verbose, TEXT("CDGEditorState: Unregistered viewport overlay"));
	}
}

void UCDGEditorState::DrawViewportOverlay(UCanvas* Canvas, APlayerController* PC)
{
	if (!Canvas || CurrentState != ECDGEditorPreviewState::PREVIEW_CAMERA)
	{
		return;
	}

	// Use ClipX/ClipY instead of SizeX/SizeY to match the actual drawable area
	const float CanvasWidth = Canvas->ClipX;
	const float CanvasHeight = Canvas->ClipY;
	const float BorderThickness = 10.0f;
	const FLinearColor BorderColor = FLinearColor(1.0f, 0.09f, 0.09f, 1.0f);

	// Create a single tile item and reuse it (following Unreal Engine pattern from HUD.cpp)
	FCanvasTileItem TileItem(FVector2D::ZeroVector, GWhiteTexture, BorderColor);
	TileItem.BlendMode = SE_BLEND_Translucent;

	// Draw red border around viewport - using full dimensions with overlap (simpler approach)
	// Top border
	TileItem.Position = FVector2D(0.0f, 0.0f);
	TileItem.Size = FVector2D(CanvasWidth, BorderThickness);
	Canvas->DrawItem(TileItem);

	// Bottom border
	TileItem.Position = FVector2D(0.0f, CanvasHeight - BorderThickness);
	TileItem.Size = FVector2D(CanvasWidth, BorderThickness);
	Canvas->DrawItem(TileItem);

	// Left border (full height)
	TileItem.Position = FVector2D(0.0f, 0.0f);
	TileItem.Size = FVector2D(BorderThickness, CanvasHeight);
	Canvas->DrawItem(TileItem);

	// Right border (full height)
	TileItem.Position = FVector2D(CanvasWidth - BorderThickness, 0.0f);
	TileItem.Size = FVector2D(BorderThickness, CanvasHeight);
	Canvas->DrawItem(TileItem);

	// Draw "CAMERA PREVIEW" text in top right corner
	const FString PreviewText = TEXT("CAMERA PREVIEW");
	const float TextScale = 2.0f;
	const float TextPadding = 15.0f;

	FCanvasTextItem TextItem(
		FVector2D(0, 0), // Position will be calculated below
		FText::FromString(PreviewText),
		GEngine->GetLargeFont(),
		BorderColor
	);
	TextItem.Scale = FVector2D(TextScale, TextScale);

	// Calculate text size to position it properly
	float TextWidth, TextHeight;
	Canvas->StrLen(GEngine->GetLargeFont(), PreviewText, TextWidth, TextHeight);
	TextWidth *= TextScale;
	TextHeight *= TextScale;

	// Position in top right corner
	TextItem.Position = FVector2D(
		CanvasWidth - TextWidth - TextPadding * 2 - BorderThickness,
		BorderThickness + TextPadding
	);

	Canvas->DrawItem(TextItem);
}

void UCDGEditorState::SyncKeyframeFromViewport()
{
	// Only sync when in camera preview mode
	if (CurrentState != ECDGEditorPreviewState::PREVIEW_CAMERA || !IsValid(PreviewedKeyframe))
	{
		return;
	}

	FLevelEditorViewportClient* ViewportClient = GetActiveViewportClient();
	if (!ViewportClient)
	{
		return;
	}

	// Get current viewport transform
	FVector ViewportLocation = ViewportClient->GetViewLocation();
	FRotator ViewportRotation = ViewportClient->GetViewRotation();

	// Check if transform has changed significantly (avoid unnecessary updates)
	const float LocationThreshold = 0.01f; // 0.01cm
	const float RotationThreshold = 0.01f; // 0.01 degrees
	
	FVector CurrentLocation = PreviewedKeyframe->GetActorLocation();
	FRotator CurrentRotation = PreviewedKeyframe->GetActorRotation();

	bool bLocationChanged = !ViewportLocation.Equals(CurrentLocation, LocationThreshold);
	bool bRotationChanged = !ViewportRotation.Equals(CurrentRotation, RotationThreshold);

	if (bLocationChanged || bRotationChanged)
	{
		// Update keyframe transform to match viewport
		PreviewedKeyframe->SetActorLocation(ViewportLocation);
		PreviewedKeyframe->SetActorRotation(ViewportRotation);
		
		// Update visualizer to reflect new transform
		PreviewedKeyframe->UpdateVisualizer();
	}
}

void UCDGEditorState::UpdateViewportFromKeyframe()
{
	SyncViewportFromKeyframe();
}

void UCDGEditorState::SyncViewportFromKeyframe()
{
	// Only sync when in camera preview mode
	if (CurrentState != ECDGEditorPreviewState::PREVIEW_CAMERA || !IsValid(PreviewedKeyframe))
	{
		return;
	}

	FLevelEditorViewportClient* ViewportClient = GetActiveViewportClient();
	if (!ViewportClient)
	{
		return;
	}

	// Apply keyframe's FOV and other camera settings to viewport
	ViewportClient->ViewFOV = PreviewedKeyframe->LensSettings.FieldOfView;
	ViewportClient->Invalidate();

	UE_LOG(LogTemp, VeryVerbose, TEXT("CDGEditorState: Synced viewport from keyframe (FOV: %.2f)"),
		PreviewedKeyframe->LensSettings.FieldOfView);
}

void UCDGEditorState::StartCameraSyncTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Stop existing timer if any
	StopCameraSyncTimer();

	// Start timer to sync keyframe from viewport at 60 FPS (0.0166s interval)
	World->GetTimerManager().SetTimer(
		CameraSyncTimerHandle,
		FTimerDelegate::CreateUObject(this, &UCDGEditorState::SyncKeyframeFromViewport),
		0.0166f,
		true // Loop
	);

	UE_LOG(LogTemp, Log, TEXT("CDGEditorState: Started camera sync timer"));
}

void UCDGEditorState::StopCameraSyncTimer()
{
	UWorld* World = GetWorld();
	if (!World || !CameraSyncTimerHandle.IsValid())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(CameraSyncTimerHandle);
	CameraSyncTimerHandle.Invalidate();

	UE_LOG(LogTemp, Log, TEXT("CDGEditorState: Stopped camera sync timer"));
}

void UCDGEditorState::PositionCameraBehindKeyframe(ACDGKeyframe* Keyframe)
{
	if (!Keyframe)
	{
		return;
	}

	FLevelEditorViewportClient* ViewportClient = GetActiveViewportClient();
	if (!ViewportClient)
	{
		return;
	}

	// Get keyframe's transform
	FVector KeyframeLocation = Keyframe->GetActorLocation();
	FRotator KeyframeRotation = Keyframe->GetActorRotation();

	// Calculate position 200 units behind the keyframe (opposite to forward direction)
	const float DistanceBehind = 200.0f;
	FVector ForwardVector = KeyframeRotation.Vector();
	FVector CameraLocation = KeyframeLocation - (ForwardVector * DistanceBehind);

	// Set viewport camera to face the same direction as the keyframe
	ViewportClient->SetViewLocation(CameraLocation);
	ViewportClient->SetViewRotation(KeyframeRotation);
	
	// Restore original FOV from cached settings
	if (CachedViewportSettings.bIsValid)
	{
		ViewportClient->ViewFOV = CachedViewportSettings.FOV;
	} else {
		ViewportClient->ViewFOV = 90.0f;
	}
	
	ViewportClient->Invalidate();

	UE_LOG(LogTemp, Log, TEXT("CDGEditorState: Positioned camera behind keyframe (Distance: %.1f units, FOV: %.2f)"), 
		DistanceBehind, ViewportClient->ViewFOV);
}

