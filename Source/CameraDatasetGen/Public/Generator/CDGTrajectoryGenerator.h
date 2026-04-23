// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CDGTrajectoryGenerator.generated.h"

class ULevelSequence;
class ACDGTrajectory;
class FJsonObject;

// ─────────────────────────────────────────────────────────────────────────────
// EGeneratorStage — identifies which pipeline stage a generator belongs to
// ─────────────────────────────────────────────────────────────────────────────

/**
 * The three stages of the camera generation pipeline:
 *
 *   POSITIONING  →  MOVEMENT  →  EFFECTS
 *
 * POSITIONING generators sample initial camera placements (world position,
 * trajectory name) without creating any world actors.
 *
 * MOVEMENT generators receive those placements together with anchor / sequence
 * context and produce fully-keyed ACDGTrajectory actors.
 *
 * EFFECTS generators post-process completed trajectories (e.g. depth of field,
 * lens simulation, camera rigs) without changing the keyframe count.
 */
UENUM(BlueprintType)
enum class EGeneratorStage : uint8
{
	Positioning UMETA(DisplayName = "Positioning"),
	Movement    UMETA(DisplayName = "Movement"),
	Effects     UMETA(DisplayName = "Effects"),
};

// ─────────────────────────────────────────────────────────────────────────────
// FCDGCameraPlacement — output record of the Positioning stage
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Lightweight descriptor produced by a UCDGPositioningGenerator.
 * Carries the world-space position and the unique trajectory name that the
 * downstream UCDGMovementGenerator should use when spawning keyframes.
 */
USTRUCT(BlueprintType)
struct CAMERADATASETGEN_API FCDGCameraPlacement
{
	GENERATED_BODY()

	/** Unique name for the trajectory this placement will become. */
	UPROPERTY(BlueprintReadWrite, Category = "Generator")
	FName TrajectoryName;

	/** Proposed initial world-space camera position. */
	UPROPERTY(BlueprintReadWrite, Category = "Generator")
	FVector Position = FVector::ZeroVector;
};

/**
 * UCDGTrajectoryGenerator
 *
 * Base interface class for procedural camera trajectory generation.
 *
 * Subclasses implement Generate() to produce ACDGKeyframe and ACDGTrajectory actors
 * in the world, spanning the playback duration of the supplied reference sequence.
 * Generated trajectories are registered with the world's UCDGTrajectorySubsystem.
 *
 * The reference sequence follows the same validity rules as the base shot sequence
 * in CDGLevelSeqExporter: it must have a valid tick resolution and a non-zero
 * playback range. Its duration is the authoritative timeline for all generated output.
 *
 * Lifecycle:
 *   1. Create an instance with a world-context outer (e.g. an actor or subsystem).
 *   2. Set ReferenceSequence (and PrimaryCharacterActor / FocusedAnchor on positioning-based generators).
 *   3. Call Generate(). The default implementation is a no-op.
 *   4. Optionally use SerializeGeneratorConfig / FetchGeneratorConfig for preset IO.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGTrajectoryGenerator : public UObject
{
	GENERATED_BODY()

public:
	// ==================== CONFIGURATION ====================

	/**
	 * Reference level sequence that defines the generation timeline duration.
	 *
	 * Must satisfy the same conditions as the base shot sequence in CDGLevelSeqExporter:
	 * - No nested UMovieSceneSubSection tracks
	 * - Valid (non-zero) tick resolution
	 * - Non-zero playback range duration
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
	TObjectPtr<ULevelSequence> ReferenceSequence;

	// ==================== GENERATION ====================

	/**
	 * Generate camera keyframes and trajectories for the reference sequence duration.
	 *
	 * Generated ACDGTrajectory and ACDGKeyframe actors are spawned into the world
	 * and registered with UCDGTrajectorySubsystem. The world is resolved from the
	 * UObject outer chain; this generator must be created with a world-context outer.
	 *
	 * The base implementation is a no-op and returns an empty array.
	 * Override in subclasses to produce concrete camera paths.
	 *
	 * @return Array of ACDGTrajectory actors created during this generation pass.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Generator")
	TArray<ACDGTrajectory*> Generate();
	virtual TArray<ACDGTrajectory*> Generate_Implementation();

	// ==================== SERIALIZATION ====================

	/**
	 * Serialize this generator's configuration parameters to a JSON object.
	 * Derived classes should populate OutJson with their specific parameters.
	 * Base implementation is a no-op.
	 *
	 * @param OutJson  JSON object to write generator parameters into.
	 */
	virtual void SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const;

	/**
	 * Populate this generator's configuration parameters from a JSON object.
	 * Derived classes should read their specific parameters from InJson.
	 * Base implementation is a no-op.
	 *
	 * @param InJson  JSON object containing previously serialized generator parameters.
	 */
	virtual void FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson);

	// ==================== IDENTITY ====================

	/**
	 * Returns which pipeline stage this generator belongs to.
	 * Base implementation returns EGeneratorStage::Positioning.
	 * Each abstract stage base class (UCDGPositioningGenerator etc.) overrides
	 * this to return its fixed stage; concrete subclasses inherit that override.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Generator")
	EGeneratorStage GetGeneratorStage() const;
	virtual EGeneratorStage GetGeneratorStage_Implementation() const;

	/**
	 * Returns a stable machine-readable name that uniquely identifies this generator type.
	 * Used as a key when registering, serializing, or displaying generators in the UI.
	 * Base implementation returns NAME_None; override in every concrete subclass.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Generator")
	FName GetGeneratorName() const;
	virtual FName GetGeneratorName_Implementation() const;

	/**
	 * Returns a short human-readable description of what this generator produces.
	 * Intended for tooltips and generator-picker UI.
	 * Base implementation returns an empty text; override in every concrete subclass.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Generator")
	FText GetTip() const;
	virtual FText GetTip_Implementation() const;

	// ==================== UTILITY ====================

	/**
	 * Returns the playback duration of the reference sequence in seconds.
	 * Returns 0.0 if ReferenceSequence is null, has no valid movie scene,
	 * or has an empty / unbounded playback range.
	 */
	UFUNCTION(BlueprintCallable, Category = "Generator")
	double GetReferenceDurationSeconds() const;
};
