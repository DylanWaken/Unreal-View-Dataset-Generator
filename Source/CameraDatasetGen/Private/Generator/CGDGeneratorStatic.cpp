// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CGDGeneratorStatic.h"
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

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

// ==================== IDENTITY ====================

FName UCDGGeneratorStatic::GetGeneratorName_Implementation() const
{
	return FName("StaticGenerator");
}

FText UCDGGeneratorStatic::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "StaticGeneratorTip",
		"Places static cameras at positions uniformly sampled from a spherical shell "
		"around the subject's anchor. Each viewpoint passes a line-of-sight check "
		"against world geometry. In Follow mode the camera rotates per-frame to track "
		"the anchor; in Static mode two keyframes bracket the full shot duration.");
}

// ==================== GENERATION ====================

TArray<ACDGTrajectory*> UCDGGeneratorStatic::Generate_Implementation()
{
	TArray<ACDGTrajectory*> CreatedTrajectories;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGGeneratorStatic: No valid world context. "
			     "Create this generator with a world-context outer (e.g. an actor or subsystem)."));
		return CreatedTrajectories;
	}

	if (!PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGGeneratorStatic: PrimaryCharacterActor is not set."));
		return CreatedTrajectories;
	}

	if (RadiusMax < RadiusMin || RadiusMax <= 0.0f)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGGeneratorStatic: Invalid radius range [%.1f, %.1f]. "
			     "RadiusMax must be >= RadiusMin and > 0."),
			RadiusMin, RadiusMax);
		return CreatedTrajectories;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGGeneratorStatic: UCDGTrajectorySubsystem not found in world."));
		return CreatedTrajectories;
	}

	// RNG
	const int32 SeedToUse = (RandomSeed < 0) ? FMath::Rand() : RandomSeed;
	FRandomStream RNG(SeedToUse);
	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGGeneratorStatic: Starting generation. Count=%d, Radius=[%.1f, %.1f], "
		     "FollowAnchor=%s, Seed=%d"),
		Count, RadiusMin, RadiusMax, bFollowAnchor ? TEXT("true") : TEXT("false"), SeedToUse);

	// Anchor center — first-frame position of the focused anchor on the character
	const FVector AnchorCenter = GetCurrentAnchorWorldLocation();

	// Reference sequence duration
	const double TotalDuration = GetReferenceDurationSeconds();

	// ---- Anchor positions for follow mode ----
	// In follow mode we build a position sample per display-rate frame so each
	// generated keyframe can orient toward the anchor's moving world location.
	TArray<FVector> AnchorPositions;
	if (bFollowAnchor && TotalDuration > SMALL_NUMBER && ReferenceSequence)
	{
		AnchorPositions = SampleAnchorPositionsFromSequence(AnchorCenter);
	}

	// Ensure we always have at least one anchor position (the fixed first-frame pos)
	if (AnchorPositions.Num() == 0)
	{
		AnchorPositions.Add(AnchorCenter);
	}

	// Per-frame duration used when building follow-mode keyframe timing
	const bool bIsFollowMode = bFollowAnchor && (AnchorPositions.Num() > 1);
	const float FrameDuration = bIsFollowMode
		? (float)(TotalDuration / (double)FMath::Max(1, AnchorPositions.Num() - 1))
		: 0.0f;

	// Fallback end-time for static (non-follow) mode
	const float StaticShotDuration = (TotalDuration > SMALL_NUMBER) ? (float)TotalDuration : 5.0f;

	// ---- Sampling loop ----
	int32 GeneratedCount   = 0;
	int32 TotalAttempts    = 0;
	const int32 MaxAttempts = Count * MaxSamplingAttemptsPerShot;

	while (GeneratedCount < Count && TotalAttempts < MaxAttempts)
	{
		++TotalAttempts;

		const FVector CandidatePos = SampleUniformSphericalShell(AnchorCenter, RadiusMin, RadiusMax, RNG);

		// Reject if something (not the character) blocks the line to the first-frame anchor
		if (!HasClearLineOfSight(World, CandidatePos, AnchorCenter))
		{
			continue;
		}

		const FName TrajName = Subsystem->GenerateUniqueTrajectoryName(TEXT("StaticGen"));

		if (bIsFollowMode)
		{
			// One keyframe per anchor sample — camera position fixed, rotation tracks anchor
			for (int32 FrameIdx = 0; FrameIdx < AnchorPositions.Num(); ++FrameIdx)
			{
				const FRotator Rotation = ComputeLookAtRotation(CandidatePos, AnchorPositions[FrameIdx]);
				const float TimeToFrame = (FrameIdx == 0) ? 0.0f : FrameDuration;

				ACDGKeyframe* KF = SpawnKeyframe(
					World, Subsystem,
					CandidatePos, Rotation,
					TrajName, FrameIdx,
					TimeToFrame, 0.0f);

				if (!KF)
				{
					UE_LOG(LogCameraDatasetGen, Warning,
						TEXT("UCDGGeneratorStatic: Failed to spawn keyframe %d for '%s'."),
						FrameIdx, *TrajName.ToString());
				}
			}
		}
		else
		{
			// Two keyframes — fixed position and rotation for the whole shot
			const FRotator FixedRotation = ComputeLookAtRotation(CandidatePos, AnchorCenter);

			SpawnKeyframe(World, Subsystem,
				CandidatePos, FixedRotation,
				TrajName, 0,
				0.0f, 0.0f);

			SpawnKeyframe(World, Subsystem,
				CandidatePos, FixedRotation,
				TrajName, 1,
				StaticShotDuration, 0.0f);
		}

		Subsystem->RebuildTrajectorySpline(TrajName);

		if (ACDGTrajectory* Trajectory = Subsystem->GetTrajectory(TrajName))
		{
			CreatedTrajectories.Add(Trajectory);
		}

		++GeneratedCount;
	}

	if (GeneratedCount < Count)
	{
		UE_LOG(LogCameraDatasetGen, Warning,
			TEXT("UCDGGeneratorStatic: Only generated %d / %d trajectories after %d attempts. "
			     "Scene geometry may be heavily occluding the sampling sphere."),
			GeneratedCount, Count, TotalAttempts);
	}
	else
	{
		UE_LOG(LogCameraDatasetGen, Log,
			TEXT("UCDGGeneratorStatic: Successfully generated %d trajectories."), GeneratedCount);
	}

	return CreatedTrajectories;
}

