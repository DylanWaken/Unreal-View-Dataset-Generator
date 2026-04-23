// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "CDGMovementDollyZoom.generated.h"

/**
 * UCDGMovementDollyZoom
 *
 * Movement generator that produces the "Vertigo effect" (dolly zoom / trombone
 * shot): the camera physically moves while focal length is simultaneously
 * adjusted to keep the subject's apparent screen size constant.
 *
 * Physical basis:
 *   Image size ∝ focal_length / distance
 *   To hold image size constant:  fl₁/d₁ = fl₂/d₂  →  fl₂ = fl₁ × d₂/d₁
 *
 * When bDollyIn is true the camera moves toward the subject (d decreases) so
 * focal length must decrease (zoom out) — background shrinks.
 * When bDollyIn is false the camera moves away (d increases) so focal length
 * must increase (zoom in) — background expands.
 *
 * References:
 *   - Hitchcock, A. (1958). Vertigo. Universal Pictures. [technique origin]
 *   - Brown, B. (2012). Cinematography: Theory and Practice, 2nd ed. Focal Press.
 *     Chapter 6: "The Dolly Zoom / Vertigo Effect", pp. 153–157.
 *   - Kenworthy, C. (2012). Master Shots Vol 1, 2nd ed. Michael Wiese Productions.
 *     "Trombone Shot", pp. 96–99.
 *
 * Pipeline order:  POSITIONING  →  MovementDollyZoom  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementDollyZoom : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/** Focal length (mm) at the placement start position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "4.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "200.0"))
	float StartFocalLength = 35.0f;

	/**
	 * Distance (cm) the camera travels.  The end focal length is computed
	 * automatically to maintain constant subject image size.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "1.0", UIMin = "50.0"))
	float MoveDistance = 300.0f;

	/**
	 * When true the camera moves toward the subject (dolly in → zoom out).
	 * When false the camera moves away (dolly out → zoom in).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	bool bDollyIn = true;

	/** Number of keyframes distributed across the reference sequence duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "2", ClampMax = "240"))
	int32 NumKeyframes = 24;

	/** Speed profile for the combined dolly + zoom movement. */
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
