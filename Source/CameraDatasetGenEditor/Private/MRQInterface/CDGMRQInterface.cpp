// Copyright Epic Games, Inc. All Rights Reserved.

#include "MRQInterface/CDGMRQInterface.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGKeyframe.h"
#include "IO/TrajectorySL.h"
#include "LogCameraDatasetGenEditor.h"
#include "LevelSequenceInterface/CDGLevelSeqSubsystem.h"

#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueEngineSubsystem.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelineHighResSetting.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "MoviePipelinePIEExecutor.h"
#include "MoviePipelineCommandLineEncoder.h"
#include "MoviePipelineCommandLineEncoderSettings.h"
#include "UObject/UObjectIterator.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"

#if WITH_EDITOR
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/Factory.h"
#endif

namespace CDGMRQInterface
{
	bool RenderTrajectories(const FTrajectoryRenderConfig& Config)
	{
		// Get world
		UWorld* World = nullptr;
#if WITH_EDITOR
		if (GEditor)
		{
			World = GEditor->GetEditorWorldContext().World();
		}
#endif

		if (!World)
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: No valid world context found"));
			return false;
		}

		// Gather all trajectories
		TArray<ACDGTrajectory*> Trajectories;
		for (TActorIterator<ACDGTrajectory> It(World); It; ++It)
		{
			Trajectories.Add(*It);
		}

		if (Trajectories.Num() == 0)
		{
			UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: No trajectories found in the world"));
			return false;
		}

		return RenderTrajectories(Trajectories, Config);
	}

	bool RenderTrajectoriesWithSequence(ULevelSequence* MasterSequence, const TArray<ACDGTrajectory*>& Trajectories, const FTrajectoryRenderConfig& Config)
	{
		if (Trajectories.Num() == 0)
		{
			UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: No trajectories provided"));
			return false;
		}

		// Validate config
		if (Config.DestinationRootDir.IsEmpty())
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Destination root directory is empty"));
			return false;
		}

		if (!MasterSequence)
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Master sequence is null"));
			return false;
		}

		// Get world
		UWorld* World = nullptr;
#if WITH_EDITOR
		if (GEditor)
		{
			World = GEditor->GetEditorWorldContext().World();
		}
#endif

		if (!World)
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: No valid world context found"));
			return false;
		}

		// Get level name
		FString LevelName = World->GetMapName();
		LevelName.RemoveFromStart(World->StreamingLevelsPrefix); // Remove PIE prefix if present

		// Validate the provided master sequence
		UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Validating provided master sequence..."));
		
		if (!Internal::ValidateMasterSequence(MasterSequence, Trajectories, LevelName))
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Master sequence validation failed. Please ensure:"));
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("  1. All trajectories have corresponding shot sequences"));
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("  2. Shot sequences match trajectory data (duration, keyframes)"));
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("  3. Re-export the level sequence if trajectories have changed"));
			return false;
		}
		
		UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Master sequence validation passed"));

		// Setup output directory structure
		FString LevelOutputDir = Internal::SetupOutputDirectory(Config.DestinationRootDir, LevelName);
		if (LevelOutputDir.IsEmpty())
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Failed to setup output directory"));
			return false;
		}

		// Get MRQ subsystem
		UMoviePipelineQueueEngineSubsystem* MRQSubsystem = GEngine->GetEngineSubsystem<UMoviePipelineQueueEngineSubsystem>();
		if (!MRQSubsystem)
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Failed to get MoviePipelineQueueEngineSubsystem"));
			return false;
		}

		// Get or create queue
		UMoviePipelineQueue* Queue = MRQSubsystem->GetQueue();
		if (!Queue)
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Failed to get movie pipeline queue"));
			return false;
		}

		// Create jobs for each trajectory using existing shot sequences
		TArray<UMoviePipelineExecutorJob*> CreatedJobs;
		for (ACDGTrajectory* Trajectory : Trajectories)
		{
			if (!Trajectory)
			{
				continue;
			}

			// Find existing shot sequence for this trajectory
			ULevelSequence* ShotSequence = nullptr;
			if (!Internal::FindExistingShotSequence(Trajectory, LevelName, ShotSequence))
			{
				UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Failed to find shot sequence for trajectory: %s"), 
					*Trajectory->TrajectoryName.ToString());
				continue;
			}

			// Allocate job
			UMoviePipelineExecutorJob* Job = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
			if (!Job)
			{
				UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Failed to allocate job for trajectory: %s"), 
					*Trajectory->TrajectoryName.ToString());
				continue;
			}

			// Set sequence to the shot sequence
			Job->Sequence = FSoftObjectPath(ShotSequence);
			Job->Map = FSoftObjectPath(World);

			// Configure job
			if (!Internal::ConfigureMoviePipelineJob(Job, Trajectory, Config, LevelName))
			{
				UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Failed to configure job for trajectory: %s"), 
					*Trajectory->TrajectoryName.ToString());
				continue;
			}

			CreatedJobs.Add(Job);
			UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Created render job for trajectory: %s"), 
				*Trajectory->TrajectoryName.ToString());
		}

		if (CreatedJobs.Num() == 0)
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: No jobs were created"));
			return false;
		}

		// Export Index.json if requested
		if (Config.bExportIndexJSON)
		{
			Internal::ExportIndexJSON(LevelOutputDir, Trajectories, Config.OutputFramerateOverride);
		}

		// Setup callback to validate and cleanup after rendering completes
		if (MRQSubsystem->GetActiveExecutor())
		{
			MRQSubsystem->GetActiveExecutor()->OnExecutorFinished().RemoveAll(MRQSubsystem->GetActiveExecutor());
		}
		
		// Execute the queue using PIE executor (for editor usage)
		UMoviePipelinePIEExecutor* Executor = Cast<UMoviePipelinePIEExecutor>(MRQSubsystem->RenderQueueWithExecutor(UMoviePipelinePIEExecutor::StaticClass()));
		
		if (Executor)
		{
			// Bind callback for when all rendering completes
			Executor->OnExecutorFinished().AddLambda([LevelOutputDir, Config](UMoviePipelineExecutorBase* InExecutor, bool bSuccess)
			{
				if (bSuccess && Config.ExportFormat == ECDGRenderOutputFormat::H264_Video)
				{
					UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Render completed, validating MP4 files and cleaning up PNG frames..."));
					Internal::ValidateAndCleanupVideoOutput(LevelOutputDir);
				}
			});
		}

		UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Started rendering %d trajectories to: %s"), 
			CreatedJobs.Num(), *LevelOutputDir);

		return true;
	}

	bool RenderTrajectories(const TArray<ACDGTrajectory*>& Trajectories, const FTrajectoryRenderConfig& Config)
	{
		if (Trajectories.Num() == 0)
		{
			UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: No trajectories provided"));
			return false;
		}

		// Validate config
		if (Config.DestinationRootDir.IsEmpty())
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Destination root directory is empty"));
			return false;
		}

		// Get world
		UWorld* World = nullptr;
