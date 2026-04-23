// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGPositioningGenerator.h"
#include "CDGEffectsGenerator.generated.h"

class ACDGTrajectory;

/**
 * UCDGEffectsGenerator
 *
 * Abstract base for the EFFECTS stage of the camera generation pipeline.
 *
 * Subclasses implement ApplyEffects() to post-process the trajectory actors
 * already created by the Movement stage. Effects generators mutate keyframe
 * lens / transform properties in-place and do not create or destroy trajectories.
 *
 * PrimaryCharacterActor and FocusedAnchor are inherited from
 * UCDGPositioningGenerator and are available for focus-distance or
 * subject-relative effect computation.
 *
 * Intended for depth-of-field tuning, lens simulation, camera-rig physics,
 * handheld shake overlays, and similar per-frame overlay effects.
 *
 * Pipeline order:  POSITIONING  →  MOVEMENT  →  EFFECTS
 */
UCLASS(Abstract, Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen", HideCategories = ("Generator|Subject"))
class CAMERADATASETGEN_API UCDGEffectsGenerator : public UCDGPositioningGenerator
{
	GENERATED_BODY()

public:

	// ==================== STAGE IDENTITY ====================

	virtual EGeneratorStage GetGeneratorStage_Implementation() const override
	{
		return EGeneratorStage::Effects;
	}

	// ==================== EFFECTS ====================

	/**
	 * Apply post-processing effects to the completed trajectories.
	 *
	 * Implementations receive the live ACDGTrajectory actors from the world and
	 * may read / modify their ACDGKeyframe children (lens settings, transform
	 * offsets, etc.).  The trajectory list itself must not be altered.
	 *
	 * The base implementation is a no-op passthrough.
	 *
	 * @param InTrajectories  Trajectories produced by the Movement stage.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Generator")
	void ApplyEffects(const TArray<ACDGTrajectory*>& InTrajectories);
	virtual void ApplyEffects_Implementation(const TArray<ACDGTrajectory*>& InTrajectories);
};
