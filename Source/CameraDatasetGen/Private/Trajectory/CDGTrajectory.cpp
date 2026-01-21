// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectoryVisualizer.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"
#include "Components/SplineComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

ACDGTrajectory::ACDGTrajectory()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Create root scene component
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Create spline component
	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SplineComponent->SetupAttachment(RootComponent);
	SplineComponent->bDrawDebug = false;
	SplineComponent->SetHiddenInGame(true);
	// Note: Don't set to Static mobility as it prevents attachment to non-static parent
	
#if WITH_EDITORONLY_DATA
	// Make spline non-selectable and non-editable
	SplineComponent->bSelectable = false;
	SplineComponent->bIsEditorOnly = true;
#endif

#if WITH_EDITORONLY_DATA
	// Create visualizer component for trajectory rendering
	VisualizerComponent = CreateDefaultSubobject<UCDGTrajectoryVisualizer>(TEXT("Visualizer"));
	if (VisualizerComponent)
	{
		VisualizerComponent->SetupAttachment(RootComponent);
		VisualizerComponent->bHiddenInGame = true;
		VisualizerComponent->bSelectable = false;
		VisualizerComponent->SetVisibility(true);
		VisualizerComponent->SetHiddenInGame(false, true); // Show in editor
	}

	// Enable tick in editor for visualization updates
	bRunConstructionScriptOnDrag = false;
	bIsEditorOnlyActor = false;
	
	// Make actor visible but non-selectable
	SetActorHiddenInGame(false); // Show in editor
	
	// Lock transforms but keep editable so it shows in outliner
	bLockLocation = true;
#endif
}

void ACDGTrajectory::BeginPlay()
{
	Super::BeginPlay();
	
	// Register with subsystem
	if (UWorld* World = GetWorld())
	{
		if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
		{
			Subsystem->RegisterTrajectory(this);
		}
	}
	
#if WITH_EDITORONLY_DATA
	// Ensure visualizer is visible in editor
	if (VisualizerComponent && !GetWorld()->IsGameWorld())
	{
		VisualizerComponent->SetVisibility(bShowTrajectory);
	}
#endif
	
	// Initial rebuild
	if (bNeedsRebuild)
	{
		RebuildSpline();
	}
}

void ACDGTrajectory::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
#if WITH_EDITOR
	// In editor, rebuild spline if needed
	if (!GetWorld()->IsGameWorld() && bNeedsRebuild)
	{
		RebuildSpline();
	}
#endif
}

#if WITH_EDITOR
void ACDGTrajectory::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();

	// Handle trajectory name change
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACDGTrajectory, TrajectoryName))
	{
		// Update actor label to match trajectory name
		SetActorLabel(TrajectoryName.ToString());
		
		// Notify subsystem to update trajectory mapping
		if (UWorld* World = GetWorld())
		{
			if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
			{
				Subsystem->OnTrajectoryNameChanged(this);
			}
		}
	}

	// Handle visualization property changes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACDGTrajectory, TrajectoryColor) ||
	    PropertyName == GET_MEMBER_NAME_CHECKED(ACDGTrajectory, bShowTrajectory) ||
	    PropertyName == GET_MEMBER_NAME_CHECKED(ACDGTrajectory, LineThickness) ||
	    PropertyName == GET_MEMBER_NAME_CHECKED(ACDGTrajectory, VisualizationSegments))
	{
		UpdateVisualizer();
	}

	// Handle spline property changes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACDGTrajectory, bClosedLoop))
	{
		if (SplineComponent)
		{
			SplineComponent->SetClosedLoop(bClosedLoop);
		}
		MarkNeedsRebuild();
	}
}

void ACDGTrajectory::PostLoad()
{
	Super::PostLoad();
	
	// Set actor label to match trajectory name
	if (!TrajectoryName.IsNone())
	{
		SetActorLabel(TrajectoryName.ToString());
	}
	
	// Ensure visualizer is set up
	if (VisualizerComponent)
	{
		VisualizerComponent->SetVisibility(bShowTrajectory);
	}
	
	// Mark for rebuild
	MarkNeedsRebuild();
}

