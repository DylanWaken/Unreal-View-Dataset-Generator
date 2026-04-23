// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "CDGMovementPan.generated.h"

/**
 * UCDGMovementPan
 *
 * Movement generator that rotates the camera horizontally about a fixed world
 * position — the classical film "pan" shot.
 *
 * The camera holds its placement position throughout the entire shot.  Two
 * look-at angles (PanStartAngle / PanEndAngle), each expressed as a yaw offset
 * in degrees from the direct look-at direction toward the anchor, are linearly
 * distributed across NumKeyframes keyframes spanning the reference sequence
 * duration.
 *
 * References:
 *   - Katz, S.D. (1991). Film Directing Shot by Shot. Michael Wiese Productions.
 *   - Brown, B. (2012). Cinematography: Theory and Practice, 2nd ed. Focal Press.
 *     Chapter 6: Camera Movement — "The Pan".
 *
 * Pipeline order:  POSITIONING  →  MovementPan  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementPan : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/**
	 * Yaw offset (degrees) at the start of the pan, relative to the direct
	 * look-at direction toward the anchor.
	 * Negative = camera starts panning from the left of the subject.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "-180.0", ClampMax = "180.0", UIMin = "-90.0", UIMax = "90.0"))
	float PanStartAngle = -30.0f;

	/**
	 * Yaw offset (degrees) at the end of the pan, relative to the direct
	 * look-at direction toward the anchor.
	 * Positive = camera ends panning to the right of the subject.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "-180.0", ClampMax = "180.0", UIMin = "-90.0", UIMax = "90.0"))
	float PanEndAngle = 30.0f;

	/**
	 * Additional vertical pitch offset (degrees) applied constantly throughout
	 * the pan.  Y component shifts the framing up (positive) or down (negative).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	float PitchOffset = 0.0f;

	/** Number of keyframes to distribute across the reference sequence duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "2", ClampMax = "240"))
	int32 NumKeyframes = 12;

	/** Speed profile used for camera-head rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	ECDGSpeedInterpolationMode SpeedInterpolation = ECDGSpeedInterpolationMode::SlowInOut;

	// ==================== ADVANCED ====================

	/** Aperture (f-stop) applied to every spawned keyframe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement|Advanced",
		meta = (ClampMin = "1.2", ClampMax = "22.0"))
	float DefaultAperture = 5.6f;

	// ==================== OVERRIDES ====================

	virtual FName GetGeneratorName_Implementation() const override;
	virtual FText GetTip_Implementation() const override;
	virtual TArray<ACDGTrajectory*> GenerateMovement_Implementation(const TArray<FCDGCameraPlacement>& Placements) override;
	virtual void SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const override;
	virtual void FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson) override;
};
