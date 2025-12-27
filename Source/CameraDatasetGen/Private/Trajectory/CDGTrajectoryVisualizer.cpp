// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trajectory/CDGTrajectoryVisualizer.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGKeyframe.h"
#include "LogCameraDatasetGen.h"
#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "DynamicMeshBuilder.h"
#include "PrimitiveViewRelevance.h"
#include "SceneView.h"
#include "Components/SplineComponent.h"
#include "RenderResource.h"
#include "VertexFactory.h"

/**
 * Scene proxy for rendering the trajectory spline visualization
 */
class FCDGTrajectorySceneProxy final : public FPrimitiveSceneProxy
{
public:
	FCDGTrajectorySceneProxy(const UCDGTrajectoryVisualizer* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, TrajectoryColor(InComponent->TrajectoryColor)
		, LineThickness(InComponent->LineThickness)
		, VisualizationSegments(InComponent->VisualizationSegments)
		, bShowKeyframePoints(InComponent->bShowKeyframePoints)
		, KeyframePointSize(InComponent->KeyframePointSize)
		, bShowDirectionIndicators(InComponent->bShowDirectionIndicators)
		, DirectionIndicatorSpacing(InComponent->DirectionIndicatorSpacing)
		, SplineLength(0.0f)
	{
		bWillEverBeLit = false;

		// Get trajectory data from owning actor
		if (const ACDGTrajectory* Trajectory = InComponent->GetOwningTrajectory())
		{
			if (Trajectory->SplineComponent)
			{
				USplineComponent* Spline = Trajectory->SplineComponent;
				
				// Cache spline length
				SplineLength = Spline->GetSplineLength();
				
				// Only generate visualization if spline has valid length
				if (SplineLength > 0.0f)
				{
					// Cache spline points for rendering
					const int32 NumSegments = FMath::Max(VisualizationSegments, 10);
					SplinePoints.Reserve(NumSegments + 1);
					
					for (int32 i = 0; i <= NumSegments; ++i)
					{
						const float Alpha = (float)i / (float)NumSegments;
						const float Distance = Alpha * SplineLength;
						const FVector Location = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
						const FVector Direction = Spline->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
						
						SplinePoints.Add(FSplinePointData{Location, Direction});
					}

					// Cache keyframe positions
					KeyframePositions.Reserve(Trajectory->Keyframes.Num());
					for (const TObjectPtr<ACDGKeyframe>& Keyframe : Trajectory->Keyframes)
					{
						if (Keyframe)
						{
							KeyframePositions.Add(Keyframe->GetActorLocation());
						}
					}
				}
			}
		}
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_CDGTrajectorySceneProxy_GetDynamicMeshElements);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

				DrawTrajectory(PDI);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = false;
		Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	uint32 GetAllocatedSize() const
	{
		return FPrimitiveSceneProxy::GetAllocatedSize() + 
		       SplinePoints.GetAllocatedSize() + 
		       KeyframePositions.GetAllocatedSize();
	}

private:
	struct FSplinePointData
	{
		FVector Location;
		FVector Direction;
	};

	void DrawTrajectory(FPrimitiveDrawInterface* PDI) const
	{
		if (SplinePoints.Num() < 2)
		{
			return;
		}

		const FColor DrawColor = TrajectoryColor.ToFColor(true);
		const uint8 DepthPriority = SDPG_World;

		// Draw spline segments
		for (int32 i = 0; i < SplinePoints.Num() - 1; ++i)
		{
			const FVector& Point1 = SplinePoints[i].Location;
			const FVector& Point2 = SplinePoints[i + 1].Location;
			
			PDI->DrawLine(Point1, Point2, DrawColor, DepthPriority, LineThickness);
		}

		// Draw keyframe points
		if (bShowKeyframePoints)
		{
			for (const FVector& KeyframePos : KeyframePositions)
			{
				// Draw sphere at keyframe position
				DrawWireSphere(PDI, KeyframePos, DrawColor, KeyframePointSize, 12, DepthPriority, LineThickness);
			}
		}

		// Draw direction indicators
		if (bShowDirectionIndicators && SplineLength > 0.0f && DirectionIndicatorSpacing > 0.0f)
		{
			const int32 NumIndicators = FMath::Max(1, FMath::FloorToInt(SplineLength / DirectionIndicatorSpacing));
			const FColor DirectionColor = FColor::Yellow;
			const float ArrowSize = 40.0f; // Increased from 30.0f

			for (int32 i = 1; i < NumIndicators; ++i) // Start at 1 to skip the first keyframe
			{
				const float Distance = (float)i * DirectionIndicatorSpacing;
				if (Distance >= SplineLength)
				{
					break;
				}

				// Find closest spline point
				const int32 PointIndex = FMath::Clamp(
					FMath::FloorToInt((Distance / SplineLength) * (SplinePoints.Num() - 1)),
					0, SplinePoints.Num() - 1);

				if (PointIndex >= 0 && PointIndex < SplinePoints.Num())
				{
					const FVector& Location = SplinePoints[PointIndex].Location;
					const FVector& Direction = SplinePoints[PointIndex].Direction;

					// Calculate arrow head position
					const FVector ArrowEnd = Location + Direction * ArrowSize;

					// Draw arrow head only (no shaft)
					const FVector Right = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
					const FVector ArrowLeft = ArrowEnd - Direction * (ArrowSize * 0.4f) - Right * (ArrowSize * 0.25f);
					const FVector ArrowRight = ArrowEnd - Direction * (ArrowSize * 0.4f) + Right * (ArrowSize * 0.25f);
					
					PDI->DrawLine(ArrowEnd, ArrowLeft, DirectionColor, DepthPriority, LineThickness * 0.75f);
					PDI->DrawLine(ArrowEnd, ArrowRight, DirectionColor, DepthPriority, LineThickness * 0.75f);
				}
			}
		}
	}

	FLinearColor TrajectoryColor;
	float LineThickness;
	int32 VisualizationSegments;
	bool bShowKeyframePoints;
	float KeyframePointSize;
	bool bShowDirectionIndicators;
	float DirectionIndicatorSpacing;

	// Cached spline data
	float SplineLength;
	TArray<FSplinePointData> SplinePoints;
	TArray<FVector> KeyframePositions;
};

// ==================== UCDGTrajectoryVisualizer Implementation ====================

UCDGTrajectoryVisualizer::UCDGTrajectoryVisualizer()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	// Visibility settings
	SetVisibility(true);
	bHiddenInGame = true;
	
