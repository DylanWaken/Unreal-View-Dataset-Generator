// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "CDGSceneAnchorVisualizer.generated.h"

class ACDGLevelSceneAnchor;

/**
 * UCDGSceneAnchorVisualizer
 *
 * Custom primitive component that renders the LevelSceneAnchor placement radius as a
 * flat circle in the XY plane, a center cross and an upward arrow in the editor viewport.
 * Hidden during play/render.
 */
UCLASS(ClassGroup = "CameraDatasetGen",
	hidecategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming),
	editinlinenew,
	meta = (BlueprintSpawnableComponent))
class CAMERADATASETGEN_API UCDGSceneAnchorVisualizer : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UCDGSceneAnchorVisualizer();

	/** Radius of the drawn circle (synced from owning anchor's DispersionRadius) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float DispersionRadius = 150.0f;

	/** Color of the visualization lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FLinearColor AnchorColor = FLinearColor(0.2f, 0.85f, 0.95f, 1.0f);

	/** Line thickness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float LineThickness = 2.0f;

	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	/** Get the owning LevelSceneAnchor actor */
	ACDGLevelSceneAnchor* GetOwningAnchor() const;

	/** Sync visualization parameters from the owning anchor */
	void UpdateVisualization();
};
