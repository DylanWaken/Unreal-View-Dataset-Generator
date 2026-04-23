// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "CDGMovementTilt.generated.h"

/**
 * UCDGMovementTilt
 *
 * Movement generator that rotates the camera vertically about a fixed world
 * position — the classical film "tilt" shot.
 *
 * Camera translation is fixed at the placement position.  Two pitch offsets
 * (TiltStartAngle / TiltEndAngle), each in degrees from the direct look-at
 * direction toward the anchor, are interpolated across NumKeyframes keyframes
 * spanning the reference sequence duration.
 *
 * Common uses: reveal a character's full height, follow a falling object, or
 * disclose a dramatic environment element above or below the frame.
 *
 * References:
 *   - Katz, S.D. (1991). Film Directing Shot by Shot. Michael Wiese Productions.
 *   - Brown, B. (2012). Cinematography: Theory and Practice, 2nd ed. Focal Press.
 *     Chapter 6: "The Tilt".
 *
 * Pipeline order:  POSITIONING  →  MovementTilt  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementTilt : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/**
	 * Pitch offset (degrees) at the start of the tilt, relative to the direct
	 * look-at direction toward the anchor.
	 * Negative = camera starts tilted downward.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float TiltStartAngle = -15.0f;

	/**
	 * Pitch offset (degrees) at the end of the tilt, relative to the direct
	 * look-at direction toward the anchor.
	 * Positive = camera ends tilted upward.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float TiltEndAngle = 30.0f;

	/** Additional constant horizontal yaw offset throughout the tilt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float YawOffset = 0.0f;

	/** Number of keyframes to distribute across the reference sequence duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "2", ClampMax = "240"))
	int32 NumKeyframes = 12;

	/** Speed profile used for the vertical rotation. */
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
