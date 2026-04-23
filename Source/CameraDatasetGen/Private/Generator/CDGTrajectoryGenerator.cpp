// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGTrajectoryGenerator.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Dom/JsonObject.h"
#include "LogCameraDatasetGen.h"

EGeneratorStage UCDGTrajectoryGenerator::GetGeneratorStage_Implementation() const
{
	return EGeneratorStage::Positioning;
}

FName UCDGTrajectoryGenerator::GetGeneratorName_Implementation() const
{
	return NAME_None;
}

FText UCDGTrajectoryGenerator::GetTip_Implementation() const
{
	return FText::GetEmpty();
}

TArray<ACDGTrajectory*> UCDGTrajectoryGenerator::Generate_Implementation()
{
	// Base implementation is intentionally a no-op.
	// Derived generators override this to produce concrete camera paths.
	return TArray<ACDGTrajectory*>();
}

void UCDGTrajectoryGenerator::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	// Base implementation is intentionally a no-op.
	// Derived generators override this to write their parameters to OutJson.
}

void UCDGTrajectoryGenerator::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	// Base implementation is intentionally a no-op.
	// Derived generators override this to read their parameters from InJson.
}

double UCDGTrajectoryGenerator::GetReferenceDurationSeconds() const
{
	if (!ReferenceSequence)
	{
		return 0.0;
	}

	const UMovieScene* MovieScene = ReferenceSequence->GetMovieScene();
	if (!MovieScene)
	{
		return 0.0;
	}

	const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
	if (PlaybackRange.IsEmpty() || !PlaybackRange.HasLowerBound() || !PlaybackRange.HasUpperBound())
	{
		UE_LOG(LogCameraDatasetGen, Warning,
			TEXT("UCDGTrajectoryGenerator: ReferenceSequence '%s' has an empty or unbounded playback range."),
			*ReferenceSequence->GetName());
		return 0.0;
	}

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	if (!TickResolution.IsValid() || TickResolution.Denominator == 0)
	{
		UE_LOG(LogCameraDatasetGen, Warning,
			TEXT("UCDGTrajectoryGenerator: ReferenceSequence '%s' has an invalid tick resolution."),
			*ReferenceSequence->GetName());
		return 0.0;
	}

	const int64 DurationInTicks =
		(PlaybackRange.GetUpperBoundValue() - PlaybackRange.GetLowerBoundValue()).Value;

	return static_cast<double>(DurationInTicks) / TickResolution.AsDecimal();
}
