// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGPositioningGenerator.h"
#include "CDGPositioningSpherical.generated.h"

/**
 * UCDGPositioningSpherical
 *
 * Positioning generator that samples camera viewpoints uniformly from a
 * spherical shell centered on the primary character's focused anchor at the
 * first frame of the reference sequence.
 *
 * Each accepted position passes two complementary line-of-sight checks
 * (ECC_Camera trace channel + WorldStatic/WorldDynamic object sweep) so that
 * cameras behind walls or below the floor are discarded and resampled.
 *
 * Output positions are named and forwarded to the Movement stage as
 * FCDGCameraPlacement records.  No world actors are spawned here.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGPositioningSpherical : public UCDGPositioningGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/** Number of viewpoints (placements) to generate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Positioning",
		meta = (ClampMin = "1", ClampMax = "500"))
	int32 Count = 5;

	/** Minimum sphere radius for viewpoint sampling (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Positioning",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RadiusMin = 200.0f;

	/** Maximum sphere radius for viewpoint sampling (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Positioning",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RadiusMax = 800.0f;

	// ==================== ADVANCED ====================

	/**
	 * Maximum sampling attempts allowed per placement slot before giving up.
	 * Increase when heavy occlusion causes fewer placements than requested.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Positioning|Advanced",
		meta = (ClampMin = "1"))
	int32 MaxSamplingAttemptsPerShot = 200;

	/**
	 * RNG seed used for sphere sampling.
	 * Set to -1 to use a random (time-based) seed each generation pass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Positioning|Advanced")
	int32 RandomSeed = -1;

	/**
	 * Sphere radius (cm) used for line-of-sight sweep probes.
	 * Increase for large virtual-camera rigs; decrease only if valid placements
	 * near thin walls are incorrectly rejected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Positioning|Advanced",
		meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CameraCollisionRadius = 15.0f;

	// ==================== OVERRIDES ====================

	virtual FName GetGeneratorName_Implementation() const override;
	virtual FText GetTip_Implementation() const override;
	virtual TArray<FCDGCameraPlacement> GeneratePlacements_Implementation() override;
	virtual void SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const override;
	virtual void FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson) override;

private:

	// ==================== HELPERS ====================

	/** Returns the world location of the focused anchor on PrimaryCharacterActor. */
	FVector GetCurrentAnchorWorldLocation() const;

	/**
	 * Draws one sample uniformly distributed in the solid sphere shell
	 * [MinR, MaxR] centered at Center.
	 */
	FVector SampleUniformSphericalShell(const FVector& Center, float MinR, float MaxR,
		FRandomStream& RNG) const;

	/**
	 * Returns true when the line segment From → To is not blocked by world
	 * geometry other than PrimaryCharacterActor.
	 * Runs two complementary sweeps: ECC_Camera channel + WorldStatic/WorldDynamic.
	 */
	bool HasClearLineOfSight(UWorld* World, const FVector& From, const FVector& To) const;
};
