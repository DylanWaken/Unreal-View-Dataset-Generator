// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "Trajectory/CDGKeyframeVisualizer.h"
#include "DrawDebugHelpers.h"
#include "Components/SphereComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "SceneManagement.h"
#endif

ACDGKeyframe::ACDGKeyframe()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Create root scene component
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

#if WITH_EDITORONLY_DATA
	// Create visualizer component for camera frustum
	VisualizerComponent = CreateDefaultSubobject<UCDGKeyframeVisualizer>(TEXT("Visualizer"));
	if (VisualizerComponent)
	{
		VisualizerComponent->SetupAttachment(RootComponent);
		VisualizerComponent->bHiddenInGame = true;
	}

	// Create invisible sphere component for easier selection in editor
	SelectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SelectionSphere"));
	if (SelectionSphere)
	{
		SelectionSphere->SetupAttachment(RootComponent);
		SelectionSphere->SetSphereRadius(30.0f);
		SelectionSphere->SetHiddenInGame(true);
		SelectionSphere->SetVisibility(false); // Invisible in editor
		SelectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SelectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		SelectionSphere->bIsEditorOnly = true;
		SelectionSphere->bDrawOnlyIfSelected = false;
		SelectionSphere->ShapeColor = FColor::Transparent;
	}

	// Enable tick in editor for visualization updates
	bRunConstructionScriptOnDrag = false;
	bIsEditorOnlyActor = false;

	// Initialize tracking variable
	PreviousTrajectoryName = NAME_None;
#endif

	// Set default visibility
	SetActorHiddenInGame(true);
	
	// Initialize FOV from focal length (always synchronized)
	UpdateFOVFromFocalLength();
}

void ACDGKeyframe::BeginPlay()
{
	Super::BeginPlay();
	
	// Register with trajectory subsystem
	if (UWorld* World = GetWorld())
	{
		if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
		{
			Subsystem->RegisterKeyframe(this);
		}
	}

#if WITH_EDITOR
	// Initialize previous trajectory name
	PreviousTrajectoryName = TrajectoryName;
#endif
	
	// Hide actor during play mode
	UpdateVisibility();
}

void ACDGKeyframe::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from trajectory subsystem
	if (UWorld* World = GetWorld())
	{
		if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
		{
			Subsystem->UnregisterKeyframe(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ACDGKeyframe::Destroyed()
{
	// Ensure cleanup happens when actor is destroyed (more reliable than EndPlay in editor)
	if (UWorld* World = GetWorld())
	{
		if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
		{
			Subsystem->UnregisterKeyframe(this);
		}
	}

	Super::Destroyed();
}

void ACDGKeyframe::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if WITH_EDITOR
	// Update visibility in editor
	if (!GetWorld()->IsGameWorld())
	{
		UpdateVisibility();
	}
	
	// Ensure visualizer component stays aligned with keyframe
	if (VisualizerComponent)
	{
		VisualizerComponent->SetRelativeTransform(FTransform::Identity);
	}
#endif
}

#if WITH_EDITOR
void ACDGKeyframe::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();

	// Handle FOV <-> Focal Length synchronization (always synchronized)
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FCDGCameraLensSettings, FieldOfView))
	{
		UpdateFocalLengthFromFOV();
		UpdateVisualizer();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FCDGCameraLensSettings, FocalLength))
	{
		UpdateFOVFromFocalLength();
		UpdateVisualizer();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FCDGCameraFilmbackSettings, SensorWidth))
	{
		// Update sensor height from width and aspect ratio (height is locked to aspect ratio)
		if (FilmbackSettings.SensorAspectRatio > 0.0f)
		{
			FilmbackSettings.SensorHeight = FilmbackSettings.SensorWidth / FilmbackSettings.SensorAspectRatio;
		}
		UpdateFOVFromFocalLength();
		UpdateVisualizer();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FCDGCameraFilmbackSettings, SensorAspectRatio))
	{
		// Update sensor height from width and aspect ratio (height is locked to aspect ratio)
		if (FilmbackSettings.SensorAspectRatio > 0.0f)
		{
			FilmbackSettings.SensorHeight = FilmbackSettings.SensorWidth / FilmbackSettings.SensorAspectRatio;
		}
		UpdateFOVFromFocalLength();
		UpdateVisualizer();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACDGKeyframe, KeyframeColor) ||
	         PropertyName == GET_MEMBER_NAME_CHECKED(ACDGKeyframe, FrustumSize) ||
	         PropertyName == GET_MEMBER_NAME_CHECKED(ACDGKeyframe, bShowCameraFrustum))
	{
		UpdateVisualizer();
	}

	// Handle trajectory name change specially
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACDGKeyframe, TrajectoryName))
	{
		// If trajectory name is empty, generate a unique name
		if (TrajectoryName.IsNone())
		{
			if (UWorld* World = GetWorld())
			{
				if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
				{
					TrajectoryName = Subsystem->GenerateUniqueTrajectoryName();
				}
			}
		}

		// Notify subsystem if trajectory name has changed
		if (TrajectoryName != PreviousTrajectoryName)
		{
			if (UWorld* World = GetWorld())
			{
				if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
				{
					Subsystem->OnKeyframeTrajectoryNameChanged(this, PreviousTrajectoryName);
					PreviousTrajectoryName = TrajectoryName; // Update for next change
				}
			}
		}
	}
	// Notify trajectory subsystem of order changes (triggers auto-reassignment)
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACDGKeyframe, OrderInTrajectory))
	{
		if (UWorld* World = GetWorld())
		{
			if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
			{
				Subsystem->OnKeyframeOrderChanged(this);
			}
		}
	}
	// Notify trajectory subsystem of other changes
	else if (PropertyName == TEXT("InterpolationSettings") ||
	         PropertyName == GET_MEMBER_NAME_CHECKED(ACDGKeyframe, TimeToCurrentFrame) ||
	         PropertyName == GET_MEMBER_NAME_CHECKED(ACDGKeyframe, TimeAtCurrentFrame) ||
	         PropertyName == GET_MEMBER_NAME_CHECKED(ACDGKeyframe, SpeedInterpolationMode))
	{
		NotifyTrajectorySubsystem();
	}
}

