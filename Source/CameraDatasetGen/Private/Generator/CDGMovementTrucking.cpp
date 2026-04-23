// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementTrucking.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementTrucking::GetGeneratorName_Implementation() const
{
	return FName("MovementTrucking");
}

FText UCDGMovementTrucking::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementTruckingTip",
		"Slides the camera laterally (perpendicular to the look-at direction) by "
		"TruckDistance centimetres. Positive = right, negative = left.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementTrucking::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementTrucking: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementTrucking: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementTrucking: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementTrucking"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF     = FMath::Max(2, NumKeyframes);
	const float   SegDur    = EffDur / (float)(NumKF - 1);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector StartPos = Placement.Position;
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);

		// Lateral direction = right of the initial look-at (horizontal plane)
		FVector LookDir2D = (AnchorPos - StartPos);
		LookDir2D.Z = 0.0f;
		if (!LookDir2D.Normalize()) LookDir2D = FVector::ForwardVector;

		// Right direction in horizontal plane (positive TruckDistance = truck right)
		const FVector RightDir = FVector::CrossProduct(FVector::UpVector, LookDir2D).GetSafeNormal();
		const FVector TruckVec = RightDir * TruckDistance;
		const FVector EndPos   = StartPos + TruckVec;

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
		TEXT("UCDGMovementTrucking: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementTrucking::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("TruckDistance"),   (double)TruckDistance);
	OutJson->SetNumberField(TEXT("NumKeyframes"),    (double)NumKeyframes);
	OutJson->SetNumberField(TEXT("SpeedInterpolation"),(double)(uint8)SpeedInterpolation);
	OutJson->SetNumberField(TEXT("DefaultAperture"), (double)DefaultAperture);
}

void UCDGMovementTrucking::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("TruckDistance")))
		TruckDistance = (float)InJson->GetNumberField(TEXT("TruckDistance"));
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("SpeedInterpolation")))
		SpeedInterpolation = (ECDGSpeedInterpolationMode)(uint8)InJson->GetNumberField(TEXT("SpeedInterpolation"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
