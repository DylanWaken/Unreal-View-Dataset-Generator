// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementRandom.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"

// ==================== IDENTITY ====================

FName UCDGMovementRandom::GetGeneratorName_Implementation() const
{
	return FName("MovementRandom");
}

FText UCDGMovementRandom::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementRandomTip",
		"Produces a pseudo-random handheld-style camera path. "
		"Seeded with RandomSeed for reproducibility (-1 = non-deterministic).");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementRandom::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementRandom: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementRandom: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementRandom: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementRandom"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF     = FMath::Max(2, NumKeyframes);
	const float   SegDur    = EffDur / (float)(NumKF - 1);

	// Per-trajectory seed offset so each placement gets a unique stream
	const int32 BaseSeed = (RandomSeed >= 0) ? RandomSeed : FMath::Rand();

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementRandom: seed=%d, %d placements, %d keyframes."),
		BaseSeed, Placements.Num(), NumKF);

	for (int32 PlacementIdx = 0; PlacementIdx < Placements.Num(); ++PlacementIdx)
	{
		const FCDGCameraPlacement& Placement = Placements[PlacementIdx];
		const FName TrajName = ComposeTrajectoryName(Subsystem, Placement);

		FRandomStream RNG(BaseSeed + PlacementIdx * 1000);

		for (int32 i = 0; i < NumKF; ++i)
		{
			// Position: random offset inside a sphere
			FVector PosNoise = FVector::ZeroVector;
			if (PositionNoiseMagnitude > SMALL_NUMBER)
			{
				PosNoise = RNG.GetUnitVector() * RNG.FRandRange(0.0f, PositionNoiseMagnitude);
			}
			const FVector Pos = Placement.Position + PosNoise;

			// Rotation: base look-at + random yaw/pitch perturbation
			FVector2D RotNoise = FVector2D::ZeroVector;
			if (RotationNoiseMagnitude > SMALL_NUMBER)
			{
				RotNoise.X = RNG.FRandRange(-RotationNoiseMagnitude, RotationNoiseMagnitude);
				RotNoise.Y = RNG.FRandRange(-RotationNoiseMagnitude, RotationNoiseMagnitude);
			}
			const FRotator Rot = ComputeLookAtRotation(Pos, AnchorPos, RotNoise);

			// Focal length variation
			float FL = BaseFocalLength;
			if (FocalLengthVariation > SMALL_NUMBER)
			{
				FL = FMath::Clamp(
					BaseFocalLength + RNG.FRandRange(-FocalLengthVariation, FocalLengthVariation),
					4.0f, 1000.0f);
			}

			const float TimeToKF = (i == 0) ? 0.0f : SegDur;

			if (ACDGKeyframe* KF = SpawnKeyframe(World, Subsystem,
				Pos, Rot, TrajName, i,
				TimeToKF, 0.0f, AnchorPos, DefaultAperture))
			{
				KF->LensSettings.FocalLength = FL;
				KF->SpeedInterpolationMode   = ECDGSpeedInterpolationMode::Cubic;
			}
		}

		Subsystem->RebuildTrajectorySpline(TrajName);

		if (ACDGTrajectory* Traj = Subsystem->GetTrajectory(TrajName))
		{
			CreatedTrajectories.Add(Traj);
		}
	}

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementRandom: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementRandom::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("PositionNoiseMagnitude"), (double)PositionNoiseMagnitude);
	OutJson->SetNumberField(TEXT("RotationNoiseMagnitude"), (double)RotationNoiseMagnitude);
	OutJson->SetNumberField(TEXT("BaseFocalLength"),        (double)BaseFocalLength);
	OutJson->SetNumberField(TEXT("FocalLengthVariation"),   (double)FocalLengthVariation);
	OutJson->SetNumberField(TEXT("NumKeyframes"),           (double)NumKeyframes);
	OutJson->SetNumberField(TEXT("RandomSeed"),             (double)RandomSeed);
	OutJson->SetNumberField(TEXT("DefaultAperture"),        (double)DefaultAperture);
}

void UCDGMovementRandom::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("PositionNoiseMagnitude")))
		PositionNoiseMagnitude = FMath::Max(0.0f, (float)InJson->GetNumberField(TEXT("PositionNoiseMagnitude")));
	if (InJson->HasField(TEXT("RotationNoiseMagnitude")))
		RotationNoiseMagnitude = FMath::Max(0.0f, (float)InJson->GetNumberField(TEXT("RotationNoiseMagnitude")));
	if (InJson->HasField(TEXT("BaseFocalLength")))
		BaseFocalLength = FMath::Clamp((float)InJson->GetNumberField(TEXT("BaseFocalLength")), 4.0f, 1000.0f);
	if (InJson->HasField(TEXT("FocalLengthVariation")))
		FocalLengthVariation = FMath::Max(0.0f, (float)InJson->GetNumberField(TEXT("FocalLengthVariation")));
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("RandomSeed")))
		RandomSeed = (int32)InJson->GetNumberField(TEXT("RandomSeed"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
