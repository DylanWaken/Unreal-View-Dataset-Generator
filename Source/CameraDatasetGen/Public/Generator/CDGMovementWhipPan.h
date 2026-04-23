// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "CDGMovementWhipPan.generated.h"

/**
 * UCDGMovementWhipPan
 *
 * Extremely fast horizontal pan that sweeps across a large arc in a very short
 * duration.  The motion-blur "smear" is a deliberate stylistic device used to
 * cut between scenes or punctuate action beats.
 *
 * Camera translation is fixed at the placement position.  Three keyframes are
 * spawned — start, midpoint, end — with the full arc distributed across
 * CustomDuration seconds (typically 0.1 – 0.5 s), leaving the remaining
 * reference-sequence time at the hold position on the last keyframe.
 *
 * References:
 *   - Ward, P. (2003). Picture Composition for Film and Television, 2nd ed.
 *     Focal Press. "Dynamic Camera Transitions", pp. 112–114.
 *   - Mercado, G. (2011). The Filmmaker's Eye. Focal Press.
 *     Chapter 7: "The Whip Pan as Punctuation".
 *
 * Pipeline order:  POSITIONING  →  MovementWhipPan  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementWhipPan : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/**
	 * Total angular sweep of the whip in degrees.
	 * 90–180 degrees is typical; larger values produce a more disorienting effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "10.0", ClampMax = "360.0", UIMin = "45.0", UIMax = "270.0"))
	float WhipAngle = 180.0f;

	/**
	 * When true the pan sweeps to the left (negative yaw); otherwise to the right.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	bool bPanLeft = false;

	/**
	 * Duration of the whip motion in seconds.  The remaining reference-sequence
	 * time is added as a dwell at the final orientation.
	 * Must be shorter than the reference sequence duration.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "0.05", ClampMax = "2.0", UIMin = "0.1", UIMax = "1.0"))
	float WhipDuration = 0.3f;

	/** Vertical pitch applied to both the start and end orientations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float PitchOffset = 0.0f;

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
