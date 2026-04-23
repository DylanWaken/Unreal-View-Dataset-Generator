// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementPan.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementPan::GetGeneratorName_Implementation() const
{
	return FName("MovementPan");
}

FText UCDGMovementPan::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementPanTip",
		"Rotates the camera head horizontally about a fixed position. "
		"PanStartAngle and PanEndAngle are yaw offsets from the direct look-at direction.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementPan::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning,
			TEXT("UCDGMovementPan: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementPan: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementPan: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementPan"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF       = FMath::Max(2, NumKeyframes);
	const float   SegDur      = EffDur / (float)(NumKF - 1);

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementPan: %d placement(s), %d keyframes, pan %.1f→%.1f deg."),
		Placements.Num(), NumKF, PanStartAngle, PanEndAngle);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector CamPos   = Placement.Position;
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);

		for (int32 i = 0; i < NumKF; ++i)
		{
			const float t       = (NumKF > 1) ? (float)i / (float)(NumKF - 1) : 0.0f;
			const float YawOff  = FMath::Lerp(PanStartAngle, PanEndAngle, t);
			const FVector2D Dev(YawOff, PitchOffset);
			const FRotator  Rot = ComputeLookAtRotation(CamPos, AnchorPos, Dev);
			const float TimeToKF = (i == 0) ? 0.0f : SegDur;

			ACDGKeyframe* KF = SpawnKeyframe(World, Subsystem,
				CamPos, Rot, TrajName, i,
				TimeToKF, 0.0f, AnchorPos, DefaultAperture);

			if (KF)
			{
				KF->SpeedInterpolationMode = SpeedInterpolation;
			}
			else
			{
				UE_LOG(LogCameraDatasetGen, Warning,
					TEXT("UCDGMovementPan: Failed to spawn keyframe %d for '%s'."),
					i, *TrajName.ToString());
			}
		}

		Subsystem->RebuildTrajectorySpline(TrajName);

		if (ACDGTrajectory* Traj = Subsystem->GetTrajectory(TrajName))
		{
			CreatedTrajectories.Add(Traj);
		}
	}

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementPan: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementPan::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("PanStartAngle"),    (double)PanStartAngle);
	OutJson->SetNumberField(TEXT("PanEndAngle"),      (double)PanEndAngle);
	OutJson->SetNumberField(TEXT("PitchOffset"),      (double)PitchOffset);
	OutJson->SetNumberField(TEXT("NumKeyframes"),     (double)NumKeyframes);
	OutJson->SetNumberField(TEXT("SpeedInterpolation"),(double)(uint8)SpeedInterpolation);
	OutJson->SetNumberField(TEXT("DefaultAperture"),  (double)DefaultAperture);
}

void UCDGMovementPan::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("PanStartAngle")))
		PanStartAngle = (float)InJson->GetNumberField(TEXT("PanStartAngle"));
	if (InJson->HasField(TEXT("PanEndAngle")))
		PanEndAngle = (float)InJson->GetNumberField(TEXT("PanEndAngle"));
	if (InJson->HasField(TEXT("PitchOffset")))
		PitchOffset = (float)InJson->GetNumberField(TEXT("PitchOffset"));
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("SpeedInterpolation")))
		SpeedInterpolation = (ECDGSpeedInterpolationMode)(uint8)InJson->GetNumberField(TEXT("SpeedInterpolation"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
