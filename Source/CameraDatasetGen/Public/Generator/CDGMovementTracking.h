// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "CDGMovementTracking.generated.h"

/**
 * UCDGMovementTracking
 *
 * Movement generator that follows the subject by sampling the anchor's world
 * position at every display-rate frame of the reference sequence and
 * maintaining a constant world-space offset from the anchor.
 *
 * The offset is derived from the placement position at frame zero:
 *   Offset = PlacementPosition − AnchorPosition[0]
 *
 * At each subsequent frame:
 *   CameraPosition[i] = AnchorPosition[i] + Offset
 *   Rotation[i] = look-at toward AnchorPosition[i]
 *
 * This produces a camera that travels with the character while always looking
 * at the defined anchor point — equivalent to a real-world tracking dolly or
 * Steadicam rig.
 *
 * References:
 *   - Katz, S.D. (1991). Film Directing Shot by Shot. Michael Wiese Productions.
 *     "Tracking Shot / Following Shot", pp. 110–115.
 *   - Brown, B. (2012). Cinematography: Theory and Practice, 2nd ed. Focal Press.
 *     Chapter 6: "Moving with the Subject".
 *
 * Pipeline order:  POSITIONING  →  MovementTracking  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementTracking : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/**
	 * Additional world-space offset added on top of the placement-derived
	 * offset.  Useful for fine-tuning the default follow distance without
	 * moving the positioning generator's output.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	FVector AdditionalOffset = FVector::ZeroVector;

	/**
	 * When true, vertical (Z) tracking is included so the camera rises and
	 * falls with the anchor.  When false, Z is locked to the placement height,
	 * producing a horizontally-sliding shot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	bool bTrackVertical = true;

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
