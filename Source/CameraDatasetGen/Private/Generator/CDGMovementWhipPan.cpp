// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementWhipPan.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementWhipPan::GetGeneratorName_Implementation() const
{
	return FName("MovementWhipPan");
}

FText UCDGMovementWhipPan::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementWhipPanTip",
		"Extremely fast horizontal pan over WhipAngle degrees in WhipDuration seconds. "
		"Produces a motion-blur smear used as a dynamic scene transition.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementWhipPan::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning,
			TEXT("UCDGMovementWhipPan: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementWhipPan: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementWhipPan: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos   = GetCurrentAnchorWorldLocation();
	const float   EffTotalDur = GetValidatedSequenceDuration(TEXT("UCDGMovementWhipPan"));
	if (EffTotalDur <= 0.0f) return CreatedTrajectories;

	// Clamp whip duration to be no longer than the full sequence
	const float   EffWhipDur  = FMath::Min(WhipDuration, EffTotalDur * 0.9f);
	const float   DwellDur    = FMath::Max(0.0f, EffTotalDur - EffWhipDur);
	const float   DirectionSign = bPanLeft ? -1.0f : 1.0f;

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector CamPos   = Placement.Position;
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);

		// KF 0 — start orientation (looking at anchor, no offset)
		const FVector2D StartDev(0.0f, PitchOffset);
		const FRotator  StartRot = ComputeLookAtRotation(CamPos, AnchorPos, StartDev);
		SpawnKeyframe(World, Subsystem,
			CamPos, StartRot, TrajName, 0,
			0.0f, 0.0f, AnchorPos, DefaultAperture);

		// KF 1 — end orientation after the whip sweep; Constant interpolation for instant blur
		const FVector2D EndDev(DirectionSign * WhipAngle, PitchOffset);
		const FRotator  EndRot = ComputeLookAtRotation(CamPos, AnchorPos, EndDev);
		if (ACDGKeyframe* KF = SpawnKeyframe(World, Subsystem,
			CamPos, EndRot, TrajName, 1,
			EffWhipDur, DwellDur, AnchorPos, DefaultAperture))
		{
			// Constant speed — no easing, maximises perceived blur
			KF->SpeedInterpolationMode = ECDGSpeedInterpolationMode::Constant;
		}

		Subsystem->RebuildTrajectorySpline(TrajName);

		if (ACDGTrajectory* Traj = Subsystem->GetTrajectory(TrajName))
		{
			CreatedTrajectories.Add(Traj);
		}
	}

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementWhipPan: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementWhipPan::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("WhipAngle"),      (double)WhipAngle);
	OutJson->SetBoolField  (TEXT("bPanLeft"),        bPanLeft);
	OutJson->SetNumberField(TEXT("WhipDuration"),   (double)WhipDuration);
	OutJson->SetNumberField(TEXT("PitchOffset"),    (double)PitchOffset);
	OutJson->SetNumberField(TEXT("DefaultAperture"),(double)DefaultAperture);
}

void UCDGMovementWhipPan::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("WhipAngle")))
		WhipAngle = FMath::Clamp((float)InJson->GetNumberField(TEXT("WhipAngle")), 10.0f, 360.0f);
	if (InJson->HasField(TEXT("bPanLeft")))
		bPanLeft = InJson->GetBoolField(TEXT("bPanLeft"));
	if (InJson->HasField(TEXT("WhipDuration")))
		WhipDuration = FMath::Clamp((float)InJson->GetNumberField(TEXT("WhipDuration")), 0.05f, 2.0f);
	if (InJson->HasField(TEXT("PitchOffset")))
		PitchOffset = (float)InJson->GetNumberField(TEXT("PitchOffset"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
