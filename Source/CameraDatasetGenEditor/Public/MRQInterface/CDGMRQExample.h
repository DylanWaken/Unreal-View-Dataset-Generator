// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Example usage of CDGMRQInterface
 * 
 * This header provides example code for using the MRQ rendering interface.
 * 
 * Basic Usage:
 * 
 * ```cpp
 * #include "MRQInterface/CDGMRQInterface.h"
 * 
 * // Setup configuration
 * FTrajectoryRenderConfig Config;
 * Config.DestinationRootDir = TEXT("D:/Renders");
 * Config.OutputResolutionOverride = FIntPoint(1920, 1080);
 * Config.OutputFramerateOverride = 30;
 * Config.ExportFormat = ECDGRenderOutputFormat::PNG_Sequence;
 * Config.bExportIndexJSON = true;
 * Config.SpatialSampleCount = 4;  // Anti-aliasing
 * Config.TemporalSampleCount = 8; // Motion blur
 * 
 * // Render all trajectories in the world
 * bool bSuccess = CDGMRQInterface::RenderTrajectories(Config);
 * 
 * if (bSuccess)
 * {
 *     UE_LOG(LogTemp, Log, TEXT("Rendering started successfully"));
 * }
 * ```
 * 
 * Advanced Usage (Render specific trajectories):
 * 
 * ```cpp
 * #include "MRQInterface/CDGMRQInterface.h"
 * #include "Trajectory/CDGTrajectory.h"
 * #include "EngineUtils.h"
 * 
 * // Get specific trajectories
 * TArray<ACDGTrajectory*> TrajectoriesToRender;
 * for (TActorIterator<ACDGTrajectory> It(GetWorld()); It; ++It)
 * {
 *     if (It->TrajectoryName.ToString().Contains(TEXT("Selected")))
 *     {
 *         TrajectoriesToRender.Add(*It);
 *     }
 * }
 * 
 * // Setup configuration
 * FTrajectoryRenderConfig Config;
 * Config.DestinationRootDir = TEXT("D:/Renders");
 * Config.OutputResolutionOverride = FIntPoint(3840, 2160); // 4K
 * Config.OutputFramerateOverride = 60;
 * Config.ExportFormat = ECDGRenderOutputFormat::EXR_Sequence; // High dynamic range
 * Config.bExportIndexJSON = true;
 * Config.bOverwriteExistingOutput = true;
 * Config.SpatialSampleCount = 8;
 * Config.TemporalSampleCount = 16;
 * 
 * // Render selected trajectories
 * bool bSuccess = CDGMRQInterface::RenderTrajectories(TrajectoriesToRender, Config);
 * ```
 * 
 * Output Structure:
 * 
 * The rendered output will be organized as follows:
 * 
 * ```
 * D:/Renders/
 *   <LevelName>/
 *     OUTPUTS/
 *       <LevelName>_<TrajectoryName>.0001.png
 *       <LevelName>_<TrajectoryName>.0002.png
 *       ...
 *       <LevelName>_<TrajectoryName>.NNNN.png
 *       (or for video)
 *       <LevelName>_<TrajectoryName>.mp4
 *     Index.json
 * ```
 * 
 * The Index.json file contains the trajectory data in the format specified by TrajectorySL,
 * including per-frame camera transforms, focal lengths, and other parameters.
 */
class CAMERADATASETGENEDITOR_API CDGMRQExample
{
public:
	/**
	 * Example function that renders all trajectories with default settings
	 * This can be called from a Blueprint or editor utility widget
	 */
	static void RenderAllTrajectoriesExample();

	/**
	 * Example function that renders trajectories with custom high-quality settings
	 */
	static void RenderHighQualityExample();
};