void ACDGKeyframe::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// Notify trajectory subsystem continuously during move for real-time spline updates
	NotifyTrajectorySubsystem();
}

void ACDGKeyframe::PostEditImport()
{
	Super::PostEditImport();

	UE_LOG(LogCameraDatasetGen, Log, TEXT("Keyframe PostEditImport: %s --- TrajectoryName: %s, PreviousTrajectoryName: %s"), 
	       *GetName(), *TrajectoryName.ToString(), *PreviousTrajectoryName.ToString());

	// NOTE: PostEditImport is called AFTER properties are copied during copy-paste operations
	// At this point, TrajectoryName contains the value from the source keyframe
	
	// Check if trajectory name changed during import (it will have changed from the temp name assigned in PostActorCreated)
	if (TrajectoryName != PreviousTrajectoryName && !PreviousTrajectoryName.IsNone())
	{
		// Handle trajectory change - this moves the keyframe from the temporary trajectory
		// (created in PostActorCreated) to the actual trajectory from the source
		if (UWorld* World = GetWorld())
		{
			if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
			{
				UE_LOG(LogCameraDatasetGen, Log, TEXT("Keyframe %s trajectory changed during import from '%s' to '%s'"), 
				       *GetName(), *PreviousTrajectoryName.ToString(), *TrajectoryName.ToString());
				
				// Notify subsystem of trajectory change - this will:
				// 1. Remove keyframe from old (temporary) trajectory
				// 2. Add keyframe to new (source) trajectory
				// 3. Clean up empty trajectories
				Subsystem->OnKeyframeTrajectoryNameChanged(this, PreviousTrajectoryName);
			}
		}
	}

	// Update previous trajectory name for future changes
	PreviousTrajectoryName = TrajectoryName;
}

void ACDGKeyframe::NotifyTrajectorySubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
		{
			Subsystem->OnKeyframeModified(this);
		}
	}
}

void ACDGKeyframe::PostLoad()
{
	Super::PostLoad();

	UE_LOG(LogCameraDatasetGen, Log, TEXT("Keyframe PostLoad: %s"), *GetName());
	
	// Initialize previous trajectory name
	PreviousTrajectoryName = TrajectoryName;
}

