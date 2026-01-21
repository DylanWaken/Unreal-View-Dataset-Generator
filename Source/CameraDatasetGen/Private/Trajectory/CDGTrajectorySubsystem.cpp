// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trajectory/CDGTrajectorySubsystem.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGKeyframe.h"
#include "LogCameraDatasetGen.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// ==================== UCDGTrajectorySubsystem Implementation ====================

// Define the default color palette (8 light colors in hex format)
const TArray<FLinearColor> UCDGTrajectorySubsystem::DefaultColorPalette = {
	FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("ffadad"))),  // Light red/pink
	FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("ffd6a5"))),  // Light orange
	FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("fdffb6"))),  // Light yellow
	FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("caffbf"))),  // Light green
	FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("9bf6ff"))),  // Light cyan
	FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("a0c4ff"))),  // Light blue
	FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("bdb2ff"))),  // Light purple
	FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("ffc6ff")))   // Light pink/magenta
};

FLinearColor UCDGTrajectorySubsystem::GetNextTrajectoryColor() const
{
	if (DefaultColorPalette.Num() == 0)
	{
		return FLinearColor::White;
	}
	
	// Cycle through colors based on current trajectory count
	const int32 ColorIndex = Trajectories.Num() % DefaultColorPalette.Num();
	return DefaultColorPalette[ColorIndex];
}

FLinearColor UCDGTrajectorySubsystem::GetTrajectoryColor(FName TrajectoryName) const
{
	if (const TObjectPtr<ACDGTrajectory>* TrajectoryPtr = Trajectories.Find(TrajectoryName))
	{
		if (ACDGTrajectory* Trajectory = TrajectoryPtr->Get())
		{
			return Trajectory->TrajectoryColor;
		}
	}
	
	// Return white if trajectory not found
	return FLinearColor::White;
}

void UCDGTrajectorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bIsInitialized = true;
	bHasPerformedInitialRefresh = false;
}

void UCDGTrajectorySubsystem::Deinitialize()
{
	// Clean up
	Trajectories.Empty();
	AllKeyframes.Empty();

	bIsInitialized = false;

	Super::Deinitialize();
}

void UCDGTrajectorySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Refresh when play begins
	RefreshAll();
	bHasPerformedInitialRefresh = true;
}

void UCDGTrajectorySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bHasPerformedInitialRefresh)
	{
		UE_LOG(LogCameraDatasetGen, Log, TEXT("CDGTrajectorySubsystem: Performing initial refresh in Tick"));
		RefreshAll();
		bHasPerformedInitialRefresh = true;
	}

	// Clear out empty trajectories per tick
	CleanupEmptyTrajectories();
}

TStatId UCDGTrajectorySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCDGTrajectorySubsystem, STATGROUP_Tickables);
}

// ==================== TRAJECTORY MANAGEMENT ====================

TArray<ACDGTrajectory*> UCDGTrajectorySubsystem::GetAllTrajectories() const
{
	TArray<ACDGTrajectory*> Result;
	
	for (const auto& Pair : Trajectories)
	{
		if (Pair.Value)
		{
			Result.Add(Pair.Value.Get());
		}
	}
	
	return Result;
}

ACDGTrajectory* UCDGTrajectorySubsystem::GetTrajectory(FName TrajectoryName) const
{
	if (const TObjectPtr<ACDGTrajectory>* TrajectoryPtr = Trajectories.Find(TrajectoryName))
	{
		return TrajectoryPtr->Get();
	}
	return nullptr;
}

bool UCDGTrajectorySubsystem::HasTrajectory(FName TrajectoryName) const
{
	return Trajectories.Contains(TrajectoryName);
}

TArray<FName> UCDGTrajectorySubsystem::GetTrajectoryNames() const
{
	TArray<FName> Names;
	Trajectories.GetKeys(Names);
	return Names;
}

TArray<ACDGKeyframe*> UCDGTrajectorySubsystem::GetKeyframesInTrajectory(FName TrajectoryName) const
{
	if (ACDGTrajectory* Trajectory = GetTrajectory(TrajectoryName))
	{
		return Trajectory->GetSortedKeyframes();
	}
	
	return TArray<ACDGKeyframe*>();
}

