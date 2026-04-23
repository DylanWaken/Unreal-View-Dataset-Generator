// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "CDGMovementZoom.generated.h"

/**
 * UCDGMovementZoom
 *
 * Movement generator that changes focal length (optical zoom) while the camera
 * holds a fixed world position.
 *
 * Unlike a dolly move, a zoom only changes the field of view — perspective
 * compression / expansion is NOT produced.  The subject's apparent size
 * changes while the spatial relationship between foreground and background
 * remains constant.
 *
 * StartFocalLength and EndFocalLength are interpolated across NumKeyframes
 * keyframes spanning the reference sequence duration.
 *
 * References:
 *   - Brown, B. (2012). Cinematography: Theory and Practice, 2nd ed. Focal Press.
 *     Chapter 4: "Lenses — Focal Length and Its Effects".
 *   - Ascher, S. & Pincus, E. (2012). The Filmmaker's Handbook, 4th ed.
 *     Penguin. "Zoom vs. Dolly", pp. 51–53.
 *
 * Pipeline order:  POSITIONING  →  MovementZoom  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementZoom : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/** Focal length (mm) at the start of the zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "4.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "200.0"))
	float StartFocalLength = 35.0f;

	/** Focal length (mm) at the end of the zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "4.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "200.0"))
	float EndFocalLength = 85.0f;

	/** Number of keyframes distributed across the reference sequence duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "2", ClampMax = "240"))
	int32 NumKeyframes = 2;

	/** Speed profile for the focal length ramp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement")
	ECDGSpeedInterpolationMode SpeedInterpolation = ECDGSpeedInterpolationMode::Linear;

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
