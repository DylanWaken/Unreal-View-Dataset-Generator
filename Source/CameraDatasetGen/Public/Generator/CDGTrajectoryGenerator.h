// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Anchor/CDGCharacterAnchor.h"
#include "CDGTrajectoryGenerator.generated.h"

class ULevelSequence;
class ACDGTrajectory;
class FJsonObject;

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
 *   2. Set ReferenceSequence, PrimaryCharacterActor, and FocusedAnchor.
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

	/** Primary character actor that exists in the same level referenced by the sequence */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
	TObjectPtr<AActor> PrimaryCharacterActor;

	/** Anchor point on the primary character that the generated camera should orient toward */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
	AnchorType FocusedAnchor = AnchorType::CDG_ANCHOR_HEAD;

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
