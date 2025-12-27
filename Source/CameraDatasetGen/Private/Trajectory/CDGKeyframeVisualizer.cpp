// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trajectory/CDGKeyframeVisualizer.h"
#include "Trajectory/CDGKeyframe.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "DynamicMeshBuilder.h"
#include "PrimitiveViewRelevance.h"
#include "SceneView.h"

/**
 * Scene proxy for rendering the camera frustum visualization
 */
class FCDGKeyframeSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FCDGKeyframeSceneProxy(const UCDGKeyframeVisualizer* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, FrustumSize(InComponent->FrustumSize)
		, FrustumColor(InComponent->FrustumColor)
		, LineThickness(InComponent->LineThickness)
		, bShowFocalPoint(InComponent->bShowFocalPoint)
		, bShowCameraBody(InComponent->bShowCameraBody)
	{
		// Get camera parameters from owning keyframe
		if (const ACDGKeyframe* Keyframe = InComponent->GetOwningKeyframe())
		{
			FieldOfView = Keyframe->LensSettings.FieldOfView;
			AspectRatio = Keyframe->FilmbackSettings.SensorAspectRatio;
			FocusDistance = Keyframe->LensSettings.FocusDistance;
		}
		else
		{
			FieldOfView = 90.0f;
			AspectRatio = 1.777778f;
			FocusDistance = 100000.0f;
		}

		bWillEverBeLit = false;
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_CDGKeyframeSceneProxy_GetDynamicMeshElements);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

				DrawFrustum(PDI);
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
		return FPrimitiveSceneProxy::GetAllocatedSize();
	}

