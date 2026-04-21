// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AssetRegistry/AssetData.h"
#include "CDGBatchProcExecService.generated.h"

class UGeneratorStackConfig;
class ULevelSeqExportConfig;
class ULevelSequence;
class UCDGTrajectoryGenerator;
class ACDGLevelSceneAnchor;
class ACDGTrajectory;

// ─────────────────────────────────────────────────────────────────────────────
// FBatchProcInput  —  everything the batch processor needs to run
// ─────────────────────────────────────────────────────────────────────────────

struct FBatchProcInput
{
	/** Level (UWorld) assets to process sequentially */
	TArray<FAssetData> Levels;

	/** Character Blueprint assets — must derive from APawn/ACharacter and contain
	 *  a UCDGCharacterAnchor + USkeletalMeshComponent as per the picker filter. */
	TArray<FAssetData> Characters;

	/** Animation assets played on the character during each combination */
	TArray<FAssetData> Animations;

	/** Generator stack that produces camera trajectories */
	TWeakObjectPtr<UGeneratorStackConfig> GeneratorConfig;

	/** Export / render settings (FPS, resolution, format, output directory …) */
	TWeakObjectPtr<ULevelSeqExportConfig> ExporterConfig;
};

// ─────────────────────────────────────────────────────────────────────────────
// UCDGBatchProcExecService
//
// Drives the nested batch loop:
//
//   FOR lv IN Levels:
//     Open level lv
//     FOR sc IN SceneAnchors:           ← discovered in lv
//       FOR ch IN Characters:
//         Spawn ch at ground-casted position within sc.DispersionRadius
//         FOR anim IN Animations:
//           Create reference level-seq  (ch animated by anim, duration = anim)
//           Instantiate generators from config; set refSeq + ch actor
//           Run generator stack → ACDGTrajectory actors
//           Export trajectories to shot sequences (ch animation in each shot)
//           Render via MRQ → <OutputDir>/<ComboKey>/OUTPUTS/
//           Write  <OutputDir>/<ComboKey>/<ComboKey>.json
//           Clear all trajectory actors and level sequences
//         Destroy ch actor
//
// Output file structure:
//   ExporterConfig.OutputDirectory/
//     {Level}_{Anchor}_{Character}_{Anim}/
//       {Level}_{Anchor}_{Character}_{Anim}.json
//       OUTPUTS/
//         {ComboKey}.{TrajName}.{frame}.png  (or .mp4 etc.)
//
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class CAMERADATASETGENEDITOR_API UCDGBatchProcExecService : public UObject
{
	GENERATED_BODY()

public:
	// ── Lifecycle ────────────────────────────────────────────────────────────

	/**
	 * Construct a new service.
	 * The caller must keep a hard reference (or call AddToRoot) until
	 * OnBatchCompleted fires, then call RemoveFromRoot.
	 */
	static UCDGBatchProcExecService* Create(UObject* Outer, const FBatchProcInput& Input);

	/** Begin execution.  Must be called at most once per instance. */
	void Start();

	/** Request cancellation; the current combo finishes before stopping. */
	void Cancel();

	bool IsRunning() const { return bIsRunning; }

	// ── Progress / completion delegates ──────────────────────────────────────

	/** Fired whenever a combo starts: (completedCombos, totalCombos) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProgressUpdated, int32, int32);
	FOnProgressUpdated OnProgressUpdated;

	/** Fired for each log line the service wants to surface to the UI */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLogMessage, const FString&);
	FOnLogMessage OnLogMessage;

	/** Fired once when the entire batch finishes (success) or is cancelled/fails */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBatchCompleted, bool /*bSuccess*/);
	FOnBatchCompleted OnBatchCompleted;

