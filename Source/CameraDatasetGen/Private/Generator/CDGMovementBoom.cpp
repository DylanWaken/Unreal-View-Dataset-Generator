// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementBoom.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementBoom::GetGeneratorName_Implementation() const
{
	return FName("MovementBoom");
}

FText UCDGMovementBoom::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementBoomTip",
		"Translates the camera vertically (boom up / crane shot). "
		"Positive BoomDistance = up; negative = down. "
		"bLookAtSubjectThroughout keeps the subject centred during the rise.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementBoom::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementBoom: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementBoom: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementBoom: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementBoom"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF     = FMath::Max(2, NumKeyframes);
	const float   SegDur    = EffDur / (float)(NumKF - 1);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector StartPos = Placement.Position;
		const FVector EndPos   = StartPos + FVector(0.0f, 0.0f, BoomDistance);
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);

		// Fixed look-at used when bLookAtSubjectThroughout is false
		const FRotator FixedRot = ComputeLookAtRotation(StartPos, AnchorPos);

		for (int32 i = 0; i < NumKF; ++i)
		{
			const float t       = (NumKF > 1) ? (float)i / (float)(NumKF - 1) : 0.0f;
			const FVector Pos   = FMath::Lerp(StartPos, EndPos, t);
			const FRotator Rot  = bLookAtSubjectThroughout
				? ComputeLookAtRotation(Pos, AnchorPos)
				: FixedRot;
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
		TEXT("UCDGMovementBoom: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementBoom::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("BoomDistance"),              (double)BoomDistance);
	OutJson->SetNumberField(TEXT("NumKeyframes"),              (double)NumKeyframes);
	OutJson->SetBoolField  (TEXT("bLookAtSubjectThroughout"),   bLookAtSubjectThroughout);
	OutJson->SetNumberField(TEXT("SpeedInterpolation"),        (double)(uint8)SpeedInterpolation);
	OutJson->SetNumberField(TEXT("DefaultAperture"),           (double)DefaultAperture);
}

void UCDGMovementBoom::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("BoomDistance")))
		BoomDistance = (float)InJson->GetNumberField(TEXT("BoomDistance"));
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("bLookAtSubjectThroughout")))
		bLookAtSubjectThroughout = InJson->GetBoolField(TEXT("bLookAtSubjectThroughout"));
	if (InJson->HasField(TEXT("SpeedInterpolation")))
		SpeedInterpolation = (ECDGSpeedInterpolationMode)(uint8)InJson->GetNumberField(TEXT("SpeedInterpolation"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
