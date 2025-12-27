// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "CDGTrajectoryVisualizer.generated.h"

class ACDGTrajectory;

/**
 * CDGTrajectoryVisualizer
 * 
 * Custom primitive component for rendering trajectory spline paths in editor.
 * Visualizes the camera path with customizable line thickness and color.
 * 
 * This component:
 * - Renders a 3D spline path with proper interpolation
 * - Shows keyframe positions as spheres
 * - Displays direction indicators
 * - Only renders in editor, hidden during play/render/MRQ
 */
UCLASS(ClassGroup = "CameraDatasetGen", hidecategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming), editinlinenew, meta = (BlueprintSpawnableComponent))
class CAMERADATASETGEN_API UCDGTrajectoryVisualizer : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UCDGTrajectoryVisualizer();

	// ==================== VISUALIZATION SETTINGS ====================

	/** Color of the trajectory line */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FLinearColor TrajectoryColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);

	/** Thickness of the trajectory lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float LineThickness = 3.0f;

	/** Number of segments to render the spline */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	int32 VisualizationSegments = 50;

	/** Show keyframe positions as spheres */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool bShowKeyframePoints = true;

	/** Size of keyframe point spheres */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float KeyframePointSize = 15.0f;

	/** Show direction indicators along the path */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool bShowDirectionIndicators = true;

	/** Spacing for direction indicators */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float DirectionIndicatorSpacing = 100.0f;

	// ==================== PRIMITIVE COMPONENT INTERFACE ====================

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool ShouldCreateRenderState() const override { return true; }

	// ==================== UTILITY ====================

	/** Get the owning trajectory actor */
	ACDGTrajectory* GetOwningTrajectory() const;

	/** Update visualization based on trajectory settings */
	void UpdateVisualization();
};