ACDGTrajectory* UCDGTrajectorySubsystem::SpawnTrajectory(FName TrajectoryName, FVector Location)
{
	UWorld* World = GetWorld();
	if (!World || TrajectoryName.IsNone())
	{
		return nullptr;
	}

	// Check if trajectory already exists in our registry
	if (HasTrajectory(TrajectoryName))
	{
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("Trajectory '%s' already exists in registry"), *TrajectoryName.ToString());
		return GetTrajectory(TrajectoryName);
	}

	// Scan for existing trajectory actors in the level that might not be registered yet
	// This can happen during level loading when actors are being deserialized
	for (TActorIterator<ACDGTrajectory> It(World); It; ++It)
	{
		ACDGTrajectory* ExistingTrajectory = *It;
		if (IsValid(ExistingTrajectory) && ExistingTrajectory->TrajectoryName == TrajectoryName)
		{
			// Found an existing trajectory actor with this name - register it
			UE_LOG(LogCameraDatasetGen, Log, TEXT("Found existing trajectory actor for '%s', registering it"), *TrajectoryName.ToString());
			ExistingTrajectory->SetActorLabel(TrajectoryName.ToString());
			RegisterTrajectory(ExistingTrajectory);
			return ExistingTrajectory;
		}
	}

	// Spawn new trajectory actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = *FString::Printf(TEXT("Trajectory_%s"), *TrajectoryName.ToString());
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested; // Allow unique name generation
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACDGTrajectory* NewTrajectory = World->SpawnActor<ACDGTrajectory>(ACDGTrajectory::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
	
	if (NewTrajectory)
	{
		NewTrajectory->TrajectoryName = TrajectoryName;
		NewTrajectory->SetActorLabel(TrajectoryName.ToString());
		RegisterTrajectory(NewTrajectory);
		UE_LOG(LogCameraDatasetGen, Log, TEXT("Spawned new trajectory actor '%s' for trajectory '%s'"), *NewTrajectory->GetName(), *TrajectoryName.ToString());
	}
	else
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("Failed to spawn trajectory actor for '%s'"), *TrajectoryName.ToString());
	}

	return NewTrajectory;
}

ACDGTrajectory* UCDGTrajectorySubsystem::GetOrCreateTrajectory(FName TrajectoryName)
{
	// Get existing trajectory
	if (ACDGTrajectory* Trajectory = GetTrajectory(TrajectoryName))
	{
		return Trajectory;
	}

	// Spawn new trajectory
	return SpawnTrajectory(TrajectoryName);
}

// ==================== KEYFRAME MANAGEMENT ====================

void UCDGTrajectorySubsystem::RegisterKeyframe(ACDGKeyframe* Keyframe)
{
	if (!Keyframe || AllKeyframes.Contains(Keyframe))
	{
		return;
	}

	AllKeyframes.Add(Keyframe);

	// If keyframe has no trajectory assigned, generate a unique one
	// Otherwise, keep its existing trajectory (important for duplication)
	if (!Keyframe->IsAssignedToTrajectory())
	{
		Keyframe->TrajectoryName = GenerateUniqueTrajectoryName();
	}

	// Add to trajectory (will get existing trajectory or create new one)
	AddKeyframeToTrajectory(Keyframe);
}

void UCDGTrajectorySubsystem::UnregisterKeyframe(ACDGKeyframe* Keyframe)
{
	if (!Keyframe)
	{
		return;
	}

	// Remove from trajectory
	if (Keyframe->IsAssignedToTrajectory())
	{
		RemoveKeyframeFromTrajectory(Keyframe, Keyframe->TrajectoryName);
	}

	// Remove from all keyframes list
	AllKeyframes.Remove(Keyframe);

	// Clean up empty trajectories
	CleanupEmptyTrajectories();
}

