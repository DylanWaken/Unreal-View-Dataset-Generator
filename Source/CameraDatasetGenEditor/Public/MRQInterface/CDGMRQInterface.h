// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CDGMRQInterface.generated.h"

class ACDGTrajectory;
class ULevelSequence;
class UMoviePipelineQueue;
class UMoviePipelineExecutorJob;

/**
 * Output format options for trajectory rendering
 */
UENUM(BlueprintType)
enum class ECDGRenderOutputFormat : uint8
{
	/** BMP Sequence [8bit] */
	BMP_Sequence UMETA(DisplayName = "BMP Sequence [8bit]"),
	
	/** EXR Sequence [16bit] */
	EXR_Sequence UMETA(DisplayName = "EXR Sequence [16bit]"),
	
	/** PNG Sequence [8bit] */
	PNG_Sequence UMETA(DisplayName = "PNG Sequence [8bit]"),
	
	/** WAV Audio */
	WAV_Audio UMETA(DisplayName = "WAV Audio"),
	
	/** H.264 MP4 Video [8bit] */
	H264_Video UMETA(DisplayName = "H.264 MP4 [8bit]"),
	
	/** Command Line Encoder */
	CommandLineEncoder UMETA(DisplayName = "Command Line Encoder"),
	
	/** Final Cut Pro XML */
	FinalCutProXML UMETA(DisplayName = "Final Cut Pro XML")
};

/**
 * Configuration for rendering trajectories via Movie Render Queue
 */
USTRUCT(BlueprintType)
struct FTrajectoryRenderConfig
{
	GENERATED_BODY()

	/** Root directory where all rendered output will be saved */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	FString DestinationRootDir;

	/** Override output resolution (leave at 0,0 to use sequence default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	FIntPoint OutputResolutionOverride = FIntPoint(1920, 1080);

	/** Override output framerate (leave at 0 to use sequence default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	int32 OutputFramerateOverride = 30;

	/** Export format for rendered output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	ECDGRenderOutputFormat ExportFormat = ECDGRenderOutputFormat::PNG_Sequence;

	/** Whether to include Index.json file with trajectory data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bExportIndexJSON = true;

	/** Whether to overwrite existing output files */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bOverwriteExistingOutput = false;

	/** Anti-aliasing settings (spatial sample count) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	int32 SpatialSampleCount = 1;

	/** Temporal sample count for motion blur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	int32 TemporalSampleCount = 1;
};

/**
 * Movie Render Queue Interface for CDG System
 * 
 * Provides high-level API for rendering trajectories using Unreal's Movie Render Queue.
 * Handles automatic configuration, directory structure, and file management.
 */
namespace CDGMRQInterface
{
	/**
	 * Render all trajectories in the current world using Movie Render Queue
	 * 
	 * Output Structure:
	 *   <DestinationRootDir>/
	 *     <LevelName>/
	 *       OUTPUTS/
	 *         <LevelName>_<TrajectoryName>.<FrameNumber>.<ImageFormat>
	 *         Example: TestMap_Trajectory_1.0001.png
	 *         or
	 *         <LevelName>_<TrajectoryName>.<VideoFormat>
	 *         Example: TestMap_Trajectory_1.mp4
	 *       Index.json
	 * 
	 * @param Config - Rendering configuration settings
	 * @return true if rendering was initiated successfully, false otherwise
	 */
	CAMERADATASETGENEDITOR_API bool RenderTrajectories(const FTrajectoryRenderConfig& Config);

	/**
	 * Render specific trajectories using Movie Render Queue
	 * 
	 * @param Trajectories - Array of trajectories to render
	 * @param Config - Rendering configuration settings
	 * @return true if rendering was initiated successfully, false otherwise
	 */
	CAMERADATASETGENEDITOR_API bool RenderTrajectories(const TArray<ACDGTrajectory*>& Trajectories, const FTrajectoryRenderConfig& Config);

