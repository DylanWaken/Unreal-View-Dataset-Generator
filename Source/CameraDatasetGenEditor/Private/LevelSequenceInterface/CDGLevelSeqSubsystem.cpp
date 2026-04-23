// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceInterface/CDGLevelSeqSubsystem.h"
#include "LevelSequence.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Misc/MessageDialog.h"

#if WITH_EDITOR
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ObjectTools.h"
#include "FileHelpers.h"
#endif

void UCDGLevelSeqSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// Initialize dependency on Trajectory Subsystem
	Collection.InitializeDependency(UCDGTrajectorySubsystem::StaticClass());
}

void UCDGLevelSeqSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UCDGLevelSeqSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	// Scan for existing sequence on startup
	ScanForLevelSequence();
}

bool UCDGLevelSeqSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer);
}

void UCDGLevelSeqSubsystem::ScanForLevelSequence()
{
	FString SequencePackageName = GetSequencePackageName();
	if (SequencePackageName.IsEmpty())
	{
		return;
	}

	ActiveLevelSequence = LoadObject<ULevelSequence>(nullptr, *SequencePackageName);
	// Guard: LoadObject can return a garbage-marked object that was deleted by
	// a previous batch combo but not yet GC'd.  Treat it as "not found".
	if (ActiveLevelSequence && !IsValid(ActiveLevelSequence))
		ActiveLevelSequence = nullptr;
	
	if (ActiveLevelSequence)
	{
		UE_LOG(LogTemp, Log, TEXT("CDGLevelSeqSubsystem: Found existing Level Sequence: %s"), *SequencePackageName);
	}
}

void UCDGLevelSeqSubsystem::InitLevelSequence()
{
	ScanForLevelSequence();
	if (ActiveLevelSequence)
	{
		return;
	}

#if WITH_EDITOR
	FString SequencePackageName = GetSequencePackageName();
	if (SequencePackageName.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("CDG", "InvalidLevelPath", "Cannot create sequence: Level is not saved or path is invalid. Please save the level first."));
		return;
	}

	FString AssetName = FPackageName::GetShortName(SequencePackageName);
	
	if (!SequencePackageName.IsEmpty())
	{
		CreateSequenceAsset(SequencePackageName, AssetName);
	}
#else
	UE_LOG(LogTemp, Warning, TEXT("CDGLevelSeqSubsystem: Cannot create Level Sequence at runtime. This is an Editor-only operation."));
#endif
}

void UCDGLevelSeqSubsystem::DeleteLevelSequence()
{
#if WITH_EDITOR
	FString SequencePackageName = GetSequencePackageName();
	if (SequencePackageName.IsEmpty())
	{
		return;
	}

	TArray<FAssetData> AssetsToDelete;

	// Add master sequence asset.
	if (UObject* MasterAsset = LoadObject<UObject>(nullptr, *SequencePackageName))
	{
		AssetsToDelete.Add(FAssetData(MasterAsset));
	}

	// Add all shot sequence assets that belong to this master sequence.
	const FString MasterPackagePath = FPackageName::GetLongPackagePath(SequencePackageName);
	const FString MasterSequenceName = FPackageName::GetShortName(SequencePackageName);
	const FString ShotNamePrefix = MasterSequenceName + TEXT("_Shot_");

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*MasterPackagePath));
	Filter.bRecursivePaths = false;
	Filter.ClassPaths.Add(ULevelSequence::StaticClass()->GetClassPathName());

	TArray<FAssetData> LevelSequenceAssets;
	AssetRegistryModule.Get().GetAssets(Filter, LevelSequenceAssets);

	for (const FAssetData& AssetData : LevelSequenceAssets)
	{
		const FString AssetName = AssetData.AssetName.ToString();
		if (AssetName.StartsWith(ShotNamePrefix))
		{
			AssetsToDelete.Add(AssetData);
		}
	}

	if (AssetsToDelete.Num() > 0)
	{
		// Delete master sequence and its child shot sequences together.
		ObjectTools::DeleteAssets(AssetsToDelete, false);
	}

	ActiveLevelSequence = nullptr;
#else
	UE_LOG(LogTemp, Warning, TEXT("CDGLevelSeqSubsystem: Cannot delete Level Sequence at runtime. This is an Editor-only operation."));