void UCDGTrajectorySubsystem::OnKeyframeModified(ACDGKeyframe* Keyframe)
{
	if (!Keyframe)
	{
		return;
	}

	// Rebuild the trajectory spline
	if (Keyframe->IsAssignedToTrajectory())
	{
		if (ACDGTrajectory* Trajectory = GetTrajectory(Keyframe->TrajectoryName))
		{
			Trajectory->MarkNeedsRebuild();
			Trajectory->RebuildSpline();
		}
	}
}

void UCDGTrajectorySubsystem::OnKeyframeOrderChanged(ACDGKeyframe* Keyframe)
{
	if (!Keyframe || !Keyframe->IsAssignedToTrajectory())
	{
		return;
	}

	// Get the trajectory this keyframe belongs to
	if (ACDGTrajectory* Trajectory = GetTrajectory(Keyframe->TrajectoryName))
	{
		// Trigger auto-reassignment of all keyframe orders, passing the changed keyframe for swap detection
		Trajectory->OnKeyframeOrderManuallyChanged(Keyframe);
	}
}

void UCDGTrajectorySubsystem::OnKeyframeTrajectoryNameChanged(ACDGKeyframe* Keyframe, FName OldTrajectoryName)
{
	if (!Keyframe)
	{
		return;
	}

	// Remove from old trajectory
	if (!OldTrajectoryName.IsNone())
	{
		RemoveKeyframeFromTrajectory(Keyframe, OldTrajectoryName);
	}

	// Add to new trajectory (will spawn if doesn't exist)
	if (Keyframe->IsAssignedToTrajectory())
	{
		AddKeyframeToTrajectory(Keyframe);
	}

	// Clean up empty trajectories
	CleanupEmptyTrajectories();
}

void UCDGTrajectorySubsystem::OnTrajectoryNameChanged(ACDGTrajectory* Trajectory)
{
	if (!Trajectory)
	{
		return;
	}

	// Find trajectory by old name and update mapping
	FName OldName = NAME_None;
	for (const auto& Pair : Trajectories)
	{
		if (Pair.Value == Trajectory && Pair.Key != Trajectory->TrajectoryName)
		{
			OldName = Pair.Key;
			break;
		}
	}

	if (!OldName.IsNone())
	{
		// Remove old mapping
		Trajectories.Remove(OldName);
		
		// Add new mapping
		Trajectories.Add(Trajectory->TrajectoryName, Trajectory);

		// Update all keyframes in this trajectory
		for (const TObjectPtr<ACDGKeyframe>& Keyframe : Trajectory->Keyframes)
		{
			if (Keyframe)
			{
				Keyframe->TrajectoryName = Trajectory->TrajectoryName;
			}
		}
	}
}

void UCDGTrajectorySubsystem::RefreshAll()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Clear existing data
	AllKeyframes.Empty();
	Trajectories.Empty();

	// Find all trajectory actors
	RefreshAllTrajectories();

	// Find all keyframe actors
	for (TActorIterator<ACDGKeyframe> It(World); It; ++It)
	{
		ACDGKeyframe* Keyframe = *It;
		if (IsValid(Keyframe))
		{
		AllKeyframes.Add(Keyframe);

		// If keyframe has no trajectory assigned, generate a unique one
		if (!Keyframe->IsAssignedToTrajectory())
		{
			Keyframe->TrajectoryName = GenerateUniqueTrajectoryName();
		}

		// Add to trajectory
		AddKeyframeToTrajectory(Keyframe);
		}
	}

	// Clean up empty trajectories
	CleanupEmptyTrajectories();

	// Rebuild all splines
	RebuildAllSplines();
}

TArray<ACDGKeyframe*> UCDGTrajectorySubsystem::GetAllKeyframes() const
{
	TArray<ACDGKeyframe*> Result;
	
	for (const TObjectPtr<ACDGKeyframe>& Keyframe : AllKeyframes)
	{
		if (Keyframe)
		{
			Result.Add(Keyframe.Get());
		}
	}
	
	return Result;
}

TArray<ACDGKeyframe*> UCDGTrajectorySubsystem::GetUnassignedKeyframes() const
{
	TArray<ACDGKeyframe*> Result;
	
	for (const TObjectPtr<ACDGKeyframe>& Keyframe : AllKeyframes)
	{
		if (Keyframe && !Keyframe->IsAssignedToTrajectory())
		{
			Result.Add(Keyframe.Get());
		}
	}
	
	return Result;
}

