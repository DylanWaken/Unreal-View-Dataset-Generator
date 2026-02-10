// Copyright Epic Games, Inc. All Rights Reserved.

#include "MRQInterface/CDGMRQExample.h"
#include "MRQInterface/CDGMRQInterface.h"
#include "LogCameraDatasetGenEditor.h"
#include "Misc/Paths.h"

void CDGMRQExample::RenderAllTrajectoriesExample()
{
	// Setup default configuration
	FTrajectoryRenderConfig Config;
	
	// Set output directory to project's Saved/Renders folder
	Config.DestinationRootDir = FPaths::ProjectSavedDir() / TEXT("Renders");
	
	// Set output resolution to Full HD
	Config.OutputResolutionOverride = FIntPoint(1920, 1080);
	
	// Set framerate to 30 FPS
	Config.OutputFramerateOverride = 30;
	
	// Use PNG sequence output
	Config.ExportFormat = ECDGRenderOutputFormat::PNG_Sequence;
	
	// Export Index.json with trajectory data
	Config.bExportIndexJSON = true;
	
	// Don't overwrite existing output
	Config.bOverwriteExistingOutput = false;
	
	// Use moderate anti-aliasing
	Config.SpatialSampleCount = 2;
	Config.TemporalSampleCount = 4;
	
	// Start rendering
	bool bSuccess = CDGMRQInterface::RenderTrajectories(Config);
	
	if (bSuccess)
	{
		UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQExample: Started rendering all trajectories to: %s"), *Config.DestinationRootDir);
	}
	else
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQExample: Failed to start rendering"));
	}
}

void CDGMRQExample::RenderHighQualityExample()
{
	// Setup high-quality configuration
	FTrajectoryRenderConfig Config;
	
	// Set output directory
	Config.DestinationRootDir = FPaths::ProjectSavedDir() / TEXT("Renders_HQ");
	
	// Set output resolution to 4K
	Config.OutputResolutionOverride = FIntPoint(3840, 2160);
	
	// Set framerate to 60 FPS
	Config.OutputFramerateOverride = 60;
	
	// Use EXR sequence output for HDR
	Config.ExportFormat = ECDGRenderOutputFormat::EXR_Sequence;
	
	// Export Index.json with trajectory data
	Config.bExportIndexJSON = true;
	
	// Overwrite existing output
	Config.bOverwriteExistingOutput = true;
	
	// Use high anti-aliasing for quality
	Config.SpatialSampleCount = 8;
	Config.TemporalSampleCount = 16;
	
	// Start rendering
	bool bSuccess = CDGMRQInterface::RenderTrajectories(Config);
	
	if (bSuccess)
	{
		UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGMRQExample: Started high-quality rendering to: %s"), *Config.DestinationRootDir);
	}
	else
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("CDGMRQExample: Failed to start rendering"));
	}
}
