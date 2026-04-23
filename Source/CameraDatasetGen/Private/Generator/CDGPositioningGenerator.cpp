// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGPositioningGenerator.h"

TArray<FCDGCameraPlacement> UCDGPositioningGenerator::GeneratePlacements_Implementation()
{
	// Base implementation is intentionally a no-op.
	// Concrete subclasses override this to produce camera placements.
	return TArray<FCDGCameraPlacement>();
}
