// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGMovementGenerator.h"
#include "CDGMovementCrashZoom.generated.h"

/**
 * UCDGMovementCrashZoom
 *
 * Movement generator that produces an extremely fast optical zoom over a very
 * short duration — the "crash zoom" (also called snap zoom or speed zoom).
 *
 * The camera holds a fixed world position.  The zoom completes within
 * ZoomDuration seconds; the remaining reference-sequence time is spent at the
 * final focal length.  Because the transition is near-instantaneous the motion
 * conveys urgency, shock, or comedy.
 *
 * References:
 *   - Mercado, G. (2011). The Filmmaker's Eye. Focal Press.
 *     Chapter 7: "Crash Zoom — Punctuation and Emphasis".
 *   - Kenworthy, C. (2012). Master Shots Vol 1, 2nd ed. Michael Wiese Productions.
 *     "Snap Zoom / Speed Zoom", pp. 88–91.
 *
 * Pipeline order:  POSITIONING  →  MovementCrashZoom  →  EFFECTS
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGMovementCrashZoom : public UCDGMovementGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/** Focal length (mm) before the crash zoom begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "4.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "200.0"))
	float StartFocalLength = 35.0f;

	/** Focal length (mm) at the end of the crash zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "4.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "200.0"))
	float EndFocalLength = 135.0f;

	/**
	 * Duration of the zoom ramp in seconds.  Remaining reference-sequence time
	 * is spent holding at EndFocalLength.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Movement",
		meta = (ClampMin = "0.05", ClampMax = "2.0", UIMin = "0.1", UIMax = "1.0"))
	float ZoomDuration = 0.4f;

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
