// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementArc.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementArc::GetGeneratorName_Implementation() const
{
	return FName("MovementArc");
}

FText UCDGMovementArc::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementArcTip",
		"Orbits the camera around the subject on a horizontal circular arc. "
		"Radius and height are preserved from the placement position.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementArc::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementArc: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementArc: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementArc: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	const FVector AnchorPos = GetCurrentAnchorWorldLocation();
	const float   EffDur   = GetValidatedSequenceDuration(TEXT("UCDGMovementArc"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32   NumKF     = FMath::Max(2, NumKeyframes);
	const float   SegDur    = EffDur / (float)(NumKF - 1);

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector StartPos = Placement.Position;
		const FName   TrajName = ComposeTrajectoryName(Subsystem, Placement);

		// Horizontal offset from anchor to start position
		const FVector HorizOffset = FVector(StartPos.X - AnchorPos.X, StartPos.Y - AnchorPos.Y, 0.0f);
		const float   Radius      = HorizOffset.Size();

		// Start angle (radians) in the horizontal plane
		const float StartAngleRad = FMath::Atan2(HorizOffset.Y, Radius > SMALL_NUMBER ? HorizOffset.X : 1.0f);

		// Camera height relative to anchor is preserved
		const float RelativeZ = StartPos.Z - AnchorPos.Z;

		// Sweep direction: counter-clockwise = +angle, clockwise = -angle
		const float SweepSign     = bClockwise ? -1.0f : 1.0f;
		const float TotalSweepRad = FMath::DegreesToRadians(ArcAngle) * SweepSign;

		UE_LOG(LogCameraDatasetGen, Log,
			TEXT("UCDGMovementArc: Radius=%.0fcm, StartAngle=%.1fdeg, Sweep=%.1fdeg"),
			Radius, FMath::RadiansToDegrees(StartAngleRad), ArcAngle * SweepSign);

		for (int32 i = 0; i < NumKF; ++i)
		{
			const float t         = (NumKF > 1) ? (float)i / (float)(NumKF - 1) : 0.0f;
			const float AngleRad  = StartAngleRad + TotalSweepRad * t;

			FVector Pos;
			Pos.X = AnchorPos.X + FMath::Cos(AngleRad) * Radius;
			Pos.Y = AnchorPos.Y + FMath::Sin(AngleRad) * Radius;
			Pos.Z = AnchorPos.Z + RelativeZ;

			const FRotator Rot   = ComputeLookAtRotation(Pos, AnchorPos);
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
		TEXT("UCDGMovementArc: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementArc::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("ArcAngle"),        (double)ArcAngle);
	OutJson->SetBoolField  (TEXT("bClockwise"),       bClockwise);
	OutJson->SetNumberField(TEXT("NumKeyframes"),    (double)NumKeyframes);
	OutJson->SetNumberField(TEXT("SpeedInterpolation"),(double)(uint8)SpeedInterpolation);
	OutJson->SetNumberField(TEXT("DefaultAperture"), (double)DefaultAperture);
}

void UCDGMovementArc::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("ArcAngle")))
		ArcAngle = FMath::Clamp((float)InJson->GetNumberField(TEXT("ArcAngle")), 1.0f, 360.0f);
	if (InJson->HasField(TEXT("bClockwise")))
		bClockwise = InJson->GetBoolField(TEXT("bClockwise"));
	if (InJson->HasField(TEXT("NumKeyframes")))
		NumKeyframes = FMath::Max(2, (int32)InJson->GetNumberField(TEXT("NumKeyframes")));
	if (InJson->HasField(TEXT("SpeedInterpolation")))
		SpeedInterpolation = (ECDGSpeedInterpolationMode)(uint8)InJson->GetNumberField(TEXT("SpeedInterpolation"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
