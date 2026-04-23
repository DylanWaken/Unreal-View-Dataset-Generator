// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGEffectsNone.h"
#include "Dom/JsonObject.h"

FName UCDGEffectsNone::GetGeneratorName_Implementation() const
{
	return FName("EffectsNone");
}

FText UCDGEffectsNone::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "EffectsNoneTip",
		"Passthrough — no modifications are applied to the completed trajectories. "
		"Use as a template for custom lens or camera-rig effects generators.");
}

void UCDGEffectsNone::ApplyEffects_Implementation(const TArray<ACDGTrajectory*>& /*InTrajectories*/)
{
	// Intentional no-op passthrough.
}

void UCDGEffectsNone::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();
	// No configuration parameters to serialize.
}

void UCDGEffectsNone::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& /*InJson*/)
{
	// No configuration parameters to restore.
}
