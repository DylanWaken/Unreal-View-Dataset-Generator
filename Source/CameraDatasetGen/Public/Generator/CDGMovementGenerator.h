// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGPositioningGenerator.h"
#include "CDGMovementGenerator.generated.h"

class ACDGTrajectory;
class ACDGKeyframe;
class UCDGTrajectorySubsystem;

/**
 * UCDGMovementGenerator
 *
 * Abstract base for the MOVEMENT stage of the camera generation pipeline.
 *
 * Subclasses receive the array of FCDGCameraPlacement records produced by the
 * Positioning stage and implement GenerateMovement() to turn them into fully-
 * keyed ACDGTrajectory actors registered with UCDGTrajectorySubsystem.
 *
 * Movement generators have full access to ReferenceSequence, PrimaryCharacterActor,
 * and FocusedAnchor (inherited from UCDGPositioningGenerator) so they can sample
 * the anchor trajectory and control how the camera evolves over time — from a
 * fully static two-keyframe shot all the way to complex motion paths.
 *
 * Pipeline order:  POSITIONING  →  MOVEMENT  →  EFFECTS
 */
UCLASS(Abstract, Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen", HideCategories = ("Generator|Subject"))
class CAMERADATASETGEN_API UCDGMovementGenerator : public UCDGPositioningGenerator
{
	GENERATED_BODY()

public:

	// ==================== STAGE IDENTITY ====================

	virtual EGeneratorStage GetGeneratorStage_Implementation() const override
	{
		return EGeneratorStage::Movement;
	}

	// ==================== MOVEMENT ====================

	/**
	 * Generate fully-keyed trajectories from the supplied placements.
	 *
	 * Implementations should:
	 *   - Iterate over Placements and spawn ACDGKeyframe actors using each
	 *     placement's Position and TrajectoryName.
	 *   - Register trajectories with UCDGTrajectorySubsystem via the world context.
	 *   - Use ReferenceSequence for timing and PrimaryCharacterActor / FocusedAnchor
	 *     for look-at and focus computation.
	 *
	 * @param Placements  Initial camera positions produced by the Positioning stage.
	 * @return Array of ACDGTrajectory actors created during this pass.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Generator")
	TArray<ACDGTrajectory*> GenerateMovement(const TArray<FCDGCameraPlacement>& Placements);
	virtual TArray<ACDGTrajectory*> GenerateMovement_Implementation(const TArray<FCDGCameraPlacement>& Placements);

protected:

	// ==================== SHARED HELPERS ====================

	/**
	 * Returns the world location of FocusedAnchor on PrimaryCharacterActor.
	 * Falls back to the actor's root location if no matching anchor component is found.
	 */
	FVector GetCurrentAnchorWorldLocation() const;

	/**
	 * Builds a world-space look-at rotation from CameraPos toward TargetPos and
	 * optionally applies a camera-space yaw/pitch deviation.
	 *
	 * @param DeviationDeg  X = horizontal yaw offset (degrees), Y = vertical pitch offset (degrees).
	 */
	FRotator ComputeLookAtRotation(const FVector& CameraPos, const FVector& TargetPos,
		const FVector2D& DeviationDeg = FVector2D::ZeroVector) const;

	/**
	 * Samples the primary character's anchor world position at every display-rate
	 * frame of the reference sequence.  Returns a single fallback element when no
	 * 3D transform track is found for PrimaryCharacterActor.
	 *
	 * @param FallbackPosition  Returned as a single-element array when sequence data is absent.
	 */
	TArray<FVector> SampleAnchorPositionsFromSequence(const FVector& FallbackPosition) const;

	/**
	 * Spawns one ACDGKeyframe actor, configures all timing, lens, and autofocus
	 * fields, and registers the name change with the subsystem when needed.
	 *
	 * @param Aperture  f-stop to write to LensSettings (caller controls per-generator default).
	 */
	ACDGKeyframe* SpawnKeyframe(UWorld* World, UCDGTrajectorySubsystem* Subsystem,
		const FVector& Position, const FRotator& Rotation,
		FName TrajectoryName, int32 Order,
		float TimeToCurrentFrame, float TimeAtCurrentFrame,
		const FVector& AnchorWorldPos, float Aperture) const;

	/**
	 * Validates that a non-null ReferenceSequence with a positive playback duration
	 * is set on this generator and returns that duration in seconds.
	 *
	 * Returns 0 and logs an error if the sequence is missing or has zero / invalid
	 * duration.  Callers should early-out when the return value is <= 0.
	 *
	 * @param CallerName  Class/function name used in the error log message.
	 */
	float GetValidatedSequenceDuration(const TCHAR* CallerName) const;

	/**
	 * Produces a trajectory name that is unique to this (placement, movement)
	 * pair by combining the placement's base name with the movement generator's
	 * identity and asking the subsystem for a collision-free variant.
	 *
	 * This is the ONLY way movement subclasses should derive trajectory names.
	 * Using Placement.TrajectoryName directly causes every movement generator
	 * in the stack to pile its keyframes onto the same trajectory, producing
	 * shots that chain multiple movements and run N× longer than the animation.
	 *
	 * @param Subsystem The world's trajectory subsystem (must not be null).
	 * @param Placement The placement produced by the positioning stage.
	 * @return A unique FName suitable for SpawnKeyframe's TrajectoryName argument.
	 */
	FName ComposeTrajectoryName(UCDGTrajectorySubsystem* Subsystem,
		const FCDGCameraPlacement& Placement) const;
};
