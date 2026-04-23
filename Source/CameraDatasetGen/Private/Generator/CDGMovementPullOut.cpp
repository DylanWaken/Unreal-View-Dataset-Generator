// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementPullOut.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementPullOut::GetGeneratorName_Implementation() const
{
	return FName("MovementPullOut");
}

FText UCDGMovementPullOut::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementPullOutTip",
		"Dollies the camera away from the subject over the reference sequence duration. "
		"Reveals environment context or creates a sense of isolation.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementPullOut::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementPullOut: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementPullOut: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementPullOut: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementPullOut"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF     = FMath::Max(2, NumKeyframes);
	const float   SegDur    = EffDur / (float)(NumKF - 1);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector StartPos = Placement.Position;
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);

		// Direction away from anchor
		FVector AwayDir = (StartPos - AnchorPos).GetSafeNormal();
		if (AwayDir.IsNearlyZero()) AwayDir = -FVector::ForwardVector;

		const FVector EndPos = StartPos + AwayDir * PullDistance;

		for (int32 i = 0; i < NumKF; ++i)
		{
			const float t       = (NumKF > 1) ? (float)i / (float)(NumKF - 1) : 0.0f;
			const FVector Pos   = FMath::Lerp(StartPos, EndPos, t);
			const FRotator Rot  = ComputeLookAtRotation(Pos, AnchorPos);
			const float TimeToKF = (i == 0) ? 0.0f : SegDur;

			if (ACDGKeyframe* KF = SpawnKeyframe(World, Subsystem,
				Pos, Rot, TrajName, i,
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
		TEXT("UCDGMovementPullOut: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementPullOut::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("PullDistance"),    (double)PullDistance);
	OutJson->SetNumberField(TEXT("NumKeyframes"),    (double)NumKeyframes);
	OutJson->SetNumberField(TEXT("SpeedInterpolation"),(double)(uint8)SpeedInterpolation);
	OutJson->SetNumberField(TEXT("DefaultAperture"), (double)DefaultAperture);
}

void UCDGMovementPullOut::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("PullDistance")))
		PullDistance = FMath::Max(1.0f, (float)InJson->GetNumberField(TEXT("PullDistance")));
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("SpeedInterpolation")))
		SpeedInterpolation = (ECDGSpeedInterpolationMode)(uint8)InJson->GetNumberField(TEXT("SpeedInterpolation"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
