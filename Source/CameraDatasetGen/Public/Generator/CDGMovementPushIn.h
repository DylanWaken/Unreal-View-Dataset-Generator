// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "CDGMovementPushIn.generated.h"

/**
 * UCDGMovementPushIn
 *
 * Movement generator that physically dollies the camera toward the subject's
 * focused anchor — the classical "push in" (dolly in) shot.
 *
 * The camera translates along the direct line from its placement position to
 * the anchor, covering PushDistance centimetres over the reference sequence
 * duration.  The look-at direction toward the anchor is recomputed at each
 * keyframe so the subject remains framed throughout.
 *
 * Used to build tension, heighten intimacy, or draw attention to an important
 * story beat.
 *
 * References:
 *   - Katz, S.D. (1991). Film Directing Shot by Shot. Michael Wiese Productions.
 *   - Mercado, G. (2011). The Filmmaker's Eye. Focal Press.
 *     Chapter 5: "Dolly In — Psychological Intensity".
 *   - Mascelli, J.V. (1965). The Five C's of Cinematography. Silman-James Press.
 *
 * Pipeline order:  POSITIONING  →  MovementPushIn  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementPushIn : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/**
	 * Distance (cm) the camera travels toward the anchor.
	 * The camera cannot overshoot the anchor: the move is clamped so the camera
	 * stops at least 10 cm from the anchor position.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "1.0", UIMin = "10.0"))
	float PushDistance = 200.0f;

	/** Number of keyframes distributed across the reference sequence duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "2", ClampMax = "240"))
	int32 NumKeyframes = 12;

	/** Speed profile for the dolly movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	ECDGSpeedInterpolationMode SpeedInterpolation = ECDGSpeedInterpolationMode::SlowIn;

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
