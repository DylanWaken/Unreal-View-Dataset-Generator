// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementTracking.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementTracking::GetGeneratorName_Implementation() const
{
	return FName("MovementTracking");
}

FText UCDGMovementTracking::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementTrackingTip",
		"Camera follows the subject at a constant offset sampled from every display-rate "
		"frame of the reference sequence. Equivalent to a tracking dolly or Steadicam rig.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementTracking::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("UCDGMovementTracking: Empty placement list."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World || !PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementTracking: Missing world context or PrimaryCharacterActor."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("UCDGMovementTracking: UCDGTrajectorySubsystem not found."));
		return CreatedTrajectories;
	}

	// Sample anchor world positions for every display-rate frame
	const FVector FirstAnchorPos = GetCurrentAnchorWorldLocation();
	TArray<FVector> AnchorPositions = SampleAnchorPositionsFromSequence(FirstAnchorPos);
	if (AnchorPositions.IsEmpty())
	{
		AnchorPositions.Add(FirstAnchorPos);
	}

	const float EffDur = GetValidatedSequenceDuration(TEXT("UCDGMovementTracking"));
	if (EffDur <= 0.0f) return CreatedTrajectories;
	const int32  NumFrames = AnchorPositions.Num();
	const float  FrameDur  = (NumFrames > 1) ? EffDur / (float)(NumFrames - 1) : EffDur;

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FName TrajName = ComposeTrajectoryName(Subsystem, Placement);

		// Derive camera offset from anchor at first frame
		FVector Offset = Placement.Position - AnchorPositions[0] + AdditionalOffset;

		for (int32 i = 0; i < NumFrames; ++i)
		{
			FVector CamPos = AnchorPositions[i] + Offset;
			if (!bTrackVertical)
			{
				CamPos.Z = Placement.Position.Z;
			}

			const FRotator Rot     = ComputeLookAtRotation(CamPos, AnchorPositions[i]);
			const float TimeToKF   = (i == 0) ? 0.0f : FrameDur;

			SpawnKeyframe(World, Subsystem,
				CamPos, Rot, TrajName, i,
				TimeToKF, 0.0f, AnchorPositions[i], DefaultAperture);
		}

		Subsystem->RebuildTrajectorySpline(TrajName);

		if (ACDGTrajectory* Traj = Subsystem->GetTrajectory(TrajName))
		{
			CreatedTrajectories.Add(Traj);
		}
	}

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementTracking: Created %d trajectory/ies, %d frames each."),
		CreatedTrajectories.Num(), AnchorPositions.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementTracking::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetNumberField(TEXT("AdditionalOffsetX"), (double)AdditionalOffset.X);
	OutJson->SetNumberField(TEXT("AdditionalOffsetY"), (double)AdditionalOffset.Y);
	OutJson->SetNumberField(TEXT("AdditionalOffsetZ"), (double)AdditionalOffset.Z);
	OutJson->SetBoolField  (TEXT("bTrackVertical"),     bTrackVertical);
	OutJson->SetNumberField(TEXT("DefaultAperture"),   (double)DefaultAperture);
}

void UCDGMovementTracking::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("AdditionalOffsetX")))
		AdditionalOffset.X = (float)InJson->GetNumberField(TEXT("AdditionalOffsetX"));
	if (InJson->HasField(TEXT("AdditionalOffsetY")))
		AdditionalOffset.Y = (float)InJson->GetNumberField(TEXT("AdditionalOffsetY"));
	if (InJson->HasField(TEXT("AdditionalOffsetZ")))
		AdditionalOffset.Z = (float)InJson->GetNumberField(TEXT("AdditionalOffsetZ"));
	if (InJson->HasField(TEXT("bTrackVertical")))
		bTrackVertical = InJson->GetBoolField(TEXT("bTrackVertical"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp((float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
}