// ==================== HELPERS ====================

FVector UCDGGeneratorStatic::GetCurrentAnchorWorldLocation() const
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

	// No matching anchor component — fall back to actor root
	UE_LOG(LogCameraDatasetGen, Warning,
		TEXT("UCDGGeneratorStatic: No UCDGCharacterAnchor with the requested type found on '%s'. "
		     "Using actor root location."),
		*PrimaryCharacterActor->GetActorLabel());

	return PrimaryCharacterActor->GetActorLocation();
}

FVector UCDGGeneratorStatic::SampleUniformSphericalShell(
	const FVector& Center, float MinR, float MaxR, FRandomStream& RNG) const
{
	// Uniform volume sampling in a spherical shell via inverse CDF:
	//   r = (r_min^3 + u * (r_max^3 - r_min^3))^(1/3)
	const double R3Min = (double)MinR * (double)MinR * (double)MinR;
	const double R3Max = (double)MaxR * (double)MaxR * (double)MaxR;
	const double U     = (double)RNG.GetFraction();
	const float  R     = (float)FMath::Pow(R3Min + U * (R3Max - R3Min), 1.0 / 3.0);

	// Uniform direction on the unit sphere
	const float CosTheta = RNG.FRandRange(-1.0f, 1.0f);
	const float SinTheta = FMath::Sqrt(FMath::Max(0.0f, 1.0f - CosTheta * CosTheta));
	const float Phi      = RNG.FRandRange(0.0f, 2.0f * PI);

	const FVector Direction(
		SinTheta * FMath::Cos(Phi),
		SinTheta * FMath::Sin(Phi),
		CosTheta);

	return Center + Direction * R;
}

bool UCDGGeneratorStatic::HasClearLineOfSight(
	UWorld* World, const FVector& From, const FVector& To) const
{
	FCollisionQueryParams QueryParams(TEXT("CDGGeneratorStatic_LOS"), /*bTraceComplex=*/false);
	QueryParams.bReturnPhysicalMaterial = false;

	if (PrimaryCharacterActor)
	{
		// Ignore the character actor and all of its components
		QueryParams.AddIgnoredActor(PrimaryCharacterActor.Get());
	}

	FHitResult HitResult;
	const bool bBlocked = World->LineTraceSingleByChannel(
		HitResult, From, To, ECC_Visibility, QueryParams);

	return !bBlocked;
}

