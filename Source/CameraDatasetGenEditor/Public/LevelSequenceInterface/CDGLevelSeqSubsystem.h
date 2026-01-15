// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LevelSequence.h"
#include "CDGLevelSeqSubsystem.generated.h"

class UCDGTrajectorySubsystem;

/**
 * CDGLevelSeqSubsystem
 * 
 * Manages the Level Sequence instance for the current level.
 * Maintains exactly one Level Sequence per level named CDG_<LevelName>_SEQ.
 */
UCLASS()
class CAMERADATASETGENEDITOR_API UCDGLevelSeqSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ==================== SUBSYSTEM LIFECYCLE ====================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ==================== OPERATIONS ====================

	/**
	 * Initialize (or create) the level sequence for the current level.
	 */
	UFUNCTION(BlueprintCallable, Category = "CDG|LevelSequence")
	void InitLevelSequence();

	/**
	 * Delete the level sequence asset associated with this level.
	 */
	UFUNCTION(BlueprintCallable, Category = "CDG|LevelSequence")
	void DeleteLevelSequence();

	/**
	 * Get the currently active level sequence.
	 */
	UFUNCTION(BlueprintCallable, Category = "CDG|LevelSequence")
	ULevelSequence* GetActiveLevelSequence() const { return ActiveLevelSequence; }

	/**
	 * Get the Trajectory Subsystem.
	 */
	UFUNCTION(BlueprintCallable, Category = "CDG|LevelSequence")
	UCDGTrajectorySubsystem* GetTrajectorySubsystem() const;

	/**
	 * Get the expected package name for the sequence: <LevelPath>/CDG_<LevelName>_SEQ
	 */
	UFUNCTION(BlueprintCallable, Category = "CDG|LevelSequence")
	FString GetSequencePackageName() const;

protected:
	// ==================== INTERNAL ====================

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequence> ActiveLevelSequence;

	/** Scans for an existing level sequence matching the naming convention */
	void ScanForLevelSequence();

#if WITH_EDITOR
	/** Creates the asset in the editor */
	ULevelSequence* CreateSequenceAsset(const FString& PackageName, const FString& AssetName);
#endif
};