	// Rendering settings
	bVisibleInReflectionCaptures = false;
	bVisibleInRayTracing = false;
	bVisibleInRealTimeSkyCaptures = false;
	SetCastShadow(false);
	
	// Collision
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// Force this component to always render
	bUseAsOccluder = false;
	bRenderCustomDepth = false;
	bRenderInMainPass = true;
	bRenderInDepthPass = false;

#if WITH_EDITORONLY_DATA
	bIsEditorOnly = true;
#endif
}

FPrimitiveSceneProxy* UCDGTrajectoryVisualizer::CreateSceneProxy()
{
	const ACDGTrajectory* Trajectory = GetOwningTrajectory();
	
	// Always create scene proxy if we have a valid trajectory
	if (Trajectory && Trajectory->SplineComponent)
	{
		return new FCDGTrajectorySceneProxy(this);
	}
	
	UE_LOG(LogCameraDatasetGen, Error, TEXT("CreateSceneProxy failed - no trajectory or spline component"));
	return nullptr;
}

FBoxSphereBounds UCDGTrajectoryVisualizer::CalcBounds(const FTransform& LocalToWorld) const
{
	// Calculate bounds based on spline
	if (const ACDGTrajectory* Trajectory = GetOwningTrajectory())
	{
		if (Trajectory->SplineComponent && Trajectory->Keyframes.Num() > 0)
		{
			return Trajectory->SplineComponent->CalcBounds(LocalToWorld);
		}
	}

	// Default bounds - large enough to always be visible
	const float BoundsRadius = 100000.0f;
	return FBoxSphereBounds(FVector::ZeroVector, FVector(BoundsRadius), BoundsRadius).TransformBy(LocalToWorld);
}

ACDGTrajectory* UCDGTrajectoryVisualizer::GetOwningTrajectory() const
{
	return Cast<ACDGTrajectory>(GetOwner());
}

void UCDGTrajectoryVisualizer::UpdateVisualization()
{
	MarkRenderStateDirty();
}

