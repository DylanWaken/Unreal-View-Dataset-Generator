// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "CDGMovementArc.generated.h"

/**
 * UCDGMovementArc
 *
 * Movement generator that orbits the camera around the subject's anchor on a
 * horizontal circular arc — the "arc shot" or "orbit shot".
 *
 * The arc radius is preserved from the placement position (i.e. the initial
 * horizontal distance from the camera to the anchor's XY projection).  Camera
 * height is kept constant relative to the anchor throughout the orbit.
 * Look-at toward the anchor is recomputed at every keyframe.
 *
 * An arc of 360 degrees produces a full revolution.  Combining with a Boom
 * generator in the Effects stage can create a helical (crane-style) orbit.
 *
 * References:
 *   - Mercado, G. (2011). The Filmmaker's Eye. Focal Press.
 *     Chapter 5: "Arc Shot — Orbiting the Subject".
 *   - Kenworthy, C. (2012). Master Shots Vol 1, 2nd ed. Michael Wiese Productions.
 *     "Arc Shot", pp. 52–55.
 *
 * Pipeline order:  POSITIONING  →  MovementArc  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementArc : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/**
	 * Total arc sweep in degrees.
	 * 90 = quarter-circle; 180 = semi-circle; 360 = full revolution.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "1.0", ClampMax = "360.0"))
	float ArcAngle = 90.0f;

	/**
	 * When true the camera orbits clockwise when viewed from above (negative
	 * yaw direction).  When false it orbits counter-clockwise.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	bool bClockwise = false;

	/** Number of keyframes distributed across the reference sequence duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "2", ClampMax = "360"))
	int32 NumKeyframes = 24;

	/** Speed profile for the orbital movement. */
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
