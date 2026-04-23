// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementTilt.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementTilt::GetGeneratorName_Implementation() const
{
	return FName("MovementTilt");
}

FText UCDGMovementTilt::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementTiltTip",
		"Rotates the camera head vertically about a fixed position. "
		"TiltStartAngle and TiltEndAngle are pitch offsets from the direct look-at direction.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementTilt::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning,
			TEXT("UCDGMovementTilt: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementTilt: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementTilt: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementTilt"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF     = FMath::Max(2, NumKeyframes);
	const float   SegDur    = EffDur / (float)(NumKF - 1);

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementTilt: %d placement(s), %d keyframes, tilt %.1f→%.1f deg."),
		Placements.Num(), NumKF, TiltStartAngle, TiltEndAngle);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector CamPos   = Placement.Position;
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);

		for (int32 i = 0; i < NumKF; ++i)
		{
			const float t        = (NumKF > 1) ? (float)i / (float)(NumKF - 1) : 0.0f;
			const float PitchOff = FMath::Lerp(TiltStartAngle, TiltEndAngle, t);
			const FVector2D Dev(YawOffset, PitchOff);
			const FRotator  Rot  = ComputeLookAtRotation(CamPos, AnchorPos, Dev);
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

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementTilt::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("TiltStartAngle"),  (double)TiltStartAngle);
	OutJson->SetNumberField(TEXT("TiltEndAngle"),    (double)TiltEndAngle);
	OutJson->SetNumberField(TEXT("YawOffset"),       (double)YawOffset);
	OutJson->SetNumberField(TEXT("NumKeyframes"),    (double)NumKeyframes);
	OutJson->SetNumberField(TEXT("SpeedInterpolation"),(double)(uint8)SpeedInterpolation);
	OutJson->SetNumberField(TEXT("DefaultAperture"), (double)DefaultAperture);
}

void UCDGMovementTilt::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("TiltStartAngle")))
		TiltStartAngle = (float)InJson->GetNumberField(TEXT("TiltStartAngle"));
	if (InJson->HasField(TEXT("TiltEndAngle")))
		TiltEndAngle = (float)InJson->GetNumberField(TEXT("TiltEndAngle"));
	if (InJson->HasField(TEXT("YawOffset")))
		YawOffset = (float)InJson->GetNumberField(TEXT("YawOffset"));
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("SpeedInterpolation")))
		SpeedInterpolation = (ECDGSpeedInterpolationMode)(uint8)InJson->GetNumberField(TEXT("SpeedInterpolation"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