// ==================== TRAJECTORY OPERATIONS ====================

void UCDGTrajectorySubsystem::RebuildTrajectorySpline(FName TrajectoryName)
{
	if (ACDGTrajectory* Trajectory = GetTrajectory(TrajectoryName))
	{
		Trajectory->RebuildSpline();
	}
}

void UCDGTrajectorySubsystem::RebuildAllSplines()
{
	for (auto& Pair : Trajectories)
	{
		if (Pair.Value)
		{
			Pair.Value->RebuildSpline();
		}
	}
}

// ==================== EXPORT ====================

bool UCDGTrajectorySubsystem::ExportTrajectoryToLevelSequence(FName TrajectoryName, const FString& SequencePath)
{
	// TODO: Implement Level Sequence export
	// This will create a ULevelSequence asset with:
	// - Camera actor binding
	// - UMovieScene3DTransformTrack with keyframes from trajectory
	// - UMovieSceneCameraCutTrack for camera activation
	// - Camera property tracks (focal length, aperture, etc.)
	
	UE_LOG(LogCameraDatasetGen, Warning, TEXT("ExportTrajectoryToLevelSequence not yet implemented"));
	return false;
}

bool UCDGTrajectorySubsystem::ExportAllTrajectoriesToLevelSequences(const FString& OutputDirectory)
{
	// TODO: Implement batch export
	
	UE_LOG(LogCameraDatasetGen, Warning, TEXT("ExportAllTrajectoriesToLevelSequences not yet implemented"));
	return false;
}

// ==================== UTILITY ====================

void UCDGTrajectorySubsystem::ValidateAllTrajectories()
{
	for (auto& Pair : Trajectories)
	{
		if (Pair.Value)
		{
			Pair.Value->ValidateKeyframes();
		}
	}

	// Clean up empty trajectories
	CleanupEmptyTrajectories();
}

void UCDGTrajectorySubsystem::DeleteTrajectory(FName TrajectoryName)
{
	if (ACDGTrajectory* Trajectory = GetTrajectory(TrajectoryName))
	{
		DeleteTrajectoryActor(Trajectory);
	}
}

void UCDGTrajectorySubsystem::DeleteTrajectoryActor(ACDGTrajectory* Trajectory)
{
	if (!Trajectory)
	{
		return;
	}

	const FName TrajectoryName = Trajectory->TrajectoryName;

	// Reassign all keyframes to their own unique trajectories
	TArray<TObjectPtr<ACDGKeyframe>> KeyframesCopy = Trajectory->Keyframes;
	for (TObjectPtr<ACDGKeyframe>& Keyframe : KeyframesCopy)
	{
		if (Keyframe)
		{
			// Store old trajectory name
			FName OldTrajectoryName = Keyframe->TrajectoryName;
			
			// Assign to a unique trajectory for this keyframe
			Keyframe->TrajectoryName = GenerateUniqueTrajectoryName();
			
			// Remove from current trajectory
			Trajectory->RemoveKeyframe(Keyframe.Get());
			
			// Notify subsystem to add to new trajectory
			OnKeyframeTrajectoryNameChanged(Keyframe.Get(), OldTrajectoryName);
		}
	}

	// Unregister from subsystem
	UnregisterTrajectory(Trajectory);

	// Destroy the actor
	Trajectory->Destroy();
}

// ==================== INTERNAL METHODS ====================

