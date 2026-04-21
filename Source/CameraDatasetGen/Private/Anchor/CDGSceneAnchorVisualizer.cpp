// Copyright Epic Games, Inc. All Rights Reserved.

#include "Anchor/CDGSceneAnchorVisualizer.h"
#include "Anchor/CDGLevelSceneAnchor.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

// ─────────────────────────────────────────────────────────────────────────────
// Scene proxy
// ─────────────────────────────────────────────────────────────────────────────

class FCDGSceneAnchorSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FCDGSceneAnchorSceneProxy(const UCDGSceneAnchorVisualizer* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, Radius(InComponent->DispersionRadius)
		, Color(InComponent->AnchorColor.ToFColor(true))
		, LineThickness(InComponent->LineThickness)
	{
		bWillEverBeLit = false;
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				DrawAnchor(Collector.GetPDI(ViewIndex));
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance      = IsShown(View);
		Result.bDynamicRelevance   = true;
		Result.bShadowRelevance    = false;
		Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize(); }

private:
	void DrawAnchor(FPrimitiveDrawInterface* PDI) const
	{
		const FMatrix& L2W      = GetLocalToWorld();
		const FVector  Center   = L2W.GetOrigin();
		const FVector  XAxis    = L2W.GetUnitAxis(EAxis::X);
		const FVector  YAxis    = L2W.GetUnitAxis(EAxis::Y);
		const FVector  ZAxis    = L2W.GetUnitAxis(EAxis::Z);

		// ── Dispersion radius circle (XY plane, 64 segments) ─────────────────
		constexpr int32 NumSegments = 64;
		for (int32 i = 0; i < NumSegments; ++i)
		{
			const float A0 = (float)i       / NumSegments * 2.f * PI;
			const float A1 = (float)(i + 1) / NumSegments * 2.f * PI;
			const FVector P0 = Center + XAxis * (FMath::Cos(A0) * Radius) + YAxis * (FMath::Sin(A0) * Radius);
			const FVector P1 = Center + XAxis * (FMath::Cos(A1) * Radius) + YAxis * (FMath::Sin(A1) * Radius);
			PDI->DrawLine(P0, P1, Color, SDPG_World, LineThickness);
		}

		// ── Center cross ─────────────────────────────────────────────────────
		const float CrossHalf = FMath::Min(Radius * 0.12f, 20.f);
		PDI->DrawLine(Center - XAxis * CrossHalf, Center + XAxis * CrossHalf, Color, SDPG_World, LineThickness);
		PDI->DrawLine(Center - YAxis * CrossHalf, Center + YAxis * CrossHalf, Color, SDPG_World, LineThickness);

		// ── Upward arrow ─────────────────────────────────────────────────────
		const float ArrowLen  = FMath::Clamp(Radius * 0.35f, 15.f, 80.f);
		const float HeadLen   = ArrowLen * 0.28f;
		const float HeadSpread = HeadLen * 0.45f;
		const FVector ArrowTip = Center + ZAxis * ArrowLen;

		PDI->DrawLine(Center,   ArrowTip,                                                      Color, SDPG_World, LineThickness);
		PDI->DrawLine(ArrowTip, ArrowTip - ZAxis * HeadLen + XAxis * HeadSpread,               Color, SDPG_World, LineThickness);
		PDI->DrawLine(ArrowTip, ArrowTip - ZAxis * HeadLen - XAxis * HeadSpread,               Color, SDPG_World, LineThickness);
		PDI->DrawLine(ArrowTip, ArrowTip - ZAxis * HeadLen + YAxis * HeadSpread,               Color, SDPG_World, LineThickness);
		PDI->DrawLine(ArrowTip, ArrowTip - ZAxis * HeadLen - YAxis * HeadSpread,               Color, SDPG_World, LineThickness);
	}

	float  Radius;
	FColor Color;
	float  LineThickness;
};

// ─────────────────────────────────────────────────────────────────────────────
// UCDGSceneAnchorVisualizer
// ─────────────────────────────────────────────────────────────────────────────

UCDGSceneAnchorVisualizer::UCDGSceneAnchorVisualizer()
{
	PrimaryComponentTick.bCanEverTick = false;
	bHiddenInGame = true;
	bIsEditorOnly = true;
}

FPrimitiveSceneProxy* UCDGSceneAnchorVisualizer::CreateSceneProxy()
{
	return new FCDGSceneAnchorSceneProxy(this);
}

FBoxSphereBounds UCDGSceneAnchorVisualizer::CalcBounds(const FTransform& LocalToWorld) const
{
	const float HalfSize = DispersionRadius + 80.f;  // a little extra for the arrow
	return FBoxSphereBounds(
		LocalToWorld.GetLocation(),
		FVector(HalfSize),
		HalfSize).TransformBy(LocalToWorld);
}

ACDGLevelSceneAnchor* UCDGSceneAnchorVisualizer::GetOwningAnchor() const
{
	return Cast<ACDGLevelSceneAnchor>(GetOwner());
}

void UCDGSceneAnchorVisualizer::UpdateVisualization()
{
	if (const ACDGLevelSceneAnchor* Anchor = GetOwningAnchor())
	{
		DispersionRadius = Anchor->DispersionRadius;
		AnchorColor      = Anchor->AnchorColor;
	}
	MarkRenderStateDirty();
}