private:
	// ── Input / state ────────────────────────────────────────────────────────

	FBatchProcInput Input;
	bool bIsRunning    = false;
	bool bCancelled    = false;
	bool bStarted      = false;

	// Level iteration
	int32 CurrentLevelIdx = 0;

	// Anchor/character/animation iteration within the current level
	TArray<TWeakObjectPtr<ACDGLevelSceneAnchor>> CurrentAnchors;
	int32 CurrentAnchorIdx    = 0;
	int32 CurrentCharacterIdx = 0;
	int32 CurrentAnimIdx      = 0;

	// Per-combo live objects (cleared after each combo)
	TWeakObjectPtr<AActor>         SpawnedCharacter;
	TWeakObjectPtr<ULevelSequence> CurrentRefSequence;
	TArray<FString>                CreatedShotSequencePaths; // collected during export, deleted afterwards

	// Generator instances re-created for each level (world must be the outer)
	TArray<TObjectPtr<UCDGTrajectoryGenerator>> Generators;

	// Progress tracking
	int32 TotalCombos     = 0;
	int32 CompletedCombos = 0;

	// ── Top-level step machine ───────────────────────────────────────────────

	void BeginProcessLevel();
	void DiscoverAnchorsAndBeginCombos();
	void BeginNextCombo();
	void ExecuteCurrentCombo();

	// ── Per-combo steps ──────────────────────────────────────────────────────

	/** Spawn character BP at a randomly ray-cast ground position within sc radius. */
	AActor* SpawnCharacterAtAnchor(UWorld* World,
	                               ACDGLevelSceneAnchor* Anchor,
	                               UClass* CharacterClass);

	/**
	 * Create a temporary reference level sequence whose duration equals the
	 * animation's play length and that binds the character + animation track.
	 * The sequence is stored at /Game/CDGBatch_Temp/<ComboKey>_RefSeq.
	 */
	ULevelSequence* CreateReferenceSequence(UWorld* World,
	                                        AActor* Character,
	                                        class UAnimationAsset* Anim,
	                                        const FString& ComboKey,
	                                        int32 FPS);

	/** Instantiate generators from GeneratorStackConfig with World as outer. */
	void InstantiateGenerators(UWorld* World,
	                           ULevelSequence* RefSeq,
	                           AActor* Character);

	/** Run all generator instances, return created ACDGTrajectory actors. */
	TArray<ACDGTrajectory*> RunGenerators(UWorld* World);

	/**
	 * Export the current trajectory actors to a master+shot level sequence.
	 * RefSeq is used as the base shot (its non-camera tracks are copied into
	 * each shot so the character animation is present during rendering).
	 * Returns the master sequence, or nullptr on failure.
	 * Populates CreatedShotSequencePaths with all created shot package names.
	 */
	ULevelSequence* ExportTrajectoriesAsLevelSequence(UWorld* World,
	                                                  ULevelSequence* RefSeq,
	                                                  const TArray<ACDGTrajectory*>& Trajectories,
	                                                  int32 FPS);

	/**
	 * Write <ComboKey>.json at ComboOutputDir (same level as OUTPUTS/).
	 * Uses TrajectorySL to serialise all trajectories in the world.
	 */
	void WriteComboIndexJson(const FString& ComboOutputDir,
	                         const FString& ComboKey,
	                         int32 FPS);

	/** Delete all ACDGTrajectory+ACDGKeyframe actors and the master+shot sequences. */
	void CleanupComboAssets(UWorld* World);

	/** Destroy the spawned character actor. */
	void DestroySpawnedCharacter(UWorld* World);

	/** Destroy all transient generator instances. */
	void DestroyGenerators();

	/** Delete the reference sequence asset. */
	void DeleteReferenceSequence();

	/** Advance (Anim → Char → Anchor) indices; returns false if level is exhausted. */
	bool AdvanceComboIndices();

	// ── Helpers ──────────────────────────────────────────────────────────────

	FString MakeComboKey(const FString& LevelShortName,
	                     const FString& AnchorName,
	                     const FString& CharShortName,
	                     const FString& AnimShortName) const;

	/** Vertical line-trace to find ground below Centre+offset, within Radius. */
	FVector FindGroundPosition(UWorld* World,
	                           const FVector& Centre,
	                           float Radius,
	                           const AActor* IgnoredActor) const;

	/** Return all ACDGLevelSceneAnchor actors in World. */
	TArray<ACDGLevelSceneAnchor*> GetAnchors(UWorld* World) const;

	int32 ComputeTotalCombos() const;

	void BroadcastLog(const FString& Msg);
	void Finish(bool bSuccess);
};
