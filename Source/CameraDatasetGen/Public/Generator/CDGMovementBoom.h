// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "CDGMovementBoom.generated.h"

/**
 * UCDGMovementBoom
 *
 * Movement generator that translates the camera vertically — the "boom" or
 * "crane" shot.
 *
 * The camera rises by BoomDistance centimetres (positive value) or descends
 * (negative value) over the reference sequence duration.  Its horizontal
 * world position is unchanged.
 *
 * When bLookAtSubjectThroughout is true, the look-at rotation toward the
 * anchor is updated at every keyframe, so the subject stays centred in
 * frame as the camera ascends or descends.  When false, the initial look-at
 * direction is locked and the camera simply translates straight up or down.
 *
 * Boom up combined with a wide focal length is classically used to open a
 * sequence; boom down to close one.
 *
 * References:
 *   - Katz, S.D. (1991). Film Directing Shot by Shot. Michael Wiese Productions.
 *     "Crane / Boom Shot", pp. 118–121.
 *   - Brown, B. (2012). Cinematography: Theory and Practice, 2nd ed. Focal Press.
 *     Chapter 6: "Boom Up / Boom Down".
 *
 * Pipeline order:  POSITIONING  →  MovementBoom  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementBoom : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/**
	 * Vertical distance (cm) to travel.
	 * Positive = boom up; negative = boom down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (UIMin = "-2000.0", UIMax = "2000.0"))
	float BoomDistance = 200.0f;

	/** Number of keyframes distributed across the reference sequence duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "2", ClampMax = "240"))
	int32 NumKeyframes = 12;

	/**
	 * When true, look-at toward the anchor is recomputed at every keyframe.
	 * When false, the initial look-at direction is kept constant throughout
	 * the vertical travel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	bool bLookAtSubjectThroughout = true;

	/** Speed profile for the vertical movement. */
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
