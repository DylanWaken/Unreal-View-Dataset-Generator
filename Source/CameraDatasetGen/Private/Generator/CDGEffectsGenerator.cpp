// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGEffectsGenerator.h"

void UCDGEffectsGenerator::ApplyEffects_Implementation(const TArray<ACDGTrajectory*>& /*InTrajectories*/)
{
	// Base implementation is intentionally a no-op passthrough.
	// Concrete subclasses override this to mutate keyframe properties.
}