FRotator UCDGGeneratorStatic::ComputeLookAtRotation(
	const FVector& CameraPos, const FVector& TargetPos) const
{
	FVector LookDir = (TargetPos - CameraPos).GetSafeNormal();
	if (LookDir.IsNearlyZero())
	{
		LookDir = FVector::ForwardVector;
	}

	FRotator BaseRotation = LookDir.Rotation();

	// Apply camera-space angular offset (X = yaw/horizontal, Y = pitch/vertical)
	if (!ViewDirectionDeviation.IsNearlyZero())
	{
		BaseRotation.Yaw   += ViewDirectionDeviation.X;
		BaseRotation.Pitch += ViewDirectionDeviation.Y;
	}

	return BaseRotation;
}

TArray<FVector> UCDGGeneratorStatic::SampleAnchorPositionsFromSequence(
	const FVector& FallbackPosition) const
{
	TArray<FVector> Positions;

	if (!ReferenceSequence || !PrimaryCharacterActor)
	{
		return Positions;
	}

	UMovieScene* MovieScene = ReferenceSequence->GetMovieScene();
	if (!MovieScene)
	{
		return Positions;
	}

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

	// ---- Locate the character's 3D transform section in the reference sequence ----
	UMovieScene3DTransformSection* TransformSection = nullptr;

	const UMovieScene* ConstMovieScene = MovieScene;
	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid());
		if (!Possessable || !Possessable->GetPossessedObjectClass())
		{
			continue;
		}

		// Match by class — first possessable whose class is a parent of the character's class
		if (!PrimaryCharacterActor->IsA(Possessable->GetPossessedObjectClass()))
		{
			continue;
		}

		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			if (UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(Track))
			{
				const TArray<UMovieSceneSection*>& Sections = TransformTrack->GetAllSections();
				if (Sections.Num() > 0)
				{
					TransformSection = Cast<UMovieScene3DTransformSection>(Sections[0]);
					break;
				}
			}
		}

		if (TransformSection)
		{
			break;
		}
	}

	if (!TransformSection)
	{
		UE_LOG(LogCameraDatasetGen, Log,
			TEXT("UCDGGeneratorStatic: No 3D transform track found for '%s' in reference sequence. "
			     "Using constant anchor position for follow-mode keyframes."),
			*PrimaryCharacterActor->GetActorLabel());
	}

	// ---- Anchor component relative-location offset (ignore rotation for first pass) ----
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

	// Number of display-rate frames spanning the playback range
	const FFrameTime StartDisplay = FFrameRate::TransformTime(FFrameTime(StartTick), TickResolution, DisplayRate);
	const FFrameTime EndDisplay   = FFrameRate::TransformTime(FFrameTime(EndTick),   TickResolution, DisplayRate);
	const int32 TotalFrames       = FMath::Max(1, FMath::RoundToInt(
		(float)(EndDisplay - StartDisplay).AsDecimal()));

	Positions.Reserve(TotalFrames + 1);

	// Translation channel indices in UMovieScene3DTransformSection's channel proxy:
	//   0 = Translation.X,  1 = Translation.Y,  2 = Translation.Z
	//   3 = Rotation.X, ...  6 = Scale.X, ...
	for (int32 FrameIdx = 0; FrameIdx <= TotalFrames; ++FrameIdx)
	{
		FVector SampledPos = FallbackPosition;

		if (TransformSection)
		{
			// Convert display-frame index → absolute tick time.
			// Use assignment-form construction to avoid the "most vexing parse" where
			// FFrameTime Foo(FFrameNumber(x)) is misread as a function declaration.
			const FFrameTime DisplayFrameTime = FFrameTime(FFrameNumber(FrameIdx));
			const FFrameTime TransformedTime  = FFrameRate::TransformTime(
				DisplayFrameTime, DisplayRate, TickResolution);
			const FFrameTime TickTime         = TransformedTime + FFrameTime(StartTick);

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

				// Compose character world translation with anchor local offset.
				// Note: this ignores character rotation for the offset — a future improvement
				// can apply the full transform by also sampling the rotation channels.
				SampledPos = FVector(TX, TY, TZ) + AnchorRelativeOffset;
			}
		}

		Positions.Add(SampledPos);
	}

	return Positions;
}

