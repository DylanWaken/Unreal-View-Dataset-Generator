// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGTrajectoryGenerator.h"
#include "CGDGeneratorStatic.generated.h"

class UCDGTrajectorySubsystem;
class ACDGKeyframe;

/**
 * UCDGGeneratorStatic
 *
 * Generates static-position camera trajectories by uniformly sampling viewpoints
 * from a spherical shell centered on the primary character's focused anchor at the
 * first frame of the reference sequence.
 *
 * Each accepted sample becomes one ACDGTrajectory registered with the world's
 * UCDGTrajectorySubsystem. Samples blocked by world geometry (excluding the primary
 * character) are discarded and resampled.
 *
 * Two generation modes:
 * - Static (bFollowAnchor = false): 2 keyframes at sequence start/end with a fixed
 *   look-at rotation toward the anchor's first-frame world position.
 * - Follow (bFollowAnchor = true): one keyframe per display-rate frame; camera position
 *   remains fixed at the sample point while the rotation tracks the anchor's world
 *   position as it moves through the reference sequence.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGGeneratorStatic : public UCDGTrajectoryGenerator
{
	GENERATED_BODY()

public:
	// ==================== CONFIGURATION ====================

	/** Number of trajectories (viewpoints) to generate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Static",
		meta = (ClampMin = "1", ClampMax = "500"))
	int32 Count = 5;

	/** Minimum sphere radius for viewpoint sampling (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Static",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RadiusMin = 200.0f;

	/** Maximum sphere radius for viewpoint sampling (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Static",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RadiusMax = 800.0f;

	/**
	 * If true, the camera rotates to track the anchor's world position at every
	 * display-rate frame of the reference sequence (one keyframe per frame).
	 *
	 * If false, the camera holds a fixed look-at rotation toward the anchor's
	 * first-frame position; only two keyframes are generated (start and end).
	 *
	 * Camera translation is always fixed at the sampled viewpoint.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Static")
	bool bFollowAnchor = false;

	/**
	 * Additional angular offset (degrees) applied to the base look-at rotation
	 * in camera space.  X = horizontal (yaw), Y = vertical (pitch).
	 * Positive X rotates the camera to the right; positive Y rotates upward.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Static")
	FVector2D ViewDirectionDeviation = FVector2D::ZeroVector;

	// ==================== ADVANCED ====================

	/**
	 * Maximum number of sampling attempts allowed per trajectory before the
	 * generator gives up on that slot.  Increase this if the scene has heavy
	 * occlusion and generation is yielding fewer results than requested.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Static|Advanced",
		meta = (ClampMin = "1"))
	int32 MaxSamplingAttemptsPerShot = 200;

	/**
	 * RNG seed used for sphere sampling.
	 * Set to -1 to use a random (time-based) seed each generation pass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Static|Advanced")
	int32 RandomSeed = -1;

	// ==================== OVERRIDES ====================

	virtual FName GetGeneratorName_Implementation() const override;
	virtual FText GetTip_Implementation() const override;
	virtual TArray<ACDGTrajectory*> Generate_Implementation() override;
	virtual void SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const override;
	virtual void FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson) override;

private:
	// ==================== HELPERS ====================

	/** Returns the world location of the focused anchor component on PrimaryCharacterActor. */
	FVector GetCurrentAnchorWorldLocation() const;

	/**
	 * Draws one sample uniformly distributed in the solid sphere shell
	 * [MinR, MaxR] centered at Center.
	 */
	FVector SampleUniformSphericalShell(const FVector& Center, float MinR, float MaxR,
		FRandomStream& RNG) const;

	/**
	 * Returns true when the line segment From → To is not blocked by any world
	 * geometry other than PrimaryCharacterActor and its components.
	 */
	bool HasClearLineOfSight(UWorld* World, const FVector& From, const FVector& To) const;

	/**
	 * Builds a world-space rotation that aims from CameraPos toward TargetPos,
	 * then applies ViewDirectionDeviation as a camera-space yaw/pitch offset.
	 */
	FRotator ComputeLookAtRotation(const FVector& CameraPos, const FVector& TargetPos) const;

	/**
	 * Samples the primary character's anchor world position at each display-rate
	 * frame of the reference sequence, using the transform track bound to
	 * PrimaryCharacterActor if one exists.
	 *
	 * Falls back to FallbackPosition for any frame where channel data is absent.
	 * Returns an empty array if the reference sequence or movie scene is invalid.
	 */
	TArray<FVector> SampleAnchorPositionsFromSequence(const FVector& FallbackPosition) const;

	/**
	 * Spawns an ACDGKeyframe at (Position, Rotation), assigns it to TrajectoryName,
	 * sets timing/order, and updates the subsystem.
	 * Returns null if the spawn failed.
	 */
	ACDGKeyframe* SpawnKeyframe(UWorld* World, UCDGTrajectorySubsystem* Subsystem,
		const FVector& Position, const FRotator& Rotation,
		FName TrajectoryName, int32 Order,
		float TimeToCurrentFrame, float TimeAtCurrentFrame) const;
};
