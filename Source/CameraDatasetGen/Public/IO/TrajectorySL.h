// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "JsonObjectConverter.h"

class ACDGTrajectory;
class ACDGKeyframe;

/**
 * Trajectory Save/Load System
 * 
 * Provides functionality to save and load all trajectories in the system
 * including keyframe data and per-frame interpolated data.
 * 
 * Format:
 * {
 *   "LevelName": "ExampleLevel",
 *   "Trajectories": [
 *     {
 *       "TrajectoryIndex": 0,
 *       "TrajectoryName": "Trajectory_01",
 *       "Prompt": "Camera moves forward smoothly",
 *       "KeyFrames": [...],
 *       "Frames": [...]
 *     }
 *   ]
 * }
 */
namespace TrajectorySL
{
	/**
	 * Save all trajectories in the current world to a JSON file
	 * 
	 * @param FilePath - Full path to the output JSON file
	 * @param FPS - Frames per second for frame interpolation (default: 30)
	 * @param bPrettyPrint - Whether to format JSON with indentation (default: true)
	 * @return true if save was successful, false otherwise
	 */
	CAMERADATASETGEN_API bool SaveAllTrajectories(const FString& FilePath, int32 FPS = 30, bool bPrettyPrint = true);

	/**
	 * Save all trajectories to a JSON string
	 * 
	 * @param OutJsonString - Output JSON string
	 * @param FPS - Frames per second for frame interpolation (default: 30)
	 * @param bPrettyPrint - Whether to format JSON with indentation (default: true)
	 * @return true if generation was successful, false otherwise
	 */
	CAMERADATASETGEN_API bool SaveAllTrajectoriesAsString(FString& OutJsonString, int32 FPS = 30, bool bPrettyPrint = true);

	/**
	 * Load trajectories from a JSON file (to be implemented)
	 * 
	 * @param FilePath - Full path to the input JSON file
	 * @return true if load was successful, false otherwise
	 */
	CAMERADATASETGEN_API bool LoadAllTrajectories(const FString& FilePath);

	// Internal helper functions
	namespace Internal
	{
		/** Convert a keyframe to a JSON object */
		TSharedPtr<FJsonObject> KeyframeToJson(ACDGKeyframe* Keyframe);

		/** Generate per-frame data for a trajectory */
		TArray<TSharedPtr<FJsonValue>> GenerateFrameData(ACDGTrajectory* Trajectory, int32 FPS);

		/** Interpolate transform between two keyframes at a given alpha */
		FTransform InterpolateTransform(ACDGKeyframe* KeyframeA, ACDGKeyframe* KeyframeB, float Alpha);

		/** Interpolate focal length between two keyframes at a given alpha */
		float InterpolateFocalLength(ACDGKeyframe* KeyframeA, ACDGKeyframe* KeyframeB, float Alpha);

		/** Load a keyframe from JSON and spawn it in the world */
		ACDGKeyframe* LoadKeyframeFromJson(UWorld* World, const TSharedPtr<FJsonObject>& KeyframeObj, FName TrajectoryName, int32 Order);
	}
}