private:
	void DrawFrustum(FPrimitiveDrawInterface* PDI) const
	{
		const FMatrix LocalToWorldMatrix = GetLocalToWorld();
		const FVector Location = LocalToWorldMatrix.GetOrigin();
		const FVector ForwardVector = LocalToWorldMatrix.GetUnitAxis(EAxis::X);
		const FVector RightVector = LocalToWorldMatrix.GetUnitAxis(EAxis::Y);
		const FVector UpVector = LocalToWorldMatrix.GetUnitAxis(EAxis::Z);

		// Calculate frustum dimensions
		const float NearPlane = 10.0f;
		const float FarPlane = FrustumSize;
		const float FOVRadians = FMath::DegreesToRadians(FieldOfView);
		const float HalfFOVRadians = FOVRadians * 0.5f;

		// Calculate near plane dimensions
		const float NearHeight = 2.0f * FMath::Tan(HalfFOVRadians) * NearPlane;
		const float NearWidth = NearHeight * AspectRatio;

		// Calculate far plane dimensions
		const float FarHeight = 2.0f * FMath::Tan(HalfFOVRadians) * FarPlane;
		const float FarWidth = FarHeight * AspectRatio;

		// Near plane corners
		const FVector NearCenter = Location + ForwardVector * NearPlane;
		const FVector NearTopLeft = NearCenter + UpVector * (NearHeight * 0.5f) - RightVector * (NearWidth * 0.5f);
		const FVector NearTopRight = NearCenter + UpVector * (NearHeight * 0.5f) + RightVector * (NearWidth * 0.5f);
		const FVector NearBottomLeft = NearCenter - UpVector * (NearHeight * 0.5f) - RightVector * (NearWidth * 0.5f);
		const FVector NearBottomRight = NearCenter - UpVector * (NearHeight * 0.5f) + RightVector * (NearWidth * 0.5f);

		// Far plane corners
		const FVector FarCenter = Location + ForwardVector * FarPlane;
		const FVector FarTopLeft = FarCenter + UpVector * (FarHeight * 0.5f) - RightVector * (FarWidth * 0.5f);
		const FVector FarTopRight = FarCenter + UpVector * (FarHeight * 0.5f) + RightVector * (FarWidth * 0.5f);
		const FVector FarBottomLeft = FarCenter - UpVector * (FarHeight * 0.5f) - RightVector * (FarWidth * 0.5f);
		const FVector FarBottomRight = FarCenter - UpVector * (FarHeight * 0.5f) + RightVector * (FarWidth * 0.5f);

		const FColor DrawColor = FrustumColor.ToFColor(true);
		const uint8 DepthPriority = SDPG_World;

		// Draw near plane rectangle
		PDI->DrawLine(NearTopLeft, NearTopRight, DrawColor, DepthPriority, LineThickness);
		PDI->DrawLine(NearTopRight, NearBottomRight, DrawColor, DepthPriority, LineThickness);
		PDI->DrawLine(NearBottomRight, NearBottomLeft, DrawColor, DepthPriority, LineThickness);
		PDI->DrawLine(NearBottomLeft, NearTopLeft, DrawColor, DepthPriority, LineThickness);

		// Draw far plane rectangle
		PDI->DrawLine(FarTopLeft, FarTopRight, DrawColor, DepthPriority, LineThickness);
		PDI->DrawLine(FarTopRight, FarBottomRight, DrawColor, DepthPriority, LineThickness);
		PDI->DrawLine(FarBottomRight, FarBottomLeft, DrawColor, DepthPriority, LineThickness);
		PDI->DrawLine(FarBottomLeft, FarTopLeft, DrawColor, DepthPriority, LineThickness);

		// Draw connecting lines (frustum edges)
		PDI->DrawLine(Location, FarTopLeft, DrawColor, DepthPriority, LineThickness);
		PDI->DrawLine(Location, FarTopRight, DrawColor, DepthPriority, LineThickness);
		PDI->DrawLine(Location, FarBottomLeft, DrawColor, DepthPriority, LineThickness);
		PDI->DrawLine(Location, FarBottomRight, DrawColor, DepthPriority, LineThickness);

		// Draw camera body (pyramid)
		if (bShowCameraBody)
		{
			const float BodySize = 20.0f;
			const FVector BodyBack = Location - ForwardVector * BodySize;
			const FVector BodyTopLeft = Location + UpVector * BodySize - RightVector * BodySize;
			const FVector BodyTopRight = Location + UpVector * BodySize + RightVector * BodySize;
			const FVector BodyBottomLeft = Location - UpVector * BodySize - RightVector * BodySize;
			const FVector BodyBottomRight = Location - UpVector * BodySize + RightVector * BodySize;

			const FColor BodyColor = FColor::White;
			PDI->DrawLine(BodyBack, BodyTopLeft, BodyColor, DepthPriority, LineThickness * 0.75f);
			PDI->DrawLine(BodyBack, BodyTopRight, BodyColor, DepthPriority, LineThickness * 0.75f);
			PDI->DrawLine(BodyBack, BodyBottomLeft, BodyColor, DepthPriority, LineThickness * 0.75f);
			PDI->DrawLine(BodyBack, BodyBottomRight, BodyColor, DepthPriority, LineThickness * 0.75f);
			PDI->DrawLine(BodyTopLeft, BodyTopRight, BodyColor, DepthPriority, LineThickness * 0.75f);
			PDI->DrawLine(BodyBottomLeft, BodyBottomRight, BodyColor, DepthPriority, LineThickness * 0.75f);
		}

		// Draw center line (forward direction indicator)
		const FColor CenterLineColor = FColor::Yellow;
		PDI->DrawLine(Location, FarCenter, CenterLineColor, DepthPriority, LineThickness * 1.5f);

		// Draw focal point indicator
		if (bShowFocalPoint && FocusDistance > 0.0f)
		{
			const float FocusDistanceUnits = FocusDistance / 100.0f; // Convert cm to units
			if (FocusDistanceUnits < FarPlane)
			{
				const FVector FocusPoint = Location + ForwardVector * FocusDistanceUnits;
				const float FocusIndicatorSize = 20.0f;

				const FColor FocusColor = FColor::Cyan;
				PDI->DrawLine(FocusPoint - RightVector * FocusIndicatorSize, FocusPoint + RightVector * FocusIndicatorSize, FocusColor, DepthPriority, LineThickness);
				PDI->DrawLine(FocusPoint - UpVector * FocusIndicatorSize, FocusPoint + UpVector * FocusIndicatorSize, FocusColor, DepthPriority, LineThickness);

				// Draw circle at focus point
				const int32 NumSegments = 16;
				for (int32 i = 0; i < NumSegments; ++i)
				{
					const float Angle1 = (float)i / (float)NumSegments * 2.0f * PI;
					const float Angle2 = (float)(i + 1) / (float)NumSegments * 2.0f * PI;

					const FVector Point1 = FocusPoint + RightVector * FMath::Cos(Angle1) * FocusIndicatorSize + UpVector * FMath::Sin(Angle1) * FocusIndicatorSize;
					const FVector Point2 = FocusPoint + RightVector * FMath::Cos(Angle2) * FocusIndicatorSize + UpVector * FMath::Sin(Angle2) * FocusIndicatorSize;

					PDI->DrawLine(Point1, Point2, FocusColor, DepthPriority, LineThickness * 0.5f);
				}
			}
		}
	}

	float FrustumSize;
	FLinearColor FrustumColor;
	float LineThickness;
	bool bShowFocalPoint;
	bool bShowCameraBody;

	// Camera parameters
	float FieldOfView;
	float AspectRatio;
	float FocusDistance;
};

// ==================== UCDGKeyframeVisualizer Implementation ====================

UCDGKeyframeVisualizer::UCDGKeyframeVisualizer()
{
	PrimaryComponentTick.bCanEverTick = false;
	bHiddenInGame = true;
	bVisibleInReflectionCaptures = false;
	bVisibleInRayTracing = false;
	bVisibleInRealTimeSkyCaptures = false;
	SetCastShadow(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

#if WITH_EDITORONLY_DATA
	bIsEditorOnly = true;
#endif
}

FPrimitiveSceneProxy* UCDGKeyframeVisualizer::CreateSceneProxy()
{
#if WITH_EDITOR
	// Only create scene proxy in editor
	if (GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
	{
		return new FCDGKeyframeSceneProxy(this);
	}
#endif
	return nullptr;
}

FBoxSphereBounds UCDGKeyframeVisualizer::CalcBounds(const FTransform& LocalToWorld) const
{
	// Calculate bounds based on frustum size
	const float BoundsRadius = FrustumSize * 1.5f;
	return FBoxSphereBounds(FVector::ZeroVector, FVector(BoundsRadius), BoundsRadius).TransformBy(LocalToWorld);
}

ACDGKeyframe* UCDGKeyframeVisualizer::GetOwningKeyframe() const
{
	return Cast<ACDGKeyframe>(GetOwner());
}

void UCDGKeyframeVisualizer::UpdateVisualization()
{
	MarkRenderStateDirty();
}

