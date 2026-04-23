// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "CDGMovementCameraRoll.generated.h"

/**
 * UCDGMovementCameraRoll
 *
 * Movement generator that rotates the camera about its own forward (optical)
 * axis — a "barrel roll" or Dutch-angle sweep.
 *
 * Camera position is fixed at the placement point.  Look-at direction toward
 * the anchor is computed once and held constant.  The roll angle transitions
 * from StartRollAngle to EndRollAngle across NumKeyframes keyframes.
 *
 * A static Dutch angle (StartRollAngle == EndRollAngle ≠ 0) expresses
 * unease or disorientation.  A dynamic roll transition can accompany a
 * character's psychological shift.
 *
 * References:
 *   - Katz, S.D. (1991). Film Directing Shot by Shot. Michael Wiese Productions.
 *     "Dutch Angle and Canted Framing", pp. 203–205.
 *   - Mercado, G. (2011). The Filmmaker's Eye. Focal Press.
 *     Chapter 6: "Dutch Angle — Tension and Disorientation".
 *
 * Pipeline order:  POSITIONING  →  MovementCameraRoll  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementCameraRoll : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/** Roll angle (degrees) at the first keyframe.  0 = upright. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float StartRollAngle = 0.0f;

	/** Roll angle (degrees) at the last keyframe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float EndRollAngle = 45.0f;

	/** Number of keyframes distributed across the reference sequence duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "2", ClampMax = "240"))
	int32 NumKeyframes = 24;

	/** Speed profile for the roll transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	ECDGSpeedInterpolationMode SpeedInterpolation = ECDGSpeedInterpolationMode::SlowInOut;

	// ==================== ADVANCED ====================

	/** Aperture (f-stop) applied to every spawned keyframe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement|Advanced",
		meta = (ClampMin = "1.2", ClampMax = "22.0"))
	float DefaultAperture = 2.8f;

	// ==================== OVERRIDES ====================

	virtual FName GetGeneratorName_Implementation() const override;
	virtual FText GetTip_Implementation() const override;
	virtual TArray<ACDGTrajectory*> GenerateMovement_Implementation(const TArray<FCDGCameraPlacement>& Placements) override;
	virtual void SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const override;
	virtual void FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson) override;
};
