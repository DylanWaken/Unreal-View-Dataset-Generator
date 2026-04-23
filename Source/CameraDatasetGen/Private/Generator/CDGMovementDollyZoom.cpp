// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementDollyZoom.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementDollyZoom::GetGeneratorName_Implementation() const
{
	return FName("MovementDollyZoom");
}

FText UCDGMovementDollyZoom::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementDollyZoomTip",
		"Vertigo / trombone effect: camera moves physically while focal length "
		"changes inversely to keep the subject's screen size constant.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementDollyZoom::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementDollyZoom: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementDollyZoom: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementDollyZoom: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementDollyZoom"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF     = FMath::Max(2, NumKeyframes);
	const float   SegDur    = EffDur / (float)(NumKF - 1);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector StartPos = Placement.Position;
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);

		FVector LookDir = (AnchorPos - StartPos).GetSafeNormal();
		if (LookDir.IsNearlyZero()) LookDir = FVector::ForwardVector;

		// d1 = initial distance to anchor
		const float d1 = FMath::Max(1.0f, FVector::Dist(StartPos, AnchorPos));

		// Camera moves toward or away from anchor
		const float MoveSign = bDollyIn ? 1.0f : -1.0f;

		// Clamp so camera stays at least 10 cm from anchor when dollying in
		const float MaxMove  = bDollyIn ? (d1 - 10.0f) : MoveDistance;
		const float EffMove  = FMath::Clamp(MoveDistance, 0.0f, FMath::Max(0.0f, MaxMove));
		const FVector EndPos = StartPos + LookDir * MoveSign * EffMove;

		// d2 = final distance; fl2 = focal length that keeps subject size constant
		const float d2  = FMath::Max(1.0f, FVector::Dist(EndPos, AnchorPos));
		const float fl2 = FMath::Clamp(StartFocalLength * d2 / d1, 4.0f, 1000.0f);

		UE_LOG(LogCameraDatasetGen, Log,
			TEXT("UCDGMovementDollyZoom: d1=%.0fcm d2=%.0fcm fl1=%.1fmm fl2=%.1fmm"),
			d1, d2, StartFocalLength, fl2);

		for (int32 i = 0; i < NumKF; ++i)
		{
			const float t       = (NumKF > 1) ? (float)i / (float)(NumKF - 1) : 0.0f;
			const FVector Pos   = FMath::Lerp(StartPos, EndPos, t);
			const float FL      = FMath::Lerp(StartFocalLength, fl2, t);
			const FRotator Rot  = ComputeLookAtRotation(Pos, AnchorPos);
			const float TimeToKF = (i == 0) ? 0.0f : SegDur;

			if (ACDGKeyframe* KF = SpawnKeyframe(World, Subsystem,
				Pos, Rot, TrajName, i,
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
		TEXT("UCDGMovementDollyZoom: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementDollyZoom::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("StartFocalLength"),  (double)StartFocalLength);
	OutJson->SetNumberField(TEXT("MoveDistance"),      (double)MoveDistance);
	OutJson->SetBoolField  (TEXT("bDollyIn"),           bDollyIn);
	OutJson->SetNumberField(TEXT("NumKeyframes"),      (double)NumKeyframes);
	OutJson->SetNumberField(TEXT("SpeedInterpolation"),(double)(uint8)SpeedInterpolation);
	OutJson->SetNumberField(TEXT("DefaultAperture"),   (double)DefaultAperture);
}

void UCDGMovementDollyZoom::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("StartFocalLength")))
		StartFocalLength = FMath::Clamp((float)InJson->GetNumberField(TEXT("StartFocalLength")), 4.0f, 1000.0f);
	if (InJson->HasField(TEXT("MoveDistance")))
		MoveDistance = FMath::Max(1.0f, (float)InJson->GetNumberField(TEXT("MoveDistance")));
	if (InJson->HasField(TEXT("bDollyIn")))
		bDollyIn = InJson->GetBoolField(TEXT("bDollyIn"));
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("SpeedInterpolation")))
		SpeedInterpolation = (ECDGSpeedInterpolationMode)(uint8)InJson->GetNumberField(TEXT("SpeedInterpolation"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
