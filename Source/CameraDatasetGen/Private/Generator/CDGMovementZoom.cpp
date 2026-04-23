// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementZoom.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementZoom::GetGeneratorName_Implementation() const
{
	return FName("MovementZoom");
}

FText UCDGMovementZoom::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementZoomTip",
		"Changes focal length while the camera holds a fixed position. "
		"Unlike a dolly, only field-of-view changes; perspective compression is not affected.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementZoom::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementZoom: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementZoom: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementZoom: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementZoom"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF     = FMath::Max(2, NumKeyframes);
	const float   SegDur    = EffDur / (float)(NumKF - 1);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector CamPos   = Placement.Position;
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);
		const FRotator FixedRot = ComputeLookAtRotation(CamPos, AnchorPos);

		for (int32 i = 0; i < NumKF; ++i)
		{
			const float t   = (NumKF > 1) ? (float)i / (float)(NumKF - 1) : 0.0f;
			const float FL  = FMath::Lerp(StartFocalLength, EndFocalLength, t);
			const float TimeToKF = (i == 0) ? 0.0f : SegDur;

			if (ACDGKeyframe* KF = SpawnKeyframe(World, Subsystem,
				CamPos, FixedRot, TrajName, i,
				TimeToKF, 0.0f, AnchorPos, DefaultAperture))
			{
				KF->LensSettings.FocalLength = FL;
				KF->SpeedInterpolationMode   = SpeedInterpolation;
			}
		}

		Subsystem->RebuildTrajectorySpline(TrajName);

		if (ACDGTrajectory* Traj = Subsystem->GetTrajectory(TrajName))
		{
			CreatedTrajectories.Add(Traj);
		}
	}

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementZoom: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementZoom::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("StartFocalLength"),  (double)StartFocalLength);
	OutJson->SetNumberField(TEXT("EndFocalLength"),    (double)EndFocalLength);
	OutJson->SetNumberField(TEXT("NumKeyframes"),      (double)NumKeyframes);
	OutJson->SetNumberField(TEXT("SpeedInterpolation"),(double)(uint8)SpeedInterpolation);
	OutJson->SetNumberField(TEXT("DefaultAperture"),   (double)DefaultAperture);
}

void UCDGMovementZoom::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("StartFocalLength")))
		StartFocalLength = FMath::Clamp((float)InJson->GetNumberField(TEXT("StartFocalLength")), 4.0f, 1000.0f);
	if (InJson->HasField(TEXT("EndFocalLength")))
		EndFocalLength = FMath::Clamp((float)InJson->GetNumberField(TEXT("EndFocalLength")), 4.0f, 1000.0f);
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("SpeedInterpolation")))
		SpeedInterpolation = (ECDGSpeedInterpolationMode)(uint8)InJson->GetNumberField(TEXT("SpeedInterpolation"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