	/**
	 * Render trajectories using a specified master level sequence
	 * 
	 * @param MasterSequence - The master level sequence to use (must contain shot sequences for trajectories)
	 * @param Trajectories - Array of trajectories to render
	 * @param Config - Rendering configuration settings
	 * @return true if rendering was initiated successfully, false otherwise
	 */
	CAMERADATASETGENEDITOR_API bool RenderTrajectoriesWithSequence(ULevelSequence* MasterSequence, const TArray<ACDGTrajectory*>& Trajectories, const FTrajectoryRenderConfig& Config);

	// Helper functions
	namespace Internal
	{
		/**
		 * Find existing shot sequence for a trajectory
		 * @param Trajectory - The trajectory to find sequence for
		 * @param LevelName - Name of the level
		 * @param OutSequence - The found sequence
		 * @return true if found
		 */
		bool FindExistingShotSequence(ACDGTrajectory* Trajectory, const FString& LevelName, ULevelSequence*& OutSequence);

		/**
		 * Validate that a shot sequence matches the trajectory
		 * @param Sequence - The sequence to validate
		 * @param Trajectory - The trajectory to validate against
		 * @return true if valid
		 */
		bool ValidateShotSequence(ULevelSequence* Sequence, ACDGTrajectory* Trajectory);

		/**
		 * Validate master sequence matches trajectory list
		 * @param MasterSequence - The master sequence to validate
		 * @param Trajectories - Array of trajectories to validate against
		 * @param LevelName - Name of the level
		 * @return true if all trajectories have valid matching shots
		 */
		bool ValidateMasterSequence(ULevelSequence* MasterSequence, const TArray<ACDGTrajectory*>& Trajectories, const FString& LevelName);

		/**
		 * Create a level sequence for a trajectory
		 * @param Trajectory - The trajectory to create a sequence for
		 * @param FPS - Frames per second
		 * @param OutSequence - The created sequence
		 * @return true if successful
		 */
		bool CreateSequenceForTrajectory(ACDGTrajectory* Trajectory, int32 FPS, ULevelSequence*& OutSequence);

		/**
		 * Configure a movie pipeline job for rendering
		 * @param Job - The job to configure
		 * @param Trajectory - The trajectory being rendered
		 * @param Config - Rendering configuration
		 * @param LevelName - Name of the current level
		 * @return true if successful
		 */
		bool ConfigureMoviePipelineJob(UMoviePipelineExecutorJob* Job, ACDGTrajectory* Trajectory, const FTrajectoryRenderConfig& Config, const FString& LevelName);

		/**
		 * Get file extension for the specified output format
		 * @param Format - The output format
		 * @return File extension (e.g., "png", "mp4")
		 */
		FString GetFileExtensionForFormat(ECDGRenderOutputFormat Format);

		/**
		 * Check if the format is a video format (vs image sequence)
		 * @param Format - The output format
		 * @return true if video format
		 */
		bool IsVideoFormat(ECDGRenderOutputFormat Format);

		/**
		 * Export Index.json for rendered trajectories
		 * @param OutputDir - Directory where Index.json will be saved
		 * @param Trajectories - Array of trajectories to export
		 * @param FPS - Frames per second
		 * @return true if successful
		 */
		bool ExportIndexJSON(const FString& OutputDir, const TArray<ACDGTrajectory*>& Trajectories, int32 FPS);

		/**
		 * Setup output directory structure
		 * @param RootDir - Root output directory
		 * @param LevelName - Name of the current level
		 * @return Path to the level output directory
		 */
		FString SetupOutputDirectory(const FString& RootDir, const FString& LevelName);

		/**
		 * Ensure FFmpeg is available (download if necessary)
		 * @param OutFFmpegPath - Path to FFmpeg executable if found/downloaded
		 * @return true if FFmpeg is available, false otherwise
		 */
		bool EnsureFFmpegAvailable(FString& OutFFmpegPath);
		
		/**
		 * Validate that MP4 files were created successfully and clean up PNG frames
		 * @param OutputDir - Directory containing rendered output
		 * @return Number of PNG files cleaned up
		 */
		int32 ValidateAndCleanupVideoOutput(const FString& OutputDir);
	}
}