void ACDGTrajectory::PostActorCreated()
{
	Super::PostActorCreated();
	
	// Set actor label to match trajectory name
	if (!TrajectoryName.IsNone())
	{
		SetActorLabel(TrajectoryName.ToString());
	}
	
	// Register with subsystem when created
	if (UWorld* World = GetWorld())
	{
		if (UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>())
		{
			Subsystem->RegisterTrajectory(this);
		}
	}
	
	// Ensure visualizer is set up
	if (VisualizerComponent)
	{
		VisualizerComponent->SetVisibility(bShowTrajectory);
	}
	
	// Mark for rebuild
	MarkNeedsRebuild();
}
#endif

// ==================== KEYFRAME MANAGEMENT ====================

void ACDGTrajectory::AddKeyframe(ACDGKeyframe* Keyframe)
{
	if (!Keyframe)
	{
		return;
	}
	
	// Check if keyframe is already in this trajectory
	const bool bAlreadyInTrajectory = Keyframes.Contains(Keyframe);
	
	if (bAlreadyInTrajectory)
	{
		// Keyframe already exists, don't recalculate order (it's just being moved/modified)
		return;
	}

	// This is a new keyframe being added to the trajectory
	Keyframes.Add(Keyframe);
	
	// If this is the first or second keyframe, assign sequential orders
	if (Keyframes.Num() <= 2)
	{
		Keyframe->OrderInTrajectory = Keyframes.Num() - 1;
	}
	else
	{
		// Find the best insertion order based on proximity to existing spline
		const int32 BestOrder = FindBestInsertionOrder(Keyframe->GetActorLocation());
		Keyframe->OrderInTrajectory = BestOrder;
		
		// Sort and reassign all orders to make them sequential
		OnKeyframeOrderManuallyChanged(nullptr);
	}
	
	MarkNeedsRebuild();
}

void ACDGTrajectory::RemoveKeyframe(ACDGKeyframe* Keyframe)
{
	if (!Keyframe)
	{
		return;
	}

	if (Keyframes.Remove(Keyframe) > 0)
	{
		MarkNeedsRebuild();
	}
}

bool ACDGTrajectory::ContainsKeyframe(ACDGKeyframe* Keyframe) const
{
	return Keyframes.Contains(Keyframe);
}

TArray<ACDGKeyframe*> ACDGTrajectory::GetSortedKeyframes() const
{
	TArray<ACDGKeyframe*> SortedKeyframes;
	
	for (const TObjectPtr<ACDGKeyframe>& Keyframe : Keyframes)
	{
		if (Keyframe)
		{
			SortedKeyframes.Add(Keyframe.Get());
		}
	}

	SortedKeyframes.Sort([](const ACDGKeyframe& A, const ACDGKeyframe& B)
	{
		return A.OrderInTrajectory < B.OrderInTrajectory;
	});

	return SortedKeyframes;
}

// ==================== SPLINE GENERATION ====================

void ACDGTrajectory::RebuildSpline()
{
	if (!SplineComponent)
	{
		return;
	}

	// If trajectory is invalid (< 2 keyframes), clear the spline and update visualizer
	if (!IsValid())
	{
		SplineComponent->ClearSplinePoints(true);
		UpdateVisualizer();
		bNeedsRebuild = false;
		return;
	}

	// Sort keyframes by order
	SortKeyframes();

	// Generate spline from keyframes
	GenerateSplineFromKeyframes();

	// Update visualizer
	UpdateVisualizer();

	bNeedsRebuild = false;
	
}

FVector ACDGTrajectory::SamplePosition(float Alpha) const
{
	if (!SplineComponent)
	{
		return FVector::ZeroVector;
	}

	const float Distance = Alpha * SplineComponent->GetSplineLength();
	return SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}

FRotator ACDGTrajectory::SampleRotation(float Alpha) const
{
	if (!SplineComponent)
	{
		return FRotator::ZeroRotator;
	}

	const float Distance = Alpha * SplineComponent->GetSplineLength();
	return SplineComponent->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}

