// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGMovementGenerator.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "Anchor/CDGCharacterAnchor.h"
#include "LogCameraDatasetGen.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneChannelProxy.h"

#include "Engine/World.h"

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGMovementGenerator::GenerateMovement_Implementation(
	const TArray<FCDGCameraPlacement>& /*Placements*/)
{
	// Base implementation is intentionally a no-op.
	// Concrete subclasses override this to produce trajectory actors.
	return TArray<ACDGTrajectory*>();
}

// ==================== SHARED HELPERS ====================

FVector UCDGMovementGenerator::GetCurrentAnchorWorldLocation() const
{
	if (!PrimaryCharacterActor)
	{
		return FVector::ZeroVector;
	}

	TArray<UCDGCharacterAnchor*> AnchorComponents;
	PrimaryCharacterActor->GetComponents<UCDGCharacterAnchor>(AnchorComponents);

	for (UCDGCharacterAnchor* Anchor : AnchorComponents)
	{
		if (Anchor && Anchor->Type == FocusedAnchor)
		{
			return Anchor->GetComponentLocation();
		}
	}

	UE_LOG(LogCameraDatasetGen, Warning,
		TEXT("UCDGMovementGenerator: No UCDGCharacterAnchor of requested type on '%s'. "
		     "Using actor root location."),
		*PrimaryCharacterActor->GetActorLabel());

	return PrimaryCharacterActor->GetActorLocation();
}

FRotator UCDGMovementGenerator::ComputeLookAtRotation(
	const FVector& CameraPos, const FVector& TargetPos,
	const FVector2D& DeviationDeg) const
{
	FVector LookDir = (TargetPos - CameraPos).GetSafeNormal();
	if (LookDir.IsNearlyZero())
	{
		LookDir = FVector::ForwardVector;
	}

	FRotator BaseRotation = LookDir.Rotation();

	if (!DeviationDeg.IsNearlyZero())
	{
		BaseRotation.Yaw   += DeviationDeg.X;
		BaseRotation.Pitch += DeviationDeg.Y;
	}

	return BaseRotation;
}

TArray<FVector> UCDGMovementGenerator::SampleAnchorPositionsFromSequence(
	const FVector& FallbackPosition) const
{
	TArray<FVector> Positions;

	if (!ReferenceSequence || !PrimaryCharacterActor) return Positions;

	UMovieScene* MovieScene = ReferenceSequence->GetMovieScene();
	if (!MovieScene) return Positions;

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate    = MovieScene->GetDisplayRate();
	const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();

	if (!TickResolution.IsValid() || !DisplayRate.IsValid()
		|| PlaybackRange.IsEmpty()
		|| !PlaybackRange.HasLowerBound()
		|| !PlaybackRange.HasUpperBound())
	{
		return Positions;
	}

	UMovieScene3DTransformSection* TransformSection = nullptr;
	const UMovieScene* ConstMovieScene = MovieScene;

	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid());
		if (!Possessable || !Possessable->GetPossessedObjectClass()) continue;
		if (!PrimaryCharacterActor->IsA(Possessable->GetPossessedObjectClass())) continue;

		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			if (UMovieScene3DTransformTrack* TT = Cast<UMovieScene3DTransformTrack>(Track))
			{
				const TArray<UMovieSceneSection*>& Sections = TT->GetAllSections();
				if (Sections.Num() > 0)
				{
					TransformSection = Cast<UMovieScene3DTransformSection>(Sections[0]);
					break;
				}
			}
		}
		if (TransformSection) break;
	}

	// ---- Anchor component relative offset ----
	FVector AnchorRelativeOffset = FVector::ZeroVector;
	{
		TArray<UCDGCharacterAnchor*> AnchorComponents;
		PrimaryCharacterActor->GetComponents<UCDGCharacterAnchor>(AnchorComponents);
		for (UCDGCharacterAnchor* Anchor : AnchorComponents)
		{
			if (Anchor && Anchor->Type == FocusedAnchor)
			{
				AnchorRelativeOffset = Anchor->GetRelativeLocation();
				break;
			}
		}
	}

	// ---- Build per-frame position array ----
	const FFrameNumber StartTick = PlaybackRange.GetLowerBoundValue();
	const FFrameNumber EndTick   = PlaybackRange.GetUpperBoundValue();

	const FFrameTime StartDisplay =
		FFrameRate::TransformTime(FFrameTime(StartTick), TickResolution, DisplayRate);
	const FFrameTime EndDisplay =
		FFrameRate::TransformTime(FFrameTime(EndTick), TickResolution, DisplayRate);
	const int32 TotalFrames =
		FMath::Max(1, FMath::RoundToInt((float)(EndDisplay - StartDisplay).AsDecimal()));

	Positions.Reserve(TotalFrames + 1);

	for (int32 FrameIdx = 0; FrameIdx <= TotalFrames; ++FrameIdx)
	{
		FVector SampledPos = FallbackPosition;

		if (TransformSection)
		{
			const FFrameTime DisplayFrameTime = FFrameTime(FFrameNumber(FrameIdx));
			const FFrameTime TransformedTime  =
				FFrameRate::TransformTime(DisplayFrameTime, DisplayRate, TickResolution);
			const FFrameTime TickTime = TransformedTime + FFrameTime(StartTick);

			FMovieSceneChannelProxy& Proxy = TransformSection->GetChannelProxy();
			TArrayView<FMovieSceneDoubleChannel* const> DoubleChannels =
				Proxy.GetChannels<FMovieSceneDoubleChannel>();

			if (DoubleChannels.Num() >= 3)
			{
				double TX = FallbackPosition.X;
				double TY = FallbackPosition.Y;
				double TZ = FallbackPosition.Z;
				DoubleChannels[0]->Evaluate(TickTime, TX);
				DoubleChannels[1]->Evaluate(TickTime, TY);
				DoubleChannels[2]->Evaluate(TickTime, TZ);

				if (DoubleChannels.Num() >= 6 && !AnchorRelativeOffset.IsNearlyZero())
				{
					double RotRoll = 0.0, RotPitch = 0.0, RotYaw = 0.0;
					DoubleChannels[3]->Evaluate(TickTime, RotRoll);
					DoubleChannels[4]->Evaluate(TickTime, RotPitch);
					DoubleChannels[5]->Evaluate(TickTime, RotYaw);
					const FRotator CharRot(
						static_cast<float>(RotPitch),
						static_cast<float>(RotYaw),
						static_cast<float>(RotRoll));
					SampledPos = FVector(TX, TY, TZ) + CharRot.RotateVector(AnchorRelativeOffset);
				}
				else
				{
					SampledPos = FVector(TX, TY, TZ) + AnchorRelativeOffset;
				}
			}
		}

		Positions.Add(SampledPos);
	}

	return Positions;
}

