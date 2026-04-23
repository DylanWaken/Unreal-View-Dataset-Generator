// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementCrashZoom.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementCrashZoom::GetGeneratorName_Implementation() const
{
	return FName("MovementCrashZoom");
}

FText UCDGMovementCrashZoom::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementCrashZoomTip",
		"Extremely fast optical zoom completing within ZoomDuration seconds. "
		"Remaining sequence time is spent at the final focal length.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementCrashZoom::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementCrashZoom: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementCrashZoom: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementCrashZoom: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos   = GetCurrentAnchorWorldLocation();
	const float   EffTotalDur = GetValidatedSequenceDuration(TEXT("UCDGMovementCrashZoom"));
	if (EffTotalDur <= 0.0f) return CreatedTrajectories;
	const float   EffZoomDur  = FMath::Min(ZoomDuration, EffTotalDur * 0.95f);
	const float   DwellDur    = FMath::Max(0.0f, EffTotalDur - EffZoomDur);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector CamPos   = Placement.Position;
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);
		const FRotator FixedRot = ComputeLookAtRotation(CamPos, AnchorPos);

		// KF 0 — start focal length
		if (ACDGKeyframe* KF = SpawnKeyframe(World, Subsystem,
			CamPos, FixedRot, TrajName, 0,
			0.0f, 0.0f, AnchorPos, DefaultAperture))
		{
			KF->LensSettings.FocalLength = StartFocalLength;
		}

		// KF 1 — end focal length; Constant speed for abrupt snap
		if (ACDGKeyframe* KF = SpawnKeyframe(World, Subsystem,
			CamPos, FixedRot, TrajName, 1,
			EffZoomDur, DwellDur, AnchorPos, DefaultAperture))
		{
			KF->LensSettings.FocalLength  = EndFocalLength;
			KF->SpeedInterpolationMode    = ECDGSpeedInterpolationMode::Constant;
		}

		Subsystem->RebuildTrajectorySpline(TrajName);

		if (ACDGTrajectory* Traj = Subsystem->GetTrajectory(TrajName))
		{
			CreatedTrajectories.Add(Traj);
		}
	}

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementCrashZoom: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementCrashZoom::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("StartFocalLength"), (double)StartFocalLength);
	OutJson->SetNumberField(TEXT("EndFocalLength"),   (double)EndFocalLength);
	OutJson->SetNumberField(TEXT("ZoomDuration"),     (double)ZoomDuration);
	OutJson->SetNumberField(TEXT("DefaultAperture"),  (double)DefaultAperture);
}

void UCDGMovementCrashZoom::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("StartFocalLength")))
		StartFocalLength = FMath::Clamp((float)InJson->GetNumberField(TEXT("StartFocalLength")), 4.0f, 1000.0f);
	if (InJson->HasField(TEXT("EndFocalLength")))
		EndFocalLength = FMath::Clamp((float)InJson->GetNumberField(TEXT("EndFocalLength")), 4.0f, 1000.0f);
	if (InJson->HasField(TEXT("ZoomDuration")))
		ZoomDuration = FMath::Clamp((float)InJson->GetNumberField(TEXT("ZoomDuration")), 0.05f, 2.0f);
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