void ACDGKeyframe::PostActorCreated()
{
	Super::PostActorCreated();

	UE_LOG(LogCameraDatasetGen, Log, TEXT("Keyframe PostActorCreated: %s --- TrajectoryName: %s"), *GetName(), *TrajectoryName.ToString());
	
	// NOTE: PostActorCreated is called BEFORE properties are copied from source during duplication
	// Therefore, TrajectoryName will ALWAYS be None here, even for duplicated actors
	// The actual trajectory from the source will be set later, and PostEditImport will handle it
	
	// Generate unique trajectory name for all newly created keyframes
	if (TrajectoryName.IsNone())
	{
		if (UWorld* World = GetWorld())
		{
			if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
			{
				TrajectoryName = Subsystem->GenerateUniqueTrajectoryName();
				UE_LOG(LogCameraDatasetGen, Log, TEXT("Generated unique trajectory name: %s"), *TrajectoryName.ToString());
			}
		}
	}
	
	// Initialize previous trajectory name
	PreviousTrajectoryName = TrajectoryName;
	
	// Register with subsystem when created
	// For duplicates, this will temporarily register with the generated name,
	// then PostEditImport will move it to the correct trajectory
	if (UWorld* World = GetWorld())
	{
		if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
		{
			Subsystem->RegisterKeyframe(this);
		}
	}
}
#endif

FTransform ACDGKeyframe::GetKeyframeTransform() const
{
	return GetActorTransform();
}

void ACDGKeyframe::SetKeyframeTransform(const FTransform& NewTransform)
{
	SetActorTransform(NewTransform);

#if WITH_EDITOR
	NotifyTrajectorySubsystem();
#endif
}

float ACDGKeyframe::CalculateFOVFromFocalLength() const
{
	// FOV calculation: FOV = 2 * atan(SensorWidth / (2 * FocalLength))
	// Convert from radians to degrees
	if (LensSettings.FocalLength > 0.0f && FilmbackSettings.SensorWidth > 0.0f)
	{
		const float FOVRadians = 2.0f * FMath::Atan(FilmbackSettings.SensorWidth / (2.0f * LensSettings.FocalLength));
		return FMath::RadiansToDegrees(FOVRadians);
	}
	return 90.0f; // Default FOV
}

float ACDGKeyframe::CalculateFocalLengthFromFOV() const
{
	// Inverse FOV calculation: FocalLength = SensorWidth / (2 * tan(FOV / 2))
	if (LensSettings.FieldOfView > 0.0f && FilmbackSettings.SensorWidth > 0.0f)
	{
		const float FOVRadians = FMath::DegreesToRadians(LensSettings.FieldOfView);
		return FilmbackSettings.SensorWidth / (2.0f * FMath::Tan(FOVRadians / 2.0f));
	}
	return 35.0f; // Default focal length
}

void ACDGKeyframe::UpdateFOVFromFocalLength()
{
	LensSettings.FieldOfView = CalculateFOVFromFocalLength();
}

void ACDGKeyframe::UpdateFocalLengthFromFOV()
{
	LensSettings.FocalLength = CalculateFocalLengthFromFOV();
}

FString ACDGKeyframe::GetKeyframeID() const
{
	return FString::Printf(TEXT("%s_%d_%s"), 
		*TrajectoryName.ToString(), 
		OrderInTrajectory, 
		*GetName());
}

bool ACDGKeyframe::ShouldHideActor() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	// Hide during play mode
	if (World->IsGameWorld())
	{
		return true;
	}

#if WITH_EDITOR
	// Check if we're rendering (MRQ, etc.)
	// Note: This is a simplified check. More sophisticated detection may be needed
	// for Movie Render Queue specifically.
	if (GIsEditor && World->WorldType == EWorldType::Editor)
	{
		// Visible in editor
		return false;
	}
#endif

	return true;
}

void ACDGKeyframe::UpdateVisibility()
{
	const bool bShouldHide = ShouldHideActor();
	
	SetActorHiddenInGame(bShouldHide);
	
#if WITH_EDITORONLY_DATA
	if (VisualizerComponent)
	{
		VisualizerComponent->SetVisibility(!bShouldHide && bShowCameraFrustum);
	}
#endif
}

FLinearColor ACDGKeyframe::GetVisualizationColor() const
{
	// Get color from trajectory if assigned
	if (IsAssignedToTrajectory())
	{
		if (UWorld* World = GetWorld())
		{
			if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
			{
				return Subsystem->GetTrajectoryColor(TrajectoryName);
			}
		}
	}
	
	// Default to white if no trajectory available
	return FLinearColor::White;
}

void ACDGKeyframe::UpdateVisualizer()
{
#if WITH_EDITORONLY_DATA
	if (VisualizerComponent)
	{
		VisualizerComponent->FrustumSize = FrustumSize;
		VisualizerComponent->FrustumColor = GetVisualizationColor();
		VisualizerComponent->UpdateVisualization();
		
		// Force recreate the scene proxy to pick up FOV/aspect ratio changes
		VisualizerComponent->RecreateRenderState_Concurrent();
	}
#endif
}

