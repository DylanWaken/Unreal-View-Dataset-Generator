// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CDGTrajectorySubsystem.generated.h"

class ACDGKeyframe;
class ACDGTrajectory;

/**
 * CDGTrajectorySubsystem
 * 
 * Global manager subsystem that manages trajectory actors and keyframes.
 * 
 * Features:
 * - Automatically discovers and tracks all CDGKeyframe actors in the world
 * - Manages ACDGTrajectory actors (spawns, deletes, tracks)
 * - Automatically spawns default trajectory for unassigned keyframes
 * - Handles trajectory name changes and keyframe reassignment
 * - Auto-deletes empty trajectories (trajectories with no keyframes)
 * - Provides API for exporting trajectories to Level Sequences
 * 
 * Based on research from CameraPathResearch.md:
 * - Trajectories contain spline components similar to USplineComponent
 * - Prepares data for export to UMovieScene3DTransformSection
 */
UCLASS()
class CAMERADATASETGEN_API UCDGTrajectorySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ==================== SUBSYSTEM LIFECYCLE ====================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ==================== CONSTANTS ====================

	/** Default trajectory name for new keyframes */
	static const FName DefaultTrajectoryName;

	// ==================== TRAJECTORY MANAGEMENT ====================

	/** Get all trajectory actors */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	TArray<ACDGTrajectory*> GetAllTrajectories() const;

	/** Get a specific trajectory by name */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	ACDGTrajectory* GetTrajectory(FName TrajectoryName) const;
	
	/** Check if a trajectory exists */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	bool HasTrajectory(FName TrajectoryName) const;

	/** Get all trajectory names */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	TArray<FName> GetTrajectoryNames() const;

	/** Get all keyframes in a trajectory, sorted by order */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	TArray<ACDGKeyframe*> GetKeyframesInTrajectory(FName TrajectoryName) const;

	/** Get the number of trajectories */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	int32 GetTrajectoryCount() const { return Trajectories.Num(); }

	/** Spawn a new trajectory actor */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	ACDGTrajectory* SpawnTrajectory(FName TrajectoryName, FVector Location = FVector::ZeroVector);

	/** Get or create a trajectory (spawns if doesn't exist) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	ACDGTrajectory* GetOrCreateTrajectory(FName TrajectoryName);

	// ==================== KEYFRAME MANAGEMENT ====================

	/** Register a keyframe with the subsystem (auto-assigns to default trajectory if unassigned) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void RegisterKeyframe(ACDGKeyframe* Keyframe);

	/** Unregister a keyframe from the subsystem */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void UnregisterKeyframe(ACDGKeyframe* Keyframe);

	/** Called when a keyframe has been modified (position, properties, etc.) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void OnKeyframeModified(ACDGKeyframe* Keyframe);
	
	/** Called when a keyframe's order has been manually changed */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void OnKeyframeOrderChanged(ACDGKeyframe* Keyframe);

	/** Called when a keyframe's trajectory name has changed */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void OnKeyframeTrajectoryNameChanged(ACDGKeyframe* Keyframe, FName OldTrajectoryName);

	/** Called when a trajectory's name has changed */
	void OnTrajectoryNameChanged(ACDGTrajectory* Trajectory);

	/** Refresh all keyframes and trajectories in the world */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void RefreshAll();

	/** Get all registered keyframes */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	TArray<ACDGKeyframe*> GetAllKeyframes() const;

	/** Get keyframes that are not assigned to any trajectory */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	TArray<ACDGKeyframe*> GetUnassignedKeyframes() const;

	// ==================== TRAJECTORY OPERATIONS ====================

	/** Rebuild the spline for a specific trajectory */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void RebuildTrajectorySpline(FName TrajectoryName);

	/** Rebuild all trajectory splines */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void RebuildAllSplines();

	// ==================== EXPORT ====================

	/** Export a trajectory to a Level Sequence (future implementation) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory|Export")
	bool ExportTrajectoryToLevelSequence(FName TrajectoryName, const FString& SequencePath);

	/** Export all trajectories to Level Sequences (future implementation) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory|Export")
	bool ExportAllTrajectoriesToLevelSequences(const FString& OutputDirectory);

	// ==================== UTILITY ====================

	/** Validate all trajectories and auto-delete empty ones */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory|Utility")
	void ValidateAllTrajectories();

	/** Delete a trajectory actor */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory|Utility")
	void DeleteTrajectory(FName TrajectoryName);

	/** Delete a trajectory actor (by reference) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory|Utility")
	void DeleteTrajectoryActor(ACDGTrajectory* Trajectory);

	/** Generate a unique trajectory name */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory|Utility")
	FName GenerateUniqueTrajectoryName(const FString& Prefix = TEXT("Trajectory")) const;

protected:
	// ==================== INTERNAL DATA ====================

	/** All trajectory actors, keyed by trajectory name */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<ACDGTrajectory>> Trajectories;

	/** All registered keyframes (for quick lookup) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ACDGKeyframe>> AllKeyframes;

	/** Whether the subsystem has been initialized */
	bool bIsInitialized = false;

	// ==================== INTERNAL METHODS ====================

public:
	/** Register a trajectory actor with the subsystem */
	void RegisterTrajectory(ACDGTrajectory* Trajectory);

	/** Unregister a trajectory actor from the subsystem */
	void UnregisterTrajectory(ACDGTrajectory* Trajectory);

	/** Scan world for all existing trajectories */
	void RefreshAllTrajectories();

	/** Add a keyframe to a trajectory (spawns trajectory if needed) */
	void AddKeyframeToTrajectory(ACDGKeyframe* Keyframe);

	/** Remove a keyframe from its current trajectory */
	void RemoveKeyframeFromTrajectory(ACDGKeyframe* Keyframe, FName TrajectoryName);

	/** Check and delete trajectories that have no keyframes */
	void CleanupEmptyTrajectories();
};