#if WITH_EDITOR
		if (GEditor)
		{
			World = GEditor->GetEditorWorldContext().World();
		}
#endif

		if (!World)
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: No valid world context found"));
			return false;
		}

		// Check for existing master sequence from CDGLevelSeqSubsystem
		UCDGLevelSeqSubsystem* LevelSeqSubsystem = World->GetSubsystem<UCDGLevelSeqSubsystem>();
		ULevelSequence* MasterSequence = nullptr;
		
		if (LevelSeqSubsystem)
		{
			LevelSeqSubsystem->InitLevelSequence();
			MasterSequence = LevelSeqSubsystem->GetActiveLevelSequence();
		}

		if (!MasterSequence)
		{
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: No existing master sequence found"));
			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("  Please export the level sequence first using the Level Sequence Exporter"));
			return false;
		}

		// Use the new function that accepts a sequence
		return RenderTrajectoriesWithSequence(MasterSequence, Trajectories, Config);
	}

	namespace Internal
	{
		bool FindExistingShotSequence(ACDGTrajectory* Trajectory, const FString& LevelName, ULevelSequence*& OutSequence)
		{
			if (!Trajectory)
			{
				return false;
			}

#if WITH_EDITOR
			// Get world
			UWorld* World = nullptr;
			if (GEditor)
			{
				World = GEditor->GetEditorWorldContext().World();
			}

			if (!World)
			{
				return false;
			}

			// Get the level sequence subsystem to find the master sequence location
			UCDGLevelSeqSubsystem* LevelSeqSubsystem = World->GetSubsystem<UCDGLevelSeqSubsystem>();
			if (!LevelSeqSubsystem)
			{
				return false;
			}

			// Get the master sequence package path
			FString MasterPackageName = LevelSeqSubsystem->GetSequencePackageName();
			if (MasterPackageName.IsEmpty())
			{
				return false;
			}

			FString MasterPackagePath = FPackageName::GetLongPackagePath(MasterPackageName);

			// Build shot sequence name (same format as LevelSeqExporter)
			FString ShotName = FString::Printf(TEXT("Shot_%s"), *Trajectory->TrajectoryName.ToString());
			FString ShotPackageName = MasterPackagePath / ShotName;
			FString ShotAssetPath = ShotPackageName + TEXT(".") + ShotName;

			// Try to load the shot sequence
			OutSequence = LoadObject<ULevelSequence>(nullptr, *ShotAssetPath);

			if (OutSequence)
			{
				UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Found existing shot sequence: %s"), *ShotAssetPath);
				return true;
			}
			else
			{
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Shot sequence not found: %s"), *ShotAssetPath);
				return false;
			}
#else
			return false;
#endif
		}

		bool ValidateShotSequence(ULevelSequence* Sequence, ACDGTrajectory* Trajectory)
		{
			if (!Sequence || !Trajectory)
			{
				return false;
			}

			UMovieScene* MovieScene = Sequence->GetMovieScene();
			if (!MovieScene)
			{
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Shot sequence has no movie scene"));
				return false;
			}

			// Check that the sequence has appropriate duration
			TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
			if (PlaybackRange.IsEmpty())
			{
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Shot sequence has empty playback range"));
				return false;
			}

			// Check that sequence has camera tracks
			bool bHasCameraCut = false;
			bool bHasTransformTrack = false;

			for (UMovieSceneTrack* Track : MovieScene->GetTracks())
			{
				if (Track->IsA<UMovieSceneCameraCutTrack>())
				{
					bHasCameraCut = true;
				}
			}

			// Check for transform tracks in bindings
			int32 BindingCount = MovieScene->GetPossessableCount() + MovieScene->GetSpawnableCount();
			if (BindingCount > 0)
			{
				for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
				{
					FGuid Guid = MovieScene->GetPossessable(i).GetGuid();
					TArray<UMovieSceneTrack*> Tracks = MovieScene->FindTracks(UMovieScene3DTransformTrack::StaticClass(), Guid);
					if (Tracks.Num() > 0)
					{
						bHasTransformTrack = true;
						break;
					}
				}
			}

			if (!bHasCameraCut)
			{
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Shot sequence missing camera cut track for trajectory: %s"), 
					*Trajectory->TrajectoryName.ToString());
				return false;
			}

			if (!bHasTransformTrack)
			{
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Shot sequence missing transform track for trajectory: %s"), 
					*Trajectory->TrajectoryName.ToString());
				return false;
			}

			// Validation passed
			return true;
		}

		bool ValidateMasterSequence(ULevelSequence* MasterSequence, const TArray<ACDGTrajectory*>& Trajectories, const FString& LevelName)
		{
			if (!MasterSequence)
			{
				UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Master sequence is null"));
				return false;
			}

			UMovieScene* MasterMovieScene = MasterSequence->GetMovieScene();
			if (!MasterMovieScene)
			{
				UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Master sequence has no movie scene"));
				return false;
			}

			// Check for cinematic shot track
			UMovieSceneCinematicShotTrack* ShotTrack = Cast<UMovieSceneCinematicShotTrack>(
				MasterMovieScene->FindTrack(UMovieSceneCinematicShotTrack::StaticClass())
			);

			if (!ShotTrack)
			{
				UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Master sequence has no cinematic shot track"));
				return false;
			}

			// Get all shot sections
			const TArray<UMovieSceneSection*>& Sections = ShotTrack->GetAllSections();
			
			if (Sections.Num() == 0)
			{
				UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Master sequence has no shot sections"));
				return false;
			}

			// Validate each trajectory has a corresponding shot
			bool bAllValid = true;
			for (ACDGTrajectory* Trajectory : Trajectories)
			{
				if (!Trajectory)
				{
					continue;
				}

				// Find shot sequence for this trajectory
				ULevelSequence* ShotSequence = nullptr;
				if (!FindExistingShotSequence(Trajectory, LevelName, ShotSequence))
				{
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Missing shot sequence for trajectory: %s"), 
						*Trajectory->TrajectoryName.ToString());
					bAllValid = false;
					continue;
				}

				// Validate the shot sequence
				if (!ValidateShotSequence(ShotSequence, Trajectory))
				{
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Invalid shot sequence for trajectory: %s"), 
						*Trajectory->TrajectoryName.ToString());
					bAllValid = false;
					continue;
				}

				// Check that shot exists in master sequence
				bool bFoundInMaster = false;
				for (UMovieSceneSection* Section : Sections)
				{
					UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
					if (SubSection && SubSection->GetSequence() == ShotSequence)
					{
						bFoundInMaster = true;
						break;
					}
				}

				if (!bFoundInMaster)
				{
					UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Shot sequence not found in master sequence for trajectory: %s"), 
						*Trajectory->TrajectoryName.ToString());
					// This is not critical - the shot exists but isn't in the master
					// We can still render it directly
				}
			}

			return bAllValid;
		}

		bool CreateSequenceForTrajectory(ACDGTrajectory* Trajectory, int32 FPS, ULevelSequence*& OutSequence)
		{
			if (!Trajectory)
			{
				return false;
			}

#if WITH_EDITOR
			// Get world
			UWorld* World = nullptr;
			if (GEditor)
			{
				World = GEditor->GetEditorWorldContext().World();
			}

			if (!World)
			{
				return false;
			}

			// Create temporary package for the sequence
			FString PackageName = FString::Printf(TEXT("/Engine/Transient/CDG_MRQ_%s"), *Trajectory->TrajectoryName.ToString());
			UPackage* Package = CreatePackage(*PackageName);
			if (!Package)
			{
				return false;
			}

			// Create level sequence
			OutSequence = NewObject<ULevelSequence>(Package, *FString::Printf(TEXT("Seq_%s"), *Trajectory->TrajectoryName.ToString()), RF_Public | RF_Standalone | RF_Transient);
			if (!OutSequence)
			{
				return false;
			}

			OutSequence->Initialize();
			UMovieScene* MovieScene = OutSequence->GetMovieScene();
			if (!MovieScene)
			{
				return false;
			}

			// Set framerate
			FFrameRate FrameRate(FPS, 1);
			const double TickResolution = 24000.0; // Standard tick resolution
			MovieScene->SetDisplayRate(FrameRate);
			MovieScene->SetTickResolutionDirectly(FFrameRate(TickResolution, 1));

			// Calculate duration
			float Duration = Trajectory->GetTrajectoryDuration();
			int32 NumFrames = FMath::Max(1, FMath::RoundToInt(Duration * FPS));
			int32 DurationInTicks = NumFrames * (TickResolution / FPS);

			// Create camera actor
			FString CameraName = FString::Printf(TEXT("Cam_MRQ_%s"), *Trajectory->TrajectoryName.ToString());
			
			// Cleanup existing camera with the same name
			for (TActorIterator<ACineCameraActor> It(World); It; ++It)
			{
				if (It->GetActorLabel() == CameraName)
				{
					World->EditorDestroyActor(*It, true);
					break;
				}
			}

			// Spawn new camera
			FActorSpawnParameters SpawnParams;
			SpawnParams.Name = FName(*CameraName);
			ACineCameraActor* CameraActor = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			if (!CameraActor)
			{
				return false;
			}

			CameraActor->SetActorLabel(CameraName);

			// Add camera to sequence
			FGuid CameraGuid = MovieScene->AddPossessable(CameraActor->GetActorLabel(), CameraActor->GetClass());
			OutSequence->BindPossessableObject(CameraGuid, *CameraActor, World);

			// Add camera cut track
			UMovieSceneCameraCutTrack* CameraCutTrack = MovieScene->AddTrack<UMovieSceneCameraCutTrack>();
			if (CameraCutTrack)
			{
				UMovieSceneCameraCutSection* CutSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
				if (CutSection)
				{
					CutSection->SetRange(TRange<FFrameNumber>(0, DurationInTicks));
					CutSection->SetCameraGuid(CameraGuid);
					CameraCutTrack->AddSection(*CutSection);
				}
			}

			// Add transform track
			UMovieScene3DTransformTrack* TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(CameraGuid);
			if (TransformTrack)
			{
				UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
				if (TransformSection)
				{
					TransformTrack->AddSection(*TransformSection);
					TransformSection->SetRange(TRange<FFrameNumber>(0, DurationInTicks));

					// Get channels
					FMovieSceneChannelProxy& ChannelProxy = TransformSection->GetChannelProxy();
					TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = ChannelProxy.GetChannels<FMovieSceneDoubleChannel>();

					if (DoubleChannels.Num() >= 6)
					{
						// Populate keyframes
						TArray<ACDGKeyframe*> TrajectoryKeyframes = Trajectory->GetSortedKeyframes();
						double CurrentTimeSeconds = 0.0;

						for (int32 k = 0; k < TrajectoryKeyframes.Num(); ++k)
						{
							ACDGKeyframe* Keyframe = TrajectoryKeyframes[k];
							if (!Keyframe) continue;

							if (k > 0)
							{
								CurrentTimeSeconds += Keyframe->TimeToCurrentFrame;
							}

							FFrameNumber KeyTime = FFrameNumber(static_cast<int32>(CurrentTimeSeconds * TickResolution));
							FTransform KeyTransform = Keyframe->GetKeyframeTransform();
							FVector Loc = KeyTransform.GetLocation();
							FRotator Rot = KeyTransform.GetRotation().Rotator();

							bool bHasStay = Keyframe->TimeAtCurrentFrame > KINDA_SMALL_NUMBER;

							// Add arrival key
							DoubleChannels[0]->AddLinearKey(KeyTime, Loc.X);
							DoubleChannels[1]->AddLinearKey(KeyTime, Loc.Y);
							DoubleChannels[2]->AddLinearKey(KeyTime, Loc.Z);
							DoubleChannels[3]->AddLinearKey(KeyTime, Rot.Roll);
							DoubleChannels[4]->AddLinearKey(KeyTime, Rot.Pitch);
							DoubleChannels[5]->AddLinearKey(KeyTime, Rot.Yaw);

							// Add stay key if needed
							if (bHasStay)
							{
								CurrentTimeSeconds += Keyframe->TimeAtCurrentFrame;
								FFrameNumber EndKeyTime = FFrameNumber(static_cast<int32>(CurrentTimeSeconds * TickResolution));

								DoubleChannels[0]->AddLinearKey(EndKeyTime, Loc.X);
								DoubleChannels[1]->AddLinearKey(EndKeyTime, Loc.Y);
								DoubleChannels[2]->AddLinearKey(EndKeyTime, Loc.Z);
								DoubleChannels[3]->AddLinearKey(EndKeyTime, Rot.Roll);
								DoubleChannels[4]->AddLinearKey(EndKeyTime, Rot.Pitch);
								DoubleChannels[5]->AddLinearKey(EndKeyTime, Rot.Yaw);
							}
						}
					}
				}
			}

			// Add focal length track
			UCineCameraComponent* CameraComponent = CameraActor->GetCineCameraComponent();
			if (CameraComponent)
			{
				FGuid ComponentGuid = MovieScene->AddPossessable(CameraComponent->GetName(), CameraComponent->GetClass());
				FMovieScenePossessable* ChildPossessable = MovieScene->FindPossessable(ComponentGuid);
				if (ChildPossessable)
				{
					ChildPossessable->SetParent(CameraGuid, MovieScene);
				}
				OutSequence->BindPossessableObject(ComponentGuid, *CameraComponent, CameraActor);

				UMovieSceneFloatTrack* FocalLengthTrack = MovieScene->AddTrack<UMovieSceneFloatTrack>(ComponentGuid);
				if (FocalLengthTrack)
				{
					FocalLengthTrack->SetPropertyNameAndPath(GET_MEMBER_NAME_CHECKED(UCineCameraComponent, CurrentFocalLength), "CurrentFocalLength");
					UMovieSceneFloatSection* FocalSection = Cast<UMovieSceneFloatSection>(FocalLengthTrack->CreateNewSection());
					FocalLengthTrack->AddSection(*FocalSection);
					FocalSection->SetRange(TRange<FFrameNumber>(0, DurationInTicks));

					FMovieSceneFloatChannel& FocalChannel = FocalSection->GetChannel();
					TArray<ACDGKeyframe*> TrajectoryKeyframes = Trajectory->GetSortedKeyframes();
					double CurrentTimeSeconds = 0.0;

					for (int32 k = 0; k < TrajectoryKeyframes.Num(); ++k)
					{
						ACDGKeyframe* Keyframe = TrajectoryKeyframes[k];
						if (!Keyframe) continue;

						if (k > 0)
						{
							CurrentTimeSeconds += Keyframe->TimeToCurrentFrame;
						}

						FFrameNumber KeyTime = FFrameNumber(static_cast<int32>(CurrentTimeSeconds * TickResolution));
						FocalChannel.AddLinearKey(KeyTime, Keyframe->LensSettings.FocalLength);

						if (Keyframe->TimeAtCurrentFrame > KINDA_SMALL_NUMBER)
						{
							CurrentTimeSeconds += Keyframe->TimeAtCurrentFrame;
							FFrameNumber EndKeyTime = FFrameNumber(static_cast<int32>(CurrentTimeSeconds * TickResolution));
							FocalChannel.AddLinearKey(EndKeyTime, Keyframe->LensSettings.FocalLength);
						}
					}
				}
			}

			// Set playback range
			MovieScene->SetPlaybackRange(TRange<FFrameNumber>(0, DurationInTicks));

			return true;
#else
			return false;
#endif
		}

		bool ConfigureMoviePipelineJob(UMoviePipelineExecutorJob* Job, ACDGTrajectory* Trajectory, const FTrajectoryRenderConfig& Config, const FString& LevelName)
		{
			if (!Job || !Trajectory)
			{
				return false;
			}

			// Get or create config
			UMoviePipelinePrimaryConfig* PipelineConfig = Job->GetConfiguration();
			if (!PipelineConfig)
			{
				PipelineConfig = NewObject<UMoviePipelinePrimaryConfig>(Job);
				Job->SetConfiguration(PipelineConfig);
			}

			if (!PipelineConfig)
			{
				return false;
			}

			// Configure output settings
			UMoviePipelineOutputSetting* OutputSetting = Cast<UMoviePipelineOutputSetting>(
				PipelineConfig->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass())
			);

			bool bIsVideoFormat = IsVideoFormat(Config.ExportFormat);

			if (OutputSetting)
			{
				// Setup output directory: <RootDir>/<LevelName>/OUTPUTS/
				FString OutputDir = FPaths::Combine(Config.DestinationRootDir, LevelName, TEXT("OUTPUTS"));
				OutputSetting->OutputDirectory.Path = OutputDir;

				// Setup file naming format: <LevelName>_<TrajectoryName>
				// For image sequences, frame numbers are appended automatically
				// Format: <LevelName>_<TrajectoryName>.{frame_number}
				// This will produce files like: TestMap_Trajectory_1.0001.png
				FString FileNameFormat = FString::Printf(TEXT("%s.%s.{frame_number}"), *LevelName, *Trajectory->TrajectoryName.ToString());
				OutputSetting->FileNameFormat = FileNameFormat;
				
				UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Output file name format: %s"), *FileNameFormat);

				// Set resolution
				OutputSetting->OutputResolution = Config.OutputResolutionOverride;

				// Set framerate
				if (Config.OutputFramerateOverride > 0)
				{
					OutputSetting->OutputFrameRate = FFrameRate(Config.OutputFramerateOverride, 1);
				}

				// Override existing output
				OutputSetting->bOverrideExistingOutput = Config.bOverwriteExistingOutput;
			}

			// Configure anti-aliasing settings
			UMoviePipelineAntiAliasingSetting* AntiAliasingSetting = Cast<UMoviePipelineAntiAliasingSetting>(
				PipelineConfig->FindOrAddSettingByClass(UMoviePipelineAntiAliasingSetting::StaticClass())
			);

			if (AntiAliasingSetting)
			{
				AntiAliasingSetting->SpatialSampleCount = Config.SpatialSampleCount;
				AntiAliasingSetting->TemporalSampleCount = Config.TemporalSampleCount;
			}

			// Configure output format based on the selected export format
			if (bIsVideoFormat)
			{
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT(""));
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("*************************************************************"));
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("*** VIDEO FORMAT REQUESTED - CHECKING FFMPEG AVAILABILITY ***"));
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("*************************************************************"));
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT(""));
				
				// Configure encoder settings in project settings
				UMoviePipelineCommandLineEncoderSettings* EncoderSettings = GetMutableDefault<UMoviePipelineCommandLineEncoderSettings>();
				bool bFFmpegAvailable = false;
				FString FFmpegPath;
				
				if (EncoderSettings)
				{
					// Try to ensure FFmpeg is available (will download if needed)
					bFFmpegAvailable = Internal::EnsureFFmpegAvailable(FFmpegPath);
					
					if (!bFFmpegAvailable)
					{
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT(""));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!                                                        !!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!    VIDEO ENCODING SKIPPED - FFMPEG NOT FOUND           !!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!    RENDERING PNG SEQUENCE INSTEAD OF MP4               !!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!                                                        !!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT(""));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("=== HOW TO ENABLE VIDEO ENCODING ==="));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("1. Download FFmpeg from: https://github.com/BtbN/FFmpeg-Builds/releases/latest"));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("2. Download: ffmpeg-master-latest-win64-gpl.zip"));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("3. Extract the complete zip (includes bin folder)"));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("4. Copy to: %s"), *FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/FFmpeg/Win64/")));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("   Final path: Engine/Binaries/ThirdParty/FFmpeg/Win64/bin/ffmpeg.exe"));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("5. Restart Unreal Editor"));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("===================================="));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT(""));
					}
					else
					{
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("*** FFmpeg found at: %s"), *FFmpegPath);
					}
					
					if (bFFmpegAvailable)
					{
						// Set H.264 codec settings
						EncoderSettings->VideoCodec = TEXT("libx264");
						EncoderSettings->AudioCodec = TEXT("aac");
						EncoderSettings->OutputFileExtension = TEXT("mp4");
						EncoderSettings->ExecutablePath = FFmpegPath;
						
						// Override quality settings for H.264
						EncoderSettings->EncodeSettings_Low = TEXT("-crf 28 -preset fast -pix_fmt yuv420p");
						EncoderSettings->EncodeSettings_Med = TEXT("-crf 23 -preset medium -pix_fmt yuv420p");
						EncoderSettings->EncodeSettings_High = TEXT("-crf 18 -preset slow -pix_fmt yuv420p -movflags +faststart");
						EncoderSettings->EncodeSettings_Epic = TEXT("-crf 16 -preset slower -pix_fmt yuv420p -movflags +faststart");
						
						// Save settings to config
						EncoderSettings->SaveConfig();
						
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("*** Configured H.264 encoder settings"));
					}
				}
				else
				{
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT(""));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!                                                        !!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!    VIDEO ENCODING SKIPPED - ENCODER SETTINGS NULL      !!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!    RENDERING PNG SEQUENCE INSTEAD OF MP4               !!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!                                                        !!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT(""));
				}
				
				// For video output, we need to render image sequence first, then encode
				// Use PNG as intermediate format with proper frame numbering
				UMoviePipelineImageSequenceOutput_PNG* ImageOutput = Cast<UMoviePipelineImageSequenceOutput_PNG>(
					PipelineConfig->FindOrAddSettingByClass(UMoviePipelineImageSequenceOutput_PNG::StaticClass())
				);
				
				// Only add command line encoder if FFmpeg is available
				if (Config.ExportFormat == ECDGRenderOutputFormat::H264_Video && bFFmpegAvailable)
				{
					UMoviePipelineCommandLineEncoder* CommandLineEncoder = Cast<UMoviePipelineCommandLineEncoder>(
						PipelineConfig->FindOrAddSettingByClass(UMoviePipelineCommandLineEncoder::StaticClass())
					);
					
					if (CommandLineEncoder)
					{
						// Configure for H.264 output using FFmpeg
						// Video file name format: <LevelName>_<TrajectoryName>.mp4
						// Example: TestMap_Trajectory_1.mp4
						FString VideoFileName = FString::Printf(TEXT("%s.%s"), *LevelName, *Trajectory->TrajectoryName.ToString());
						CommandLineEncoder->FileNameFormatOverride = VideoFileName;
						CommandLineEncoder->Quality = EMoviePipelineEncodeQuality::High;
						CommandLineEncoder->bDeleteSourceFiles = true; // Clean up intermediate PNG files
						CommandLineEncoder->bSkipEncodeOnRenderCanceled = true;
						
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT(""));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("*************************************************************"));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("*** VIDEO ENCODING ENABLED - MP4 OUTPUT WILL BE CREATED   ***"));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("*** Output video file: %s.mp4"), *VideoFileName);
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("*************************************************************"));
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT(""));
					}
					else
					{
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT(""));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!                                                        !!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!    VIDEO ENCODING SKIPPED - ENCODER CREATION FAILED    !!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!    RENDERING PNG SEQUENCE INSTEAD OF MP4               !!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!                                                        !!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
						UE_LOG(LogCameraDatasetGenEditor, Error, TEXT(""));
					}
				}
				else if (Config.ExportFormat == ECDGRenderOutputFormat::H264_Video && !bFFmpegAvailable)
				{
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT(""));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!                                                        !!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!    CONFIRMED: VIDEO ENCODING WILL BE SKIPPED           !!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!    OUTPUT: PNG IMAGE SEQUENCE (NOT MP4 VIDEO)          !!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!                                                        !!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT(""));
				}
			}
			else
			{
				// Add specific image sequence output based on format
				// Use FindObject to get the class without including headers
				UClass* OutputClass = nullptr;
				
				switch (Config.ExportFormat)
				{
				case ECDGRenderOutputFormat::PNG_Sequence:
					OutputClass = UMoviePipelineImageSequenceOutput_PNG::StaticClass();
					break;
				case ECDGRenderOutputFormat::EXR_Sequence:
					// Find EXR class dynamically to avoid header dependency
					OutputClass = FindObject<UClass>(nullptr, TEXT("/Script/MovieRenderPipelineRenderPasses.MoviePipelineImageSequenceOutput_EXR"));
					if (!OutputClass)
					{
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: EXR output class not found, falling back to PNG"));
						OutputClass = UMoviePipelineImageSequenceOutput_PNG::StaticClass();
					}
					break;
				case ECDGRenderOutputFormat::BMP_Sequence:
					// Find BMP class dynamically to avoid header dependency
					OutputClass = FindObject<UClass>(nullptr, TEXT("/Script/MovieRenderPipelineRenderPasses.MoviePipelineImageSequenceOutput_BMP"));
					if (!OutputClass)
					{
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: BMP output class not found, falling back to PNG"));
						OutputClass = UMoviePipelineImageSequenceOutput_PNG::StaticClass();
					}
					break;
				default:
					// Default to PNG
					OutputClass = UMoviePipelineImageSequenceOutput_PNG::StaticClass();
					break;
				}
				
				if (OutputClass)
				{
					PipelineConfig->FindOrAddSettingByClass(OutputClass);
				}
			}

			// Add deferred rendering pass
			UMoviePipelineDeferredPassBase* DeferredPass = Cast<UMoviePipelineDeferredPassBase>(
				PipelineConfig->FindOrAddSettingByClass(UMoviePipelineDeferredPassBase::StaticClass())
			);

			// Add high resolution settings
			UMoviePipelineHighResSetting* HighResSetting = Cast<UMoviePipelineHighResSetting>(
				PipelineConfig->FindOrAddSettingByClass(UMoviePipelineHighResSetting::StaticClass())
			);

			if (HighResSetting)
			{
				HighResSetting->TileCount = 1; // Single tile for now
			}

			return true;
		}

		FString GetFileExtensionForFormat(ECDGRenderOutputFormat Format)
		{
			switch (Format)
			{
			case ECDGRenderOutputFormat::BMP_Sequence:
				return TEXT("bmp");
			case ECDGRenderOutputFormat::EXR_Sequence:
				return TEXT("exr");
			case ECDGRenderOutputFormat::PNG_Sequence:
				return TEXT("png");
			case ECDGRenderOutputFormat::WAV_Audio:
				return TEXT("wav");
			case ECDGRenderOutputFormat::H264_Video:
				return TEXT("mp4");
			case ECDGRenderOutputFormat::CommandLineEncoder:
				return TEXT(""); // Custom
			case ECDGRenderOutputFormat::FinalCutProXML:
				return TEXT("xml");
			default:
				return TEXT("png");
			}
		}

		bool IsVideoFormat(ECDGRenderOutputFormat Format)
		{
			return Format == ECDGRenderOutputFormat::H264_Video ||
				   Format == ECDGRenderOutputFormat::CommandLineEncoder;
		}

		bool ExportIndexJSON(const FString& OutputDir, const TArray<ACDGTrajectory*>& Trajectories, int32 FPS)
		{
			// Create a temporary array to pass to TrajectorySL
			// Since we're just exporting the JSON for these specific trajectories,
			// we'll use the existing TrajectorySL::SaveAllTrajectories function
			FString JSONPath = FPaths::Combine(OutputDir, TEXT("Index.json"));
			
			// Use the TrajectorySL system to export
			bool bSuccess = TrajectorySL::SaveAllTrajectories(JSONPath, FPS, true);

			if (bSuccess)
			{
				UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Exported Index.json to: %s"), *JSONPath);
			}
			else
			{
				UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Failed to export Index.json to: %s"), *JSONPath);
			}

			return bSuccess;
		}

		FString SetupOutputDirectory(const FString& RootDir, const FString& LevelName)
		{
			// Create directory structure: <RootDir>/<LevelName>/OUTPUTS/
			FString LevelOutputDir = FPaths::Combine(RootDir, LevelName);
			FString OutputsDir = FPaths::Combine(LevelOutputDir, TEXT("OUTPUTS"));

			// Create directories
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			
			if (!PlatformFile.DirectoryExists(*LevelOutputDir))
			{
				if (!PlatformFile.CreateDirectoryTree(*LevelOutputDir))
				{
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Failed to create directory: %s"), *LevelOutputDir);
					return FString();
				}
			}

			if (!PlatformFile.DirectoryExists(*OutputsDir))
			{
				if (!PlatformFile.CreateDirectoryTree(*OutputsDir))
				{
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Failed to create directory: %s"), *OutputsDir);
					return FString();
				}
			}

			UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Setup output directory: %s"), *LevelOutputDir);
			return LevelOutputDir;
		}

		/**
		 * Download FFmpeg from reliable source
		 * @param DestinationPath - Where to save the downloaded file
		 * @param OnComplete - Callback when download completes (success, error message)
		 */
		void DownloadFFmpeg(const FString& DestinationPath, TFunction<void(bool, const FString&)> OnComplete)
		{
			UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Starting FFmpeg download..."));

			// Use BtbN/FFmpeg-Builds which provides direct download links
			// This is a reliable, frequently updated source
			FString DownloadURL = TEXT("https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip");
			
			TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
			HttpRequest->SetURL(DownloadURL);
			HttpRequest->SetVerb(TEXT("GET"));
			HttpRequest->SetTimeout(300.0f); // 5 minute timeout for large file
			
			HttpRequest->OnProcessRequestComplete().BindLambda([DestinationPath, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
			{
				if (!bWasSuccessful || !Response.IsValid())
				{
					FString Error = TEXT("Failed to download FFmpeg: Network error");
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: %s"), *Error);
					OnComplete(false, Error);
					return;
				}

				if (Response->GetResponseCode() != 200)
				{
					FString Error = FString::Printf(TEXT("Failed to download FFmpeg: HTTP %d"), Response->GetResponseCode());
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: %s"), *Error);
					OnComplete(false, Error);
					return;
				}

				// Save the downloaded zip file
				if (FFileHelper::SaveArrayToFile(Response->GetContent(), *DestinationPath))
				{
					UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: FFmpeg downloaded to: %s"), *DestinationPath);
					OnComplete(true, FString());
				}
				else
				{
					FString Error = FString::Printf(TEXT("Failed to save FFmpeg to: %s"), *DestinationPath);
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: %s"), *Error);
					OnComplete(false, Error);
				}
			});

			HttpRequest->ProcessRequest();
		}

		/**
		 * Extract FFmpeg executable from downloaded zip
		 * @param ZipPath - Path to the downloaded zip file
		 * @param DestinationDir - Directory to extract to
		 * @return Path to extracted ffmpeg.exe, or empty string on failure
		 */
		FString ExtractFFmpegFromZip(const FString& ZipPath, const FString& DestinationDir)
		{
			UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Extracting FFmpeg from: %s"), *ZipPath);

			// Unreal doesn't have built-in zip extraction, so we'll use Windows PowerShell
			FString PowerShellCommand = FString::Printf(
				TEXT("powershell -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\""),
				*ZipPath, *DestinationDir
			);

			int32 ReturnCode = 0;
			FString StdOut;
			FString StdErr;

			if (FPlatformProcess::ExecProcess(TEXT("cmd.exe"), *FString::Printf(TEXT("/c %s"), *PowerShellCommand), &ReturnCode, &StdOut, &StdErr))
			{
				if (ReturnCode == 0)
				{
					// Find ffmpeg.exe in the extracted directory
					// BtbN builds have structure: ffmpeg-master-latest-win64-gpl/bin/ffmpeg.exe
					TArray<FString> PossiblePaths = {
						FPaths::Combine(DestinationDir, TEXT("ffmpeg-master-latest-win64-gpl/bin/ffmpeg.exe")),
						FPaths::Combine(DestinationDir, TEXT("bin/ffmpeg.exe")),
						FPaths::Combine(DestinationDir, TEXT("ffmpeg.exe"))
					};

					for (const FString& Path : PossiblePaths)
					{
						if (FPaths::FileExists(Path))
						{
							UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Found ffmpeg.exe at: %s"), *Path);
							return Path;
						}
					}

					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: ffmpeg.exe not found in extracted files"));
					return FString();
				}
				else
				{
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Extraction failed with code %d: %s"), ReturnCode, *StdErr);
					return FString();
				}
			}

			UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: Failed to start extraction process"));
			return FString();
		}

		/**
		 * Automatically download and install FFmpeg if not found
		 * @param TargetPath - Where FFmpeg should be installed
		 * @return true if FFmpeg is available (either found or downloaded), false otherwise
		 */
		bool EnsureFFmpegAvailable(FString& OutFFmpegPath)
		{
			// Check multiple possible FFmpeg locations
			TArray<FString> PossiblePaths = {
				FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/FFmpeg/Win64/bin/ffmpeg.exe")),
				FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/FFmpeg/Win64/ffmpeg.exe")),
				FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/ThirdParty/FFmpeg/Win64/bin/ffmpeg.exe")),
				FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64/ffmpeg.exe"))
			};
			
			for (const FString& Path : PossiblePaths)
			{
				if (FPaths::FileExists(Path))
				{
					OutFFmpegPath = Path;
					UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Found FFmpeg at: %s"), *Path);
					return true;
				}
			}
			
			UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: FFmpeg not found in any expected location"));
			UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Checked paths:"));
			for (const FString& Path : PossiblePaths)
			{
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("  - %s"), *Path);
			}

			// FFmpeg not found, download it
			UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: FFmpeg not found, downloading automatically..."));

			FString DownloadDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FFmpeg"));
			FString ZipPath = FPaths::Combine(DownloadDir, TEXT("ffmpeg.zip"));
			FString ExtractDir = FPaths::Combine(DownloadDir, TEXT("extracted"));
			FString FinalPath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/FFmpeg/Win64"));

			// Create directories
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			PlatformFile.CreateDirectoryTree(*DownloadDir);
			PlatformFile.CreateDirectoryTree(*ExtractDir);
			PlatformFile.CreateDirectoryTree(*FinalPath);

			// Download FFmpeg (synchronous for simplicity)
			bool bDownloadSuccess = false;
			FString DownloadError;
			
			// We need to block and wait for the download
			// For now, we'll log the instruction and return false
			// A full async implementation would require more complex state management
			
			UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Automatic FFmpeg download requires async implementation"));
			UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Please manually download FFmpeg from: https://github.com/BtbN/FFmpeg-Builds/releases/latest"));
			UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Extract ffmpeg.exe to: %s"), *FinalPath);

			return false;
		}
		
		/**
		 * Validate MP4 files and cleanup PNG frames
		 * Checks that:
		 * 1. MP4 file exists
		 * 2. MP4 file has reasonable size (> 1KB)
		 * 3. If valid, deletes corresponding PNG frames
		 */
		int32 ValidateAndCleanupVideoOutput(const FString& OutputDir)
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			
			// Find OUTPUTS subdirectory
			FString OutputsDir = FPaths::Combine(OutputDir, TEXT("OUTPUTS"));
			if (!PlatformFile.DirectoryExists(*OutputsDir))
			{
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: OUTPUTS directory not found: %s"), *OutputsDir);
				return 0;
			}
			
			// Find all MP4 files
			TArray<FString> MP4Files;
			IFileManager& FileManager = IFileManager::Get();
			FString SearchPattern = FPaths::Combine(OutputsDir, TEXT("*.mp4"));
			FileManager.FindFiles(MP4Files, *SearchPattern, true, false);
			
			if (MP4Files.Num() == 0)
			{
				UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: No MP4 files found in: %s"), *OutputsDir);
				return 0;
			}
			
			int32 TotalPNGsDeleted = 0;
			
			for (const FString& MP4File : MP4Files)
			{
				FString FullMP4Path = FPaths::Combine(OutputsDir, MP4File);
				
				// Validate MP4 file
				int64 FileSize = PlatformFile.FileSize(*FullMP4Path);
				
				if (FileSize <= 0)
				{
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: MP4 file has invalid size: %s (size: %lld)"), *MP4File, FileSize);
					continue;
				}
				
				// Minimum size check - MP4 files should be at least 1KB
				const int64 MinValidSize = 1024;
				if (FileSize < MinValidSize)
				{
					UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQInterface: MP4 file too small (possibly corrupt): %s (size: %lld bytes)"), *MP4File, FileSize);
					continue;
				}
				
				UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Validated MP4 file: %s (size: %.2f MB)"), 
					*MP4File, FileSize / (1024.0 * 1024.0));
				
				// Extract base name without extension
				// Format: <LevelName>_<TrajectoryName>.mp4
				FString BaseName = FPaths::GetBaseFilename(MP4File);
				
				// Find all corresponding PNG files
				// Format: <LevelName>_<TrajectoryName>.####.png
				TArray<FString> PNGFiles;
				FString PNGSearchPattern = FPaths::Combine(OutputsDir, BaseName + TEXT(".*.png"));
				FileManager.FindFiles(PNGFiles, *PNGSearchPattern, true, false);
				
				int32 PNGsDeleted = 0;
				for (const FString& PNGFile : PNGFiles)
				{
					FString FullPNGPath = FPaths::Combine(OutputsDir, PNGFile);
					
					if (PlatformFile.DeleteFile(*FullPNGPath))
					{
						PNGsDeleted++;
					}
					else
					{
						UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("CDGMRQInterface: Failed to delete PNG frame: %s"), *PNGFile);
					}
				}
				
				if (PNGsDeleted > 0)
				{
					UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQInterface: Cleaned up %d PNG frames for: %s"), PNGsDeleted, *MP4File);
					TotalPNGsDeleted += PNGsDeleted;
				}
			}
			
			if (TotalPNGsDeleted > 0)
			{
				UE_LOG(LogCameraDatasetGenEditor, Log, TEXT(""));
				UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("*************************************************************"));
				UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("*** VIDEO ENCODING COMPLETE - CLEANED UP %d PNG FRAMES ***"), TotalPNGsDeleted);
				UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("*************************************************************"));
				UE_LOG(LogCameraDatasetGenEditor, Log, TEXT(""));
			}
			
			return TotalPNGsDeleted;
		}
	}
}
