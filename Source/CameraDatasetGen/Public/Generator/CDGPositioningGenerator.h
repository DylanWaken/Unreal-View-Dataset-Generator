// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGTrajectoryGenerator.h"
#include "Anchor/CDGCharacterAnchor.h"
#include "CDGPositioningGenerator.generated.h"

/**
 * UCDGPositioningGenerator
 *
 * Abstract base for the POSITIONING stage of the camera generation pipeline.
 *
 * Subclasses implement GeneratePlacements() to compute candidate camera world
 * positions and assign each a unique trajectory name. No world actors are
 * spawned at this stage — only lightweight FCDGCameraPlacement records are
 * returned, which are then forwarded to a UCDGMovementGenerator.
 *
 * PrimaryCharacterActor and FocusedAnchor are declared here and inherited by
 * the downstream UCDGMovementGenerator and UCDGEffectsGenerator stages, making
 * them the single point of ownership for subject / focus configuration across
 * the entire pipeline.
 *
 * Pipeline order:  POSITIONING  →  MOVEMENT  →  EFFECTS
 */
UCLASS(Abstract, Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGPositioningGenerator : public UCDGTrajectoryGenerator
{
	GENERATED_BODY()

public:

	// ==================== CONFIGURATION ====================

	/** Primary character actor that exists in the same level referenced by the sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Subject")
	TObjectPtr<AActor> PrimaryCharacterActor;

	/** Anchor point on the primary character that the generated camera should orient toward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator|Subject")
	AnchorType FocusedAnchor = AnchorType::CDG_ANCHOR_HEAD;

	// ==================== STAGE IDENTITY ====================

	virtual EGeneratorStage GetGeneratorStage_Implementation() const override
	{
		return EGeneratorStage::Positioning;
	}

	// ==================== POSITIONING ====================

	/**
	 * Compute candidate camera placements for the movement stage.
	 *
	 * Implementations should:
	 *   - Use PrimaryCharacterActor / FocusedAnchor to locate the subject.
	 *   - Sample world positions and assign unique trajectory names
	 *     (via UCDGTrajectorySubsystem::GenerateUniqueTrajectoryName or custom scheme).
	 *   - NOT spawn any world actors.
	 *
	 * @return Array of FCDGCameraPlacement records; one per desired camera viewpoint.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Generator")
	TArray<FCDGCameraPlacement> GeneratePlacements();
	virtual TArray<FCDGCameraPlacement> GeneratePlacements_Implementation();
};
