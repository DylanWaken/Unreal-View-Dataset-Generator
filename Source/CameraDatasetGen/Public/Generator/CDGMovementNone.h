// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "CDGMovementNone.generated.h"

/**
 * UCDGMovementNone
 *
 * Movement generator that produces static or anchor-following camera shots with
 * no translational camera motion.
 *
 * Two sub-modes controlled by bFollowAnchor:
 *
 *   Static (bFollowAnchor = false):
 *     Two keyframes placed at the sequence start and end.  The camera holds its
 *     sampled position and a fixed look-at rotation toward the anchor's first-
 *     frame world position.
 *
 *   Follow (bFollowAnchor = true):
 *     One keyframe per display-rate frame of the reference sequence.  Camera
 *     translation remains fixed at the placement position while the rotation
 *     tracks the anchor's world position as it moves through the sequence.
 *
 * This is the MOVEMENT stage implementation for the "Static" pipeline preset:
 *   SphericalPositioning  →  MovementNone  →  EffectsNone
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementNone : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/**
	 * When true, one keyframe per display-rate frame tracks the anchor's
	 * world position across the full reference sequence.
	 * When false, two keyframes bracket the sequence with a fixed look-at.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	bool bFollowAnchor = false;

	/**
	 * Additional angular offset (degrees) applied to the base look-at rotation
	 * in camera space.  X = horizontal (yaw), Y = vertical (pitch).
	 * Positive X rotates the camera to the right; positive Y rotates upward.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	FVector2D ViewDirectionDeviation = FVector2D::ZeroVector;

	// ==================== ADVANCED ====================

	/**
	 * Aperture (f-stop) applied to every spawned keyframe.
	 * A high value (e.g. 15) gives a large depth of field suitable for
	 * static overview shots.  Lower values produce a shallower depth of field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement|Advanced",
		meta = (ClampMin = "1.2", ClampMax = "22.0"))
	float DefaultAperture = 15.0f;

	// ==================== OVERRIDES ====================

	virtual FName GetGeneratorName_Implementation() const override;
	virtual FText GetTip_Implementation() const override;
	virtual TArray<ACDGTrajectory*> GenerateMovement_Implementation(const TArray<FCDGCameraPlacement>& Placements) override;
	virtual void SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const override;
	virtual void FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson) override;

};
