// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementPushIn.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementPushIn::GetGeneratorName_Implementation() const
{
	return FName("MovementPushIn");
}

FText UCDGMovementPushIn::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementPushInTip",
		"Dollies the camera toward the subject over the reference sequence duration. "
		"Builds tension and draws attention to an important story beat.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementPushIn::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementPushIn: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementPushIn: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementPushIn: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementPushIn"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF     = FMath::Max(2, NumKeyframes);
	const float   SegDur    = EffDur / (float)(NumKF - 1);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector StartPos  = Placement.Position;
		const FName   TrajName  = ComposeTrajectoryName(Subsystem, Placement);

		// Direction from camera to anchor; camera moves along this direction
		FVector LookDir = (AnchorPos - StartPos).GetSafeNormal();
		if (LookDir.IsNearlyZero()) LookDir = FVector::ForwardVector;

		// Clamp push so the camera stops at least 10 cm short of the anchor
		const float MaxPush = FMath::Max(0.0f, FVector::Dist(StartPos, AnchorPos) - 10.0f);
		const float EffPush = FMath::Min(PushDistance, MaxPush);
		const FVector EndPos = StartPos + LookDir * EffPush;

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
		TEXT("UCDGMovementPushIn: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementPushIn::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("PushDistance"),    (double)PushDistance);
	OutJson->SetNumberField(TEXT("NumKeyframes"),    (double)NumKeyframes);
	OutJson->SetNumberField(TEXT("SpeedInterpolation"),(double)(uint8)SpeedInterpolation);
	OutJson->SetNumberField(TEXT("DefaultAperture"), (double)DefaultAperture);
}

void UCDGMovementPushIn::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("PushDistance")))
		PushDistance = FMath::Max(1.0f, (float)InJson->GetNumberField(TEXT("PushDistance")));
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("SpeedInterpolation")))
		SpeedInterpolation = (ECDGSpeedInterpolationMode)(uint8)InJson->GetNumberField(TEXT("SpeedInterpolation"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
