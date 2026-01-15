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
#include "AssetToolsModule.h"
#include "IAssetTools.h"
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

	UObject* Asset = LoadObject<UObject>(nullptr, *SequencePackageName);
	
	if (Asset)
	{
		// Use ObjectTools to properly delete and clean up references
		TArray<FAssetData> AssetsToDelete;
		if (Asset)
		{
			AssetsToDelete.Add(FAssetData(Asset));
		}
		
		// Pass false to suppress confirmation dialog if desired, or true to be safe
		// Here we default to true (interactive) or false (force) depending on requirements.
		// Assuming we want it to just happen if called from code.
		ObjectTools::DeleteAssets(AssetsToDelete, false); 
		
		ActiveLevelSequence = nullptr;
	}
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
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	// Create the asset using AssetTools. This will find the appropriate factory (ULevelSequenceFactoryNew).
	// Passing nullptr for Factory causes it to pick the default one.
	UObject* NewAsset = AssetTools.CreateAsset(
		AssetName, 
		FPackageName::GetLongPackagePath(PackageName), 
		ULevelSequence::StaticClass(), 
		nullptr
	);
	
	ActiveLevelSequence = Cast<ULevelSequence>(NewAsset);
	
	if (ActiveLevelSequence)
	{
		UE_LOG(LogTemp, Log, TEXT("CDGLevelSeqSubsystem: Created new Level Sequence: %s"), *PackageName);
		
		// Initialize the sequence with a MovieScene
		if (ActiveLevelSequence->GetMovieScene() == nullptr)
		{
			ActiveLevelSequence->Initialize();
		}

		// Auto-save the newly created asset
		UPackage* Package = ActiveLevelSequence->GetOutermost();
		if (Package)
		{
			FEditorFileUtils::PromptForCheckoutAndSave({ Package }, false, false);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CDGLevelSeqSubsystem: Failed to create Level Sequence: %s"), *PackageName);
	}

	return ActiveLevelSequence;
}
#endif