ACDGKeyframe* UCDGGeneratorStatic::SpawnKeyframe(
	UWorld* World, UCDGTrajectorySubsystem* Subsystem,
	const FVector& Position, const FRotator& Rotation,
	FName TrajectoryName, int32 Order,
	float TimeToCurrentFrame, float TimeAtCurrentFrame) const
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACDGKeyframe* Keyframe = World->SpawnActor<ACDGKeyframe>(
		ACDGKeyframe::StaticClass(), Position, Rotation, SpawnParams);

	if (!Keyframe)
	{
		return nullptr;
	}

	// PostActorCreated (editor) auto-assigned a unique trajectory name and registered
	// the keyframe.  Record that name before overwriting so we can notify the subsystem.
	const FName AutoName = Keyframe->TrajectoryName;

	Keyframe->OrderInTrajectory  = Order;
	Keyframe->TimeToCurrentFrame = TimeToCurrentFrame;
	Keyframe->TimeAtCurrentFrame = TimeAtCurrentFrame;

	// Wire up autofocus so the exporter computes per-frame focus distance correctly
	Keyframe->LensSettings.AutofocusTargetActor      = PrimaryCharacterActor.Get();
	Keyframe->LensSettings.AutofocusTargetAnchorType = FocusedAnchor;

	// Move from auto-generated trajectory → desired trajectory
	Keyframe->TrajectoryName = TrajectoryName;
	if (AutoName != TrajectoryName)
	{
		Subsystem->OnKeyframeTrajectoryNameChanged(Keyframe, AutoName);
	}

	return Keyframe;
}

// ==================== SERIALIZATION ====================

void UCDGGeneratorStatic::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid())
	{
		OutJson = MakeShared<FJsonObject>();
	}

	OutJson->SetNumberField(TEXT("Count"),                  (double)Count);
	OutJson->SetNumberField(TEXT("RadiusMin"),              (double)RadiusMin);
	OutJson->SetNumberField(TEXT("RadiusMax"),              (double)RadiusMax);
	OutJson->SetBoolField  (TEXT("bFollowAnchor"),          bFollowAnchor);
	OutJson->SetNumberField(TEXT("ViewDirectionDeviationX"), (double)ViewDirectionDeviation.X);
	OutJson->SetNumberField(TEXT("ViewDirectionDeviationY"), (double)ViewDirectionDeviation.Y);
	OutJson->SetNumberField(TEXT("MaxSamplingAttemptsPerShot"), (double)MaxSamplingAttemptsPerShot);
	OutJson->SetNumberField(TEXT("RandomSeed"),             (double)RandomSeed);
}

void UCDGGeneratorStatic::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid())
	{
		return;
	}

	if (InJson->HasField(TEXT("Count")))
	{
		Count = FMath::Max(1, (int32)InJson->GetNumberField(TEXT("Count")));
	}
	if (InJson->HasField(TEXT("RadiusMin")))
	{
		RadiusMin = (float)InJson->GetNumberField(TEXT("RadiusMin"));
	}
	if (InJson->HasField(TEXT("RadiusMax")))
	{
		RadiusMax = (float)InJson->GetNumberField(TEXT("RadiusMax"));
	}
	if (InJson->HasField(TEXT("bFollowAnchor")))
	{
		bFollowAnchor = InJson->GetBoolField(TEXT("bFollowAnchor"));
	}
	if (InJson->HasField(TEXT("ViewDirectionDeviationX")))
	{
		ViewDirectionDeviation.X = (float)InJson->GetNumberField(TEXT("ViewDirectionDeviationX"));
	}
	if (InJson->HasField(TEXT("ViewDirectionDeviationY")))
	{
		ViewDirectionDeviation.Y = (float)InJson->GetNumberField(TEXT("ViewDirectionDeviationY"));
	}
	if (InJson->HasField(TEXT("MaxSamplingAttemptsPerShot")))
	{
		MaxSamplingAttemptsPerShot = FMath::Max(1, (int32)InJson->GetNumberField(TEXT("MaxSamplingAttemptsPerShot")));
	}
	if (InJson->HasField(TEXT("RandomSeed")))
	{
		RandomSeed = (int32)InJson->GetNumberField(TEXT("RandomSeed"));
	}
}