#endif
}

UCDGTrajectorySubsystem* UCDGLevelSeqSubsystem::GetTrajectorySubsystem() const
{
	if (GetWorld())
	{
		return GetWorld()->GetSubsystem<UCDGTrajectorySubsystem>();
	}
	return nullptr;
}

FString UCDGLevelSeqSubsystem::GetSequencePackageName() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return TEXT("");
	}
	
	// Get current level package
	ULevel* Level = World->GetCurrentLevel();
	if (!Level)
	{
		return TEXT("");
	}

	UPackage* LevelPackage = Level->GetOutermost();
	FString LevelPackageName = LevelPackage->GetName();
	
	// Handle PIE prefix stripping to get the persistent level name
	LevelPackageName = World->RemovePIEPrefix(LevelPackageName);

	// Check for unsaved/temp levels (common cause of "Path does not start with valid root")
	if (LevelPackageName.StartsWith(TEXT("/Temp/")) || 
		LevelPackageName.StartsWith(TEXT("/None")) || 
		LevelPackageName.IsEmpty() ||
		LevelPackageName.Contains(TEXT("Untitled")))
	{
		return TEXT("");
	}
	
	FString PackagePath = FPackageName::GetLongPackagePath(LevelPackageName);
	FString LevelName = FPackageName::GetShortName(LevelPackageName);
	
	// Naming: CDG_<LevelName>_SEQ
	FString SequenceName = FString::Printf(TEXT("CDG_%s_SEQ"), *LevelName);
	
	return PackagePath / SequenceName;
}

#if WITH_EDITOR
ULevelSequence* UCDGLevelSeqSubsystem::CreateSequenceAsset(const FString& PackageName, const FString& AssetName)
{
	// Use FindObject / LoadObject / NewObject so we never show an "Overwrite?" dialog.
	// This is the same pattern as ForceGetOrCreateLevelSequence in the batch processor.
	const FString ObjectPath = PackageName + TEXT(".") + AssetName;

	ActiveLevelSequence = FindObject<ULevelSequence>(nullptr, *ObjectPath);
	// Guard: garbage-marked objects from a previous combo may still be in memory.
	if (ActiveLevelSequence && !IsValid(ActiveLevelSequence)) ActiveLevelSequence = nullptr;

	if (!ActiveLevelSequence)
	{
		ActiveLevelSequence = LoadObject<ULevelSequence>(nullptr, *ObjectPath);
		if (ActiveLevelSequence && !IsValid(ActiveLevelSequence)) ActiveLevelSequence = nullptr;
	}

	if (ActiveLevelSequence)
	{
		UE_LOG(LogTemp, Log, TEXT("CDGLevelSeqSubsystem: Reusing existing Level Sequence: %s"), *PackageName);
		if (!ActiveLevelSequence->GetMovieScene())
			ActiveLevelSequence->Initialize();
		return ActiveLevelSequence;
	}

	// Not found anywhere — create a fresh one without going through AssetTools.
	// If a garbage-marked package with this name is still in memory (not yet GC'd),
	// rename it so CreatePackage produces a genuinely new package.
	if (UPackage* OldPkg = FindObject<UPackage>(nullptr, *PackageName))
	{
		if (!IsValid(OldPkg))
		{
			OldPkg->Rename(nullptr, nullptr,
				REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
		}
	}

	UPackage* Pkg = CreatePackage(*PackageName);
	if (!Pkg)
	{
		UE_LOG(LogTemp, Error, TEXT("CDGLevelSeqSubsystem: Failed to create package for: %s"), *PackageName);
		return nullptr;
	}

	ActiveLevelSequence = NewObject<ULevelSequence>(Pkg, *AssetName, RF_Public | RF_Standalone);
	if (!ActiveLevelSequence)
	{
		UE_LOG(LogTemp, Error, TEXT("CDGLevelSeqSubsystem: Failed to create Level Sequence: %s"), *PackageName);
		return nullptr;
	}

	ActiveLevelSequence->Initialize();

	FAssetRegistryModule::AssetCreated(ActiveLevelSequence);

	UE_LOG(LogTemp, Log, TEXT("CDGLevelSeqSubsystem: Created new Level Sequence: %s"), *PackageName);
	return ActiveLevelSequence;
}
#endif
