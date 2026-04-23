// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Generator/CDGEffectsGenerator.h"
#include "CDGEffectsNone.generated.h"

/**
 * UCDGEffectsNone
 *
 * Passthrough effects generator that applies no modifications to the trajectories.
 * Acts as a placeholder / template for the Effects stage of the pipeline.
 *
 * Use this as the starting point for new lens or camera-rig effects generators.
 *
 * Pipeline role:  SphericalPositioning  →  MovementNone  →  EffectsNone
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "CameraDatasetGen")
class CAMERADATASETGEN_API UCDGEffectsNone : public UCDGEffectsGenerator
{
	GENERATED_BODY()

public:

	virtual FName GetGeneratorName_Implementation() const override;
	virtual FText GetTip_Implementation() const override;
	virtual void ApplyEffects_Implementation(const TArray<ACDGTrajectory*>& InTrajectories) override;
	virtual void SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const override;
	virtual void FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson) override;
};
