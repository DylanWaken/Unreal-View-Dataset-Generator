// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementNone.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"

// ==================== IDENTITY ====================

FName UCDGMovementNone::GetGeneratorName_Implementation() const
{
	return FName("MovementNone");
}

FText UCDGMovementNone::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "MovementNoneTip",
		"Produces static camera shots with no translational motion. "
		"In Static mode two keyframes bracket the sequence at a fixed look-at orientation. "
		"In Follow mode one keyframe per frame tracks the anchor's world position.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementNone::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& Placements)
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	if (Placements.IsEmpty())
	{
		UE_LOG(LogCameraDatasetGen, Warning,
			TEXT("UCDGMovementNone: Received empty placement list from positioning stage."));
		return CreatedTrajectories;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementNone: No valid world context."));
		return CreatedTrajectories;
	}

	if (!PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementNone: PrimaryCharacterActor is not set."));
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGMovementNone: UCDGTrajectorySubsystem not found in world."));
		return CreatedTrajectories;
	}

	// Anchor first-frame position
	const FVector AnchorCenter = GetCurrentAnchorWorldLocation();

	// Reference sequence duration — required; abort if not set or zero
	const float SequenceDuration = GetValidatedSequenceDuration(TEXT("UCDGMovementNone"));
	if (SequenceDuration <= 0.0f) return CreatedTrajectories;

	// ---- Anchor positions for follow mode ----
	TArray<FVector> AnchorPositions;
	if (bFollowAnchor)
	{
		AnchorPositions = SampleAnchorPositionsFromSequence(AnchorCenter);
	}
	if (AnchorPositions.IsEmpty())
	{
		AnchorPositions.Add(AnchorCenter);
	}

	const bool bIsFollowMode = bFollowAnchor && (AnchorPositions.Num() > 1);
	const float FrameDuration = bIsFollowMode
		? SequenceDuration / (float)FMath::Max(1, AnchorPositions.Num() - 1)
		: 0.0f;
	const float StaticShotDuration = SequenceDuration;

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementNone: Processing %d placement(s). FollowAnchor=%s"),
		Placements.Num(), bIsFollowMode ? TEXT("true") : TEXT("false"));

	for (const FCDGCameraPlacement& Placement : Placements)
	{
		const FVector& CamPos    = Placement.Position;
		const FName    TrajName  = ComposeTrajectoryName(Subsystem, Placement);

		if (bIsFollowMode)
		{
			for (int32 FrameIdx = 0; FrameIdx < AnchorPositions.Num(); ++FrameIdx)
			{
				const FRotator Rotation =
					ComputeLookAtRotation(CamPos, AnchorPositions[FrameIdx], ViewDirectionDeviation);
				const float TimeToFrame = (FrameIdx == 0) ? 0.0f : FrameDuration;

				ACDGKeyframe* KF = SpawnKeyframe(
					World, Subsystem,
					CamPos, Rotation,
					TrajName, FrameIdx,
					TimeToFrame, 0.0f,
					AnchorPositions[FrameIdx], DefaultAperture);

				if (!KF)
				{
					UE_LOG(LogCameraDatasetGen, Warning,
						TEXT("UCDGMovementNone: Failed to spawn keyframe %d for '%s'."),
						FrameIdx, *TrajName.ToString());
				}
			}
		}
		else
		{
			const FRotator FixedRotation = ComputeLookAtRotation(CamPos, AnchorCenter, ViewDirectionDeviation);

			SpawnKeyframe(World, Subsystem,
				CamPos, FixedRotation, TrajName, 0,
				0.0f, 0.0f, AnchorCenter, DefaultAperture);

			SpawnKeyframe(World, Subsystem,
				CamPos, FixedRotation, TrajName, 1,
				StaticShotDuration, 0.0f, AnchorCenter, DefaultAperture);
		}

		Subsystem->RebuildTrajectorySpline(TrajName);

		if (ACDGTrajectory* Trajectory = Subsystem->GetTrajectory(TrajName))
		{
			CreatedTrajectories.Add(Trajectory);
		}
	}

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGMovementNone: Created %d trajectory/ies."), CreatedTrajectories.Num());

	return CreatedTrajectories;
}

// ==================== SERIALIZATION ====================

void UCDGMovementNone::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid()) OutJson = MakeShared<FJsonObject>();

	OutJson->SetBoolField  (TEXT("bFollowAnchor"),            bFollowAnchor);
	OutJson->SetNumberField(TEXT("ViewDirectionDeviationX"),  (double)ViewDirectionDeviation.X);
	OutJson->SetNumberField(TEXT("ViewDirectionDeviationY"),  (double)ViewDirectionDeviation.Y);
	OutJson->SetNumberField(TEXT("DefaultAperture"),          (double)DefaultAperture);
	OutJson->SetNumberField(TEXT("FocusedAnchor"),            (double)(uint8)FocusedAnchor);
}

void UCDGMovementNone::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("bFollowAnchor")))
		bFollowAnchor = InJson->GetBoolField(TEXT("bFollowAnchor"));
	if (InJson->HasField(TEXT("ViewDirectionDeviationX")))
		ViewDirectionDeviation.X = (float)InJson->GetNumberField(TEXT("ViewDirectionDeviationX"));
	if (InJson->HasField(TEXT("ViewDirectionDeviationY")))
		ViewDirectionDeviation.Y = (float)InJson->GetNumberField(TEXT("ViewDirectionDeviationY"));
	if (InJson->HasField(TEXT("DefaultAperture")))
		DefaultAperture = FMath::Clamp(
			(float)InJson->GetNumberField(TEXT("DefaultAperture")), 1.2f, 22.0f);
	if (InJson->HasField(TEXT("FocusedAnchor")))
	{
		const uint8 AnchorVal    = (uint8)InJson->GetNumberField(TEXT("FocusedAnchor"));
		const uint8 MaxAnchorVal = (uint8)AnchorType::CDG_ANCHOR_HAND_RIGHT;
		if (AnchorVal <= MaxAnchorVal)
			FocusedAnchor = (AnchorType)AnchorVal;
	}
}