float UCDGMovementGenerator::GetValidatedSequenceDuration(const TCHAR* CallerName) const
{
	if (!ReferenceSequence)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("%s: No ReferenceSequence set. Assign a valid Level Sequence before generating."),
			CallerName);
		return 0.0f;
	}

	const double Duration = GetReferenceDurationSeconds();
	if (Duration <= SMALL_NUMBER)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("%s: ReferenceSequence '%s' has zero or invalid duration. "
			     "Ensure the sequence has a valid tick resolution and non-zero playback range."),
			CallerName, *ReferenceSequence->GetName());
		return 0.0f;
	}

	return static_cast<float>(Duration);
}

FName UCDGMovementGenerator::ComposeTrajectoryName(
	UCDGTrajectorySubsystem* Subsystem,
	const FCDGCameraPlacement& Placement) const
{
	// Build a prefix that encodes both the placement identity and the
	// movement subclass so stacking multiple movements on the same placement
	// never writes to the same trajectory asset.
	const FString MovementId = GetGeneratorName().ToString();
	const FString PlacementId = Placement.TrajectoryName.IsNone()
		? TEXT("Placement")
		: Placement.TrajectoryName.ToString();

	const FString Prefix = FString::Printf(TEXT("%s_%s"), *PlacementId, *MovementId);

	if (Subsystem)
	{
		return Subsystem->GenerateUniqueTrajectoryName(Prefix);
	}
	return FName(*Prefix);
}

ACDGKeyframe* UCDGMovementGenerator::SpawnKeyframe(
	UWorld* World, UCDGTrajectorySubsystem* Subsystem,
	const FVector& Position, const FRotator& Rotation,
	FName TrajectoryName, int32 Order,
	float TimeToCurrentFrame, float TimeAtCurrentFrame,
	const FVector& AnchorWorldPos, float Aperture) const
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACDGKeyframe* Keyframe = World->SpawnActor<ACDGKeyframe>(
		ACDGKeyframe::StaticClass(), Position, Rotation, SpawnParams);
	if (!Keyframe) return nullptr;

	const FName AutoName = Keyframe->TrajectoryName;

	Keyframe->OrderInTrajectory  = Order;
	Keyframe->TimeToCurrentFrame = TimeToCurrentFrame;
	Keyframe->TimeAtCurrentFrame = TimeAtCurrentFrame;

	Keyframe->LensSettings.FocusDistance             = FVector::Dist(Position, AnchorWorldPos);
	Keyframe->LensSettings.bUseManualFocusDistance   = true;
	Keyframe->LensSettings.AutofocusTargetActor      = PrimaryCharacterActor.Get();
	Keyframe->LensSettings.AutofocusTargetAnchorType = FocusedAnchor;
	Keyframe->LensSettings.Aperture                  = Aperture;

	Keyframe->TrajectoryName = TrajectoryName;
	if (AutoName != TrajectoryName)
	{
		Subsystem->OnKeyframeTrajectoryNameChanged(Keyframe, AutoName);
	}

	return Keyframe;
}