FTransform ACDGTrajectory::SampleTransform(float Alpha) const
{
	if (!SplineComponent)
	{
		return FTransform::Identity;
	}

	const float Distance = Alpha * SplineComponent->GetSplineLength();
	return SplineComponent->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}

float ACDGTrajectory::GetTrajectoryDuration() const
{
	if (Keyframes.Num() == 0)
	{
		return 0.0f;
	}

	// Get sorted keyframes to calculate duration in order
	TArray<ACDGKeyframe*> SortedKeyframes = GetSortedKeyframes();
	
	float TotalDuration = 0.0f;

	for (int32 i = 0; i < SortedKeyframes.Num(); ++i)
	{
		if (!SortedKeyframes[i])
		{
			continue;
		}

		if (i == 0)
		{
			// First keyframe: only count stationary duration
			TotalDuration += SortedKeyframes[i]->TimeAtCurrentFrame;
		}
		else
		{
			// Subsequent keyframes: count both travel and stationary durations
			TotalDuration += SortedKeyframes[i]->TimeToCurrentFrame;
			TotalDuration += SortedKeyframes[i]->TimeAtCurrentFrame;
		}
	}

	return TotalDuration;
}

// ==================== UTILITY ====================

void ACDGTrajectory::SortKeyframes()
{
	Keyframes.Sort([](const ACDGKeyframe& A, const ACDGKeyframe& B)
	{
		return A.OrderInTrajectory < B.OrderInTrajectory;
	});
}

void ACDGTrajectory::AutoAssignKeyframeOrders()
{
	SortKeyframes();

	for (int32 i = 0; i < Keyframes.Num(); ++i)
	{
		if (Keyframes[i])
		{
			Keyframes[i]->OrderInTrajectory = i;
		}
	}

	MarkNeedsRebuild();
	RebuildSpline();
}

void ACDGTrajectory::OnKeyframeOrderManuallyChanged(ACDGKeyframe* ChangedKeyframe)
{
	// If a keyframe manually changed its order, check for conflicts and swap if needed
	if (ChangedKeyframe)
	{
		const int32 TargetOrder = ChangedKeyframe->OrderInTrajectory;
		
		// Find which order is now missing (the order that ChangedKeyframe previously had)
		// This is the "source order" that we'll give to the conflicting keyframe
		TSet<int32> UsedOrders;
		for (const TObjectPtr<ACDGKeyframe>& Keyframe : Keyframes)
		{
			if (Keyframe)
			{
				UsedOrders.Add(Keyframe->OrderInTrajectory);
			}
		}
		
		// Find the missing order (the gap in the sequence)
		int32 SrcOrder = -1;
		for (int32 i = 0; i < Keyframes.Num(); ++i)
		{
			if (!UsedOrders.Contains(i))
			{
				SrcOrder = i;
				break;
			}
		}
		
		// Find if another keyframe has the target order and exchange
		if (SrcOrder >= 0)
		{
			for (TObjectPtr<ACDGKeyframe>& OtherKeyframe : Keyframes)
			{
				if (OtherKeyframe && OtherKeyframe != ChangedKeyframe && 
				    OtherKeyframe->OrderInTrajectory == TargetOrder)
				{
					// Exchange: Give the other keyframe the missing order
					OtherKeyframe->OrderInTrajectory = SrcOrder;
					break;
				}
			}
		}
	}
	
	// Sort keyframes by their current OrderInTrajectory value
	SortKeyframes();
	
	// Reassign orders to all keyframes sequentially
	for (int32 i = 0; i < Keyframes.Num(); ++i)
	{
		if (Keyframes[i])
		{
			Keyframes[i]->OrderInTrajectory = i;
		}
	}
	
	// Rebuild the spline with the new ordering
	MarkNeedsRebuild();
	RebuildSpline();
}

