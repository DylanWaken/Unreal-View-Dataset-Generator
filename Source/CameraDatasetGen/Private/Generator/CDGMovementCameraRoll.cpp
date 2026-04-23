// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementCameraRoll.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementCameraRoll::GetGeneratorName_Implementation() const
{
	return FName("MovementCameraRoll");
}

FText UCDGMovementCameraRoll::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementCameraRollTip",
		"Rotates the camera about its forward axis (barrel roll / Dutch angle sweep). "
		"Camera position is fixed; roll angle sweeps from StartRollAngle to EndRollAngle.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementCameraRoll::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementCameraRoll: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementCameraRoll: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementCameraRoll: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementCameraRoll"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF     = FMath::Max(2, NumKeyframes);
	const float   SegDur    = EffDur / (float)(NumKF - 1);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector CamPos   = Placement.Position;
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);

		// Compute base look-at once; only roll component varies
		const FRotator BaseLookAt = ComputeLookAtRotation(CamPos, AnchorPos);

		for (int32 i = 0; i < NumKF; ++i)
		{
			const float t       = (NumKF > 1) ? (float)i / (float)(NumKF - 1) : 0.0f;
			const float Roll    = FMath::Lerp(StartRollAngle, EndRollAngle, t);
			const FRotator Rot(BaseLookAt.Pitch, BaseLookAt.Yaw, Roll);
			const float TimeToKF = (i == 0) ? 0.0f : SegDur;

			if (ACDGKeyframe* KF = SpawnKeyframe(World, Subsystem,
				CamPos, Rot, TrajName, i,
				TimeToKF, 0.0f, AnchorPos, DefaultAperture))
			{
				KF->SpeedInterpolationMode = SpeedInterpolation;
			}
		}

		Subsystem->RebuildTrajectorySpline(TrajName);

		if (ACDGTrajectory* Traj = Subsystem->GetTrajectory(TrajName))
		{
			CreatedTrajectories.Add(Traj);
		}
	}

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementCameraRoll: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementCameraRoll::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("StartRollAngle"),  (double)StartRollAngle);
	OutJson->SetNumberField(TEXT("EndRollAngle"),    (double)EndRollAngle);
	OutJson->SetNumberField(TEXT("NumKeyframes"),    (double)NumKeyframes);
	OutJson->SetNumberField(TEXT("SpeedInterpolation"),(double)(uint8)SpeedInterpolation);
	OutJson->SetNumberField(TEXT("DefaultAperture"), (double)DefaultAperture);
}

void UCDGMovementCameraRoll::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("StartRollAngle")))
		StartRollAngle = (float)InJson->GetNumberField(TEXT("StartRollAngle"));
	if (InJson->HasField(TEXT("EndRollAngle")))
		EndRollAngle = (float)InJson->GetNumberField(TEXT("EndRollAngle"));
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("SpeedInterpolation")))
		SpeedInterpolation = (ECDGSpeedInterpolationMode)(uint8)InJson->GetNumberField(TEXT("SpeedInterpolation"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
