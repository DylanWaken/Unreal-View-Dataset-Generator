// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "CDGMovementRandom.generated.h"

/**
 * UCDGMovementRandom
 *
 * Movement generator that produces a pseudo-random handheld-style camera path
 * by perturbing the placement position and the look-at rotation at each of
 * NumKeyframes evenly-spaced keyframes using a seeded random stream.
 *
 * Positional noise is applied as a uniform random offset within a sphere of
 * radius PositionNoiseMagnitude centimetres centred on the placement position.
 * Rotational noise is applied as a yaw/pitch perturbation of up to
 * RotationNoiseMagnitude degrees around the base look-at direction.
 * FocalLengthVariation optionally randomises the focal length around
 * BaseFocalLength at each keyframe.
 *
 * Setting RandomSeed to -1 (default) uses a time-based seed so each
 * generation pass produces a unique result.  A fixed non-negative seed
 * guarantees deterministic output.
 *
 * References:
 *   - Kenworthy, C. (2012). Master Shots Vol 1, 2nd ed. Michael Wiese Productions.
 *     "Handheld / Vérité Style", pp. 142–145.
 *   - Perlin, K. (1985). "An Image Synthesizer". ACM SIGGRAPH Computer Graphics
 *     19(3): 287–296. [procedural noise foundation]
 *
 * Pipeline order:  POSITIONING  →  MovementRandom  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementRandom : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/** Maximum radius (cm) of the position perturbation sphere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
	float PositionNoiseMagnitude = 50.0f;

	/** Maximum yaw and pitch perturbation (degrees) from the base look-at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "45.0"))
	float RotationNoiseMagnitude = 5.0f;

	/**
	 * Base focal length (mm) around which random variation is applied.
	 * If FocalLengthVariation is 0 the focal length is kept constant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "4.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "200.0"))
	float BaseFocalLength = 35.0f;

	/** Maximum focal length variation (±mm) added to BaseFocalLength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "50.0"))
	float FocalLengthVariation = 0.0f;

	/** Number of keyframes distributed across the reference sequence duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "2", ClampMax = "240"))
	int32 NumKeyframes = 24;

	/**
	 * RNG seed.  -1 = time-based (non-deterministic); any other value produces
	 * deterministic output for the same scene and parameters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	int32 RandomSeed = -1;

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