int32 ACDGTrajectory::FindBestInsertionOrder(const FVector& KeyframeLocation) const
{
	if (!SplineComponent || Keyframes.Num() < 2)
	{
		return Keyframes.Num();
	}
	
	// Find the closest point on the spline
	const float ClosestInputKey = SplineComponent->FindInputKeyClosestToWorldLocation(KeyframeLocation);
	
	// Get sorted keyframes to find which segment this belongs to
	TArray<ACDGKeyframe*> SortedKeyframes = GetSortedKeyframes();
	
	// If we only have 2 keyframes, check if we should insert before, between, or after
	if (SortedKeyframes.Num() == 2)
	{
		const float Key0 = 0.0f;
		const float Key1 = 1.0f;
		
		if (ClosestInputKey <= Key0)
		{
			return 0; // Insert before first
		}
		else if (ClosestInputKey >= Key1)
		{
			return 2; // Insert after last
		}
		else
		{
			return 1; // Insert between
		}
	}
	
	// For more keyframes, find which segment is closest
	int32 BestInsertionIndex = SortedKeyframes.Num();
	float MinDistance = FLT_MAX;
	
	// Check each segment between keyframes
	for (int32 i = 0; i < SortedKeyframes.Num() - 1; ++i)
	{
		const float StartKey = static_cast<float>(i);
		const float EndKey = static_cast<float>(i + 1);
		
		// If the closest point is within this segment
		if (ClosestInputKey >= StartKey && ClosestInputKey <= EndKey)
		{
			// This is the segment - insert after keyframe i
			return i + 1;
		}
	}
	
	// If closest key is before first keyframe
	if (ClosestInputKey < 0.0f)
	{
		return 0;
	}
	
	// Otherwise, append to the end
	return SortedKeyframes.Num();
}

void ACDGTrajectory::ValidateKeyframes()
{
	// Remove null keyframes
	Keyframes.RemoveAll([](const TObjectPtr<ACDGKeyframe>& Keyframe)
	{
		return !::IsValid(Keyframe.Get());
	});

	// Check for duplicate orders
	TSet<int32> UsedOrders;
	bool bHasDuplicates = false;

	for (const TObjectPtr<ACDGKeyframe>& Keyframe : Keyframes)
	{
		if (Keyframe)
		{
			if (UsedOrders.Contains(Keyframe->OrderInTrajectory))
			{
				bHasDuplicates = true;
				UE_LOG(LogCameraDatasetGen, Warning, TEXT("Trajectory '%s' has duplicate order: %d"), 
					*TrajectoryName.ToString(), Keyframe->OrderInTrajectory);
			}
			UsedOrders.Add(Keyframe->OrderInTrajectory);
		}
	}

	// Sort keyframes
	SortKeyframes();

	// Mark for rebuild if needed
	if (bHasDuplicates || bNeedsRebuild)
	{
		MarkNeedsRebuild();
	}
}

void ACDGTrajectory::UpdateVisualizer()
{
#if WITH_EDITORONLY_DATA
	if (VisualizerComponent)
	{
		VisualizerComponent->TrajectoryColor = TrajectoryColor;
		VisualizerComponent->LineThickness = LineThickness;
		VisualizerComponent->VisualizationSegments = VisualizationSegments;
		VisualizerComponent->SetVisibility(bShowTrajectory);
		VisualizerComponent->UpdateVisualization();
		
		const bool bHasRenderState = VisualizerComponent->IsRenderStateCreated();
		
		// Ensure the component is registered before recreating render state
		if (!VisualizerComponent->IsRegistered())
		{
			UE_LOG(LogCameraDatasetGen, Warning, TEXT("Visualizer component not registered, registering now"));
			VisualizerComponent->RegisterComponent();
		}
		
		// Create or recreate render state
		if (!bHasRenderState)
		{
			// UE_LOG(LogCameraDatasetGen, Warning, TEXT("Visualizer has no render state, creating now"));
			
			// Make sure component is added to world before creating render state
			if (UWorld* World = GetWorld())
			{
				if (!VisualizerComponent->IsRegistered())
				{
					VisualizerComponent->RegisterComponentWithWorld(World);
				}
			}
			
			VisualizerComponent->MarkRenderStateDirty();
		}
		else
		{
			VisualizerComponent->MarkRenderStateDirty();
		}
	}
#endif
}

