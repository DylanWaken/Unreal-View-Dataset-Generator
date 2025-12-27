// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "CDGKeyframeVisualizer.generated.h"

class ACDGKeyframe;

/**
 * CDGKeyframeVisualizer
 * 
 * Custom primitive component for rendering camera frustum visualization in editor.
 * Similar to the camera frustum visualization in Blender.
 * 
 * This component:
 * - Renders a 3D camera frustum with proper perspective
 * - Shows focal point indicator
 * - Displays camera parameters visually
 * - Only renders in editor, hidden during play/render/MRQ
 */
UCLASS(ClassGroup = "CameraDatasetGen", hidecategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming), editinlinenew, meta = (BlueprintSpawnableComponent))
class CAMERADATASETGEN_API UCDGKeyframeVisualizer : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UCDGKeyframeVisualizer();

	// ==================== VISUALIZATION SETTINGS ====================

	/** Size of the frustum visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float FrustumSize = 100.0f;

	/** Color of the frustum */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FLinearColor FrustumColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);

	/** Thickness of the frustum lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float LineThickness = 2.0f;

	/** Show focal point indicator */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool bShowFocalPoint = true;

	/** Show camera body (pyramid shape) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool bShowCameraBody = true;

	// ==================== PRIMITIVE COMPONENT INTERFACE ====================

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	// ==================== UTILITY ====================

	/** Get the owning keyframe actor */
	ACDGKeyframe* GetOwningKeyframe() const;

	/** Update visualization based on keyframe settings */
	void UpdateVisualization();
};