void UCDGTrajectorySubsystem::RegisterTrajectory(ACDGTrajectory* Trajectory)
{
	if (!Trajectory || Trajectory->TrajectoryName.IsNone())
	{
		return;
	}

	// Check for name collision with different actor instance
	if (Trajectories.Contains(Trajectory->TrajectoryName))
	{
		ACDGTrajectory* ExistingTrajectory = Trajectories[Trajectory->TrajectoryName].Get();
		
		// If it's the same actor, just return (already registered)
		if (ExistingTrajectory == Trajectory)
		{
			return;
		}
		
		// Different actor with same trajectory name - this is a problem
		// Generate a unique trajectory name for the new actor
		UE_LOG(LogCameraDatasetGen, Warning, TEXT("Trajectory name collision: %s. Generating unique name for new trajectory."), *Trajectory->TrajectoryName.ToString());
		
		FName UniqueName = Trajectory->TrajectoryName;
		int32 Suffix = 1;
		while (Trajectories.Contains(UniqueName))
		{
			UniqueName = FName(*FString::Printf(TEXT("%s_%d"), *Trajectory->TrajectoryName.ToString(), Suffix));
			Suffix++;
		}
		
		UE_LOG(LogCameraDatasetGen, Log, TEXT("Renamed trajectory from '%s' to '%s'"), *Trajectory->TrajectoryName.ToString(), *UniqueName.ToString());
		Trajectory->TrajectoryName = UniqueName;
	}

	// Assign color from palette before adding to map
	Trajectory->TrajectoryColor = GetNextTrajectoryColor();
	
	Trajectories.Add(Trajectory->TrajectoryName, Trajectory);
	
	// Update trajectory visualizer with new color
	Trajectory->UpdateVisualizer();
	
	// Update all keyframes in this trajectory to use the new color
	for (const TObjectPtr<ACDGKeyframe>& Keyframe : Trajectory->Keyframes)
	{
		if (Keyframe)
		{
			Keyframe->UpdateVisualizer();
		}
	}
	
	UE_LOG(LogCameraDatasetGen, Verbose, TEXT("Registered trajectory: %s"), 
		*Trajectory->TrajectoryName.ToString());
}

void UCDGTrajectorySubsystem::UnregisterTrajectory(ACDGTrajectory* Trajectory)
{
	if (!Trajectory)
	{
		return;
	}

	Trajectories.Remove(Trajectory->TrajectoryName);
}

void UCDGTrajectorySubsystem::RefreshAllTrajectories()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Find all trajectory actors in the world
	for (TActorIterator<ACDGTrajectory> It(World); It; ++It)
	{
		ACDGTrajectory* Trajectory = *It;
		if (IsValid(Trajectory))
		{
			RegisterTrajectory(Trajectory);
		}
	}
}

void UCDGTrajectorySubsystem::AddKeyframeToTrajectory(ACDGKeyframe* Keyframe)
{
	if (!Keyframe || !Keyframe->IsAssignedToTrajectory())
	{
		return;
	}

	const FName TrajectoryName = Keyframe->TrajectoryName;

	// Get or create trajectory
	ACDGTrajectory* Trajectory = GetOrCreateTrajectory(TrajectoryName);
	if (!Trajectory)
	{
		UE_LOG(LogCameraDatasetGen, Error, TEXT("Failed to get or create trajectory: %s"), *TrajectoryName.ToString());
		return;
	}

	// Add keyframe to trajectory
	Trajectory->AddKeyframe(Keyframe);
	Trajectory->MarkNeedsRebuild();
	Trajectory->RebuildSpline();
	
	// Update keyframe visualizer to use trajectory color
	Keyframe->UpdateVisualizer();
}

void UCDGTrajectorySubsystem::RemoveKeyframeFromTrajectory(ACDGKeyframe* Keyframe, FName TrajectoryName)
{
	if (!Keyframe || TrajectoryName.IsNone())
	{
		return;
	}

	// Get trajectory
	ACDGTrajectory* Trajectory = GetTrajectory(TrajectoryName);
	if (!Trajectory)
	{
		return;
	}

	// Remove keyframe from trajectory
	Trajectory->RemoveKeyframe(Keyframe);
	Trajectory->MarkNeedsRebuild();
	Trajectory->RebuildSpline();
}

void UCDGTrajectorySubsystem::CleanupEmptyTrajectories()
{
	TArray<FName> TrajectoriesToDelete;

	// Find empty trajectories
	for (const auto& Pair : Trajectories)
	{
		if (Pair.Value && Pair.Value->IsEmpty())
		{
			TrajectoriesToDelete.Add(Pair.Key);
		}
	}

	// Delete empty trajectories
	for (const FName& TrajectoryName : TrajectoriesToDelete)
	{
		DeleteTrajectory(TrajectoryName);
	}
}