// ==================== INTERNAL METHODS ====================

void ACDGTrajectory::GenerateSplineFromKeyframes()
{
	if (!SplineComponent || !IsValid())
	{
		return;
	}

	// Clear existing points
	SplineComponent->ClearSplinePoints(false);

	// Get first keyframe's location as the reference origin
	FVector OriginLocation = FVector::ZeroVector;
	FRotator OriginRotation = FRotator::ZeroRotator;
	
	if (Keyframes.Num() > 0 && Keyframes[0])
	{
		const FTransform FirstTransform = Keyframes[0]->GetKeyframeTransform();
		OriginLocation = FirstTransform.GetLocation();
		OriginRotation = FirstTransform.Rotator();
		
		// Position the trajectory actor at the first keyframe's location
		SetActorLocation(OriginLocation);
		SetActorRotation(FRotator::ZeroRotator); // Keep trajectory actor unrotated for simplicity
	}

	// Add spline points from keyframes using local coordinates
	for (int32 i = 0; i < Keyframes.Num(); ++i)
	{
		const TObjectPtr<ACDGKeyframe>& Keyframe = Keyframes[i];
		if (!Keyframe)
		{
			continue;
		}

		const FTransform Transform = Keyframe->GetKeyframeTransform();
		const FVector WorldLocation = Transform.GetLocation();
		const FRotator WorldRotation = Transform.Rotator();

		// Convert to local coordinates relative to the trajectory actor
		const FVector LocalLocation = WorldLocation - OriginLocation;
		const FRotator LocalRotation = WorldRotation; // Keep world rotation for now

		// Convert interpolation mode to spline point type
		const ESplinePointType::Type PointType = ConvertInterpolationMode(
			Keyframe->InterpolationSettings.PositionInterpMode
		);

		// Add point to spline using local coordinates
		SplineComponent->AddSplinePoint(LocalLocation, ESplineCoordinateSpace::Local, false);
		SplineComponent->SetRotationAtSplinePoint(i, LocalRotation, ESplineCoordinateSpace::World, false);
		SplineComponent->SetSplinePointType(i, PointType, false);
	}

	// Set closed loop
	SplineComponent->SetClosedLoop(bClosedLoop, false);

	// Apply custom tangents if needed
	ApplyInterpolationSettings();

	// Update spline
	SplineComponent->UpdateSpline();
}

void ACDGTrajectory::ApplyInterpolationSettings()
{
	if (!SplineComponent)
	{
		return;
	}

	for (int32 i = 0; i < Keyframes.Num(); ++i)
	{
		const TObjectPtr<ACDGKeyframe>& Keyframe = Keyframes[i];
		if (!Keyframe)
		{
			continue;
		}

		const FCDGSplineInterpolationSettings& Settings = Keyframe->InterpolationSettings;

		// Apply custom tangents if using custom tangent mode
		if (Settings.PositionTangentMode == ECDGTangentMode::User || 
		    Settings.PositionTangentMode == ECDGTangentMode::Break)
		{
			// Tangents are in local space since spline points are in local space
			SplineComponent->SetTangentAtSplinePoint(i, Settings.PositionLeaveTangent, ESplineCoordinateSpace::Local, false);
		}
	}

	SplineComponent->UpdateSpline();
}

ESplinePointType::Type ACDGTrajectory::ConvertInterpolationMode(ECDGInterpolationMode Mode) const
{
	switch (Mode)
	{
		case ECDGInterpolationMode::Linear:
			return ESplinePointType::Linear;
			
		case ECDGInterpolationMode::Cubic:
		case ECDGInterpolationMode::CubicClamped:
			return ESplinePointType::Curve;
			
		case ECDGInterpolationMode::Constant:
			return ESplinePointType::Constant;
			
		case ECDGInterpolationMode::CustomTangent:
			return ESplinePointType::CurveClamped;
			
		default:
			return ESplinePointType::Curve;
	}
}


