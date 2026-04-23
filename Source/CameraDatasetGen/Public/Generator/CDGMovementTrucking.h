// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "CDGMovementTrucking.generated.h"

/**
 * UCDGMovementTrucking
 *
 * Movement generator that slides the camera laterally (perpendicular to the
 * look-at direction in the horizontal plane) — the "trucking" or "crabbing"
 * shot.
 *
 * The camera starts at the placement position and moves TruckDistance
 * centimetres to the right (positive) or left (negative) while maintaining a
 * live look-at toward the anchor at every keyframe.
 *
 * Unlike a pan, the camera's world position changes; unlike a tracking shot,
 * the motion is purely lateral rather than following the subject.  Trucking
 * reveals new spatial information alongside the action.
 *
 * References:
 *   - Mascelli, J.V. (1965). The Five C's of Cinematography. Silman-James Press.
 *   - Brown, B. (2012). Cinematography: Theory and Practice, 2nd ed. Focal Press.
 *     Chapter 6: "Trucking — Lateral Dolly Movement".
 *
 * Pipeline order:  POSITIONING  →  MovementTrucking  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementTrucking : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/**
	 * Lateral displacement (cm).  Positive = truck right relative to the
	 * initial camera facing direction; negative = truck left.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (UIMin = "-1000.0", UIMax = "1000.0"))
	float TruckDistance = 300.0f;

	/** Number of keyframes distributed across the reference sequence duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "2", ClampMax = "240"))
	int32 NumKeyframes = 12;

	/** Speed profile for the lateral movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	ECDGSpeedInterpolationMode SpeedInterpolation = ECDGSpeedInterpolationMode::Linear;

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