FName UCDGTrajectorySubsystem::GenerateUniqueTrajectoryName(const FString& Prefix) const
{
	// Generate a unique trajectory name by appending a counter
	int32 Counter = 1;
	FName UniqueName;
	
	do
	{
		UniqueName = FName(*FString::Printf(TEXT("%s_%d"), *Prefix, Counter));
		Counter++;
	}
	while (HasTrajectory(UniqueName));
	
	return UniqueName;
}

// ==================== VISUALIZER CONTROL ====================

void UCDGTrajectorySubsystem::DisableAllVisualizers()
{
	// Clear previous saved states
	SavedTrajectoryVisualizerStates.Empty();
	SavedKeyframeVisualizerStates.Empty();

	// Save and disable trajectory visualizers
	for (const auto& Pair : Trajectories)
	{
		if (ACDGTrajectory* Trajectory = Pair.Value.Get())
		{
			// Save current state
			SavedTrajectoryVisualizerStates.Add(Pair.Key, Trajectory->bShowTrajectory);
			
			// Disable visualizer
			Trajectory->bShowTrajectory = false;
			Trajectory->UpdateVisualizer();
		}
	}

	// Save and disable keyframe visualizers
	for (const TObjectPtr<ACDGKeyframe>& Keyframe : AllKeyframes)
	{
		if (Keyframe.Get())
		{
			// Save current states (frustum and trajectory line)
			SavedKeyframeVisualizerStates.Add(Keyframe, TPair<bool, bool>(Keyframe->bShowCameraFrustum, Keyframe->bShowTrajectoryLine));
			
			// Disable visualizers
			Keyframe->bShowCameraFrustum = false;
			Keyframe->bShowTrajectoryLine = false;
			Keyframe->UpdateVisualizer();
		}
	}
	
	UE_LOG(LogCameraDatasetGen, Verbose, TEXT("Disabled all visualizers (Trajectories: %d, Keyframes: %d)"), 
		SavedTrajectoryVisualizerStates.Num(), SavedKeyframeVisualizerStates.Num());
}

void UCDGTrajectorySubsystem::EnableAllVisualizers()
{
	// Enable all trajectory visualizers
	for (const auto& Pair : Trajectories)
	{
		if (ACDGTrajectory* Trajectory = Pair.Value.Get())
		{
			Trajectory->bShowTrajectory = true;
			Trajectory->UpdateVisualizer();
		}
	}

	// Enable all keyframe visualizers
	for (const TObjectPtr<ACDGKeyframe>& Keyframe : AllKeyframes)
	{
		if (Keyframe.Get())
		{
			Keyframe->bShowCameraFrustum = true;
			Keyframe->bShowTrajectoryLine = true;
			Keyframe->UpdateVisualizer();
		}
	}
	
	UE_LOG(LogCameraDatasetGen, Verbose, TEXT("Enabled all visualizers"));
}

void UCDGTrajectorySubsystem::RestoreVisualizerStates()
{
	// Restore trajectory visualizers
	for (const auto& Pair : SavedTrajectoryVisualizerStates)
	{
		if (ACDGTrajectory* Trajectory = GetTrajectory(Pair.Key))
		{
			Trajectory->bShowTrajectory = Pair.Value;
			Trajectory->UpdateVisualizer();
		}
	}

	// Restore keyframe visualizers
	for (const auto& Pair : SavedKeyframeVisualizerStates)
	{
		if (ACDGKeyframe* Keyframe = Pair.Key.Get())
		{
			Keyframe->bShowCameraFrustum = Pair.Value.Key;
			Keyframe->bShowTrajectoryLine = Pair.Value.Value;
			Keyframe->UpdateVisualizer();
		}
	}
	
	// Clear saved states
	SavedTrajectoryVisualizerStates.Empty();
	SavedKeyframeVisualizerStates.Empty();
	
	UE_LOG(LogCameraDatasetGen, Verbose, TEXT("Restored visualizer states"));
}