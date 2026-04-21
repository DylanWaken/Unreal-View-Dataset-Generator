// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BatchProcEditor/CDGBatchProcExecService.h"
#include "Config/GeneratorStackConfig.h"
#include "Config/LevelSeqExportConfig.h"
#include "Generator/CDGTrajectoryGenerator.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LevelSequenceInterface/CDGLevelSeqSubsystem.h"
#include "MRQInterface/CDGMRQInterface.h"
#include "IO/TrajectorySL.h"
#include "Anchor/CDGLevelSceneAnchor.h"
#include "Anchor/CDGCharacterAnchor.h"
#include "LogCameraDatasetGenEditor.h"

// Engine
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequenceBase.h"

// Asset tools
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Factories/Factory.h"

// Level Sequence
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"

// Serialisation / JSON
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// File system
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

// Notifications
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "CDGBatchProcExecService"

static constexpr double kTickResolution = 24000.0;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
	/** Find a ULevelSequence factory without pulling in a specific header. */
	UFactory* FindLevelSequenceFactory()
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* C = *It;
			if (!C->IsChildOf(UFactory::StaticClass())) continue;
			if (C->HasAnyClassFlags(CLASS_Abstract)) continue;
			if (UFactory* F = Cast<UFactory>(C->GetDefaultObject()))
			{
				if (F->CanCreateNew() && F->SupportedClass == ULevelSequence::StaticClass())
					return F;
			}
		}
		return nullptr;
	}

	/** Sanitise a string for use as a folder/file name component. */
	FString Sanitise(const FString& In)
	{
		FString Out = In;
		const TCHAR BadChars[] = TEXT(" /\\:*?\"<>|");
		for (TCHAR C : TArrayView<const TCHAR>(BadChars, UE_ARRAY_COUNT(BadChars) - 1))
		{
			Out.ReplaceCharInline(C, TEXT('_'));
		}
		return Out;
	}

	/** Delete a UObject asset from disk. */
	void DeleteAssetByPackageName(const FString& PackageName)
	{
#if WITH_EDITOR
		const FString ShortName   = FPackageName::GetShortName(PackageName);
		const FString ObjectPath  = PackageName + TEXT(".") + ShortName;

		// Try to find it in memory first; if not found, the file cleanup still runs.
		UObject* Asset = FindObject<UObject>(nullptr, *ObjectPath);

		UPackage* Package = nullptr;
		if (Asset)
		{
			Package = Asset->GetOutermost();
			if (Asset->IsRooted()) Asset->RemoveFromRoot();
			Asset->MarkAsGarbage();
		}

		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			if (Package) Package->MarkAsGarbage();
			IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
			PF.DeleteFile(*PackageFilename);
		}
#endif
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Create / Start / Cancel
// ─────────────────────────────────────────────────────────────────────────────

UCDGBatchProcExecService* UCDGBatchProcExecService::Create(UObject* Outer, const FBatchProcInput& InInput)
{
	UCDGBatchProcExecService* Service = NewObject<UCDGBatchProcExecService>(Outer ? Outer : GetTransientPackage());
	Service->Input = InInput;
	return Service;
}

void UCDGBatchProcExecService::Start()
{
	if (bStarted)
	{
		UE_LOG(LogCameraDatasetGenEditor, Warning, TEXT("[BatchExec] Start() called more than once — ignored"));
		return;
	}
	bStarted      = true;
	bIsRunning    = true;
	bCancelled    = false;
	TotalCombos   = ComputeTotalCombos();
	CompletedCombos = 0;

	UE_LOG(LogCameraDatasetGenEditor, Log,
		TEXT("[BatchExec] Starting batch: %d level(s), combinatorial total ≈ %d"),
		Input.Levels.Num(), TotalCombos);

	BroadcastLog(FString::Printf(TEXT("Starting batch — %d level(s)"), Input.Levels.Num()));

	CurrentLevelIdx = 0;
	BeginProcessLevel();
}

void UCDGBatchProcExecService::Cancel()
{
	bCancelled = true;
	BroadcastLog(TEXT("Cancellation requested — will stop after current render."));
}

// ─────────────────────────────────────────────────────────────────────────────
// Level processing
// ─────────────────────────────────────────────────────────────────────────────

void UCDGBatchProcExecService::BeginProcessLevel()
{
	if (bCancelled || CurrentLevelIdx >= Input.Levels.Num())
	{
		Finish(!bCancelled);
		return;
	}

	const FAssetData& LevelAsset = Input.Levels[CurrentLevelIdx];
	const FString PackageName    = LevelAsset.PackageName.ToString();

	BroadcastLog(FString::Printf(TEXT("Opening level %d/%d: %s"),
		CurrentLevelIdx + 1, Input.Levels.Num(), *FPackageName::GetShortName(PackageName)));

	// Check whether this level is already loaded
	UWorld* CurrentWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	const bool bAlreadyLoaded = CurrentWorld &&
		CurrentWorld->GetOutermost()->GetFName() == LevelAsset.PackageName;

	if (!bAlreadyLoaded)
	{
		FString Filename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetMapPackageExtension()))
		{
			BroadcastLog(FString::Printf(TEXT("  ERROR: Cannot resolve filename for %s — skipping"), *PackageName));
			++CurrentLevelIdx;
			BeginProcessLevel();
			return;
		}

		// Synchronous level load — blocks the editor while loading
		FEditorFileUtils::LoadMap(Filename, /*bLoadAsTemplate=*/false, /*bShowProgress=*/true);
	}

	DiscoverAnchorsAndBeginCombos();
}

void UCDGBatchProcExecService::DiscoverAnchorsAndBeginCombos()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		BroadcastLog(TEXT("  ERROR: No valid editor world after level load — skipping."));
		++CurrentLevelIdx;
		BeginProcessLevel();
		return;
	}

	CurrentAnchors.Empty();
	for (ACDGLevelSceneAnchor* Anchor : GetAnchors(World))
	{
		CurrentAnchors.Add(Anchor);
	}

	if (CurrentAnchors.IsEmpty())
	{
		BroadcastLog(FString::Printf(TEXT("  WARNING: No ACDGLevelSceneAnchor found in %s — skipping level."),
			*World->GetMapName()));
		++CurrentLevelIdx;
		BeginProcessLevel();
		return;
	}

	BroadcastLog(FString::Printf(TEXT("  Found %d anchor(s) in %s"),
		CurrentAnchors.Num(), *World->GetMapName()));

	CurrentAnchorIdx    = 0;
	CurrentCharacterIdx = 0;
	CurrentAnimIdx      = 0;

	BeginNextCombo();
}

// ─────────────────────────────────────────────────────────────────────────────
// Combo loop
// ─────────────────────────────────────────────────────────────────────────────

void UCDGBatchProcExecService::BeginNextCombo()
{
	if (bCancelled)
	{
		Finish(false);
		return;
	}

	if (CurrentAnchorIdx >= CurrentAnchors.Num())
	{
		// All anchors done for this level — move to next level
		++CurrentLevelIdx;
		BeginProcessLevel();
		return;
	}

	OnProgressUpdated.Broadcast(CompletedCombos, TotalCombos);
	ExecuteCurrentCombo();
}

void UCDGBatchProcExecService::ExecuteCurrentCombo()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		BroadcastLog(TEXT("  ERROR: World became invalid — aborting."));
		Finish(false);
		return;
	}

	// ── Gather combo elements ─────────────────────────────────────────────────
	ACDGLevelSceneAnchor* Anchor = CurrentAnchors.IsValidIndex(CurrentAnchorIdx) ?
		CurrentAnchors[CurrentAnchorIdx].Get() : nullptr;

	if (!Anchor)
	{
		BroadcastLog(TEXT("  WARNING: Anchor became invalid — skipping."));
		if (!AdvanceComboIndices()) { ++CurrentLevelIdx; BeginProcessLevel(); }
		else BeginNextCombo();
		return;
	}

	const FAssetData& CharAsset = Input.Characters[CurrentCharacterIdx];
	const FAssetData& AnimAsset = Input.Animations[CurrentAnimIdx];

	// ── Build combo key ───────────────────────────────────────────────────────
	const FString LevelShortName = Sanitise(FPackageName::GetShortName(
		Input.Levels[CurrentLevelIdx].PackageName.ToString()));
	const FString AnchorName     = Sanitise(Anchor->GetActorNameOrLabel());
	const FString CharShortName  = Sanitise(FPackageName::GetShortName(CharAsset.PackageName.ToString()));
	const FString AnimShortName  = Sanitise(FPackageName::GetShortName(AnimAsset.PackageName.ToString()));

	const FString ComboKey = MakeComboKey(LevelShortName, AnchorName, CharShortName, AnimShortName);

	BroadcastLog(FString::Printf(TEXT("  Combo [%d/%d]: %s"), CompletedCombos + 1, TotalCombos, *ComboKey));

	// ── Resolve assets ────────────────────────────────────────────────────────
	UBlueprint* CharBP = Cast<UBlueprint>(CharAsset.GetAsset());
	if (!CharBP || !CharBP->GeneratedClass)
	{
		BroadcastLog(FString::Printf(TEXT("    ERROR: Cannot load character blueprint %s — skipping."), *CharShortName));
		if (!AdvanceComboIndices()) { ++CurrentLevelIdx; BeginProcessLevel(); }
		else BeginNextCombo();
		return;
	}

	UAnimationAsset* Anim = Cast<UAnimationAsset>(AnimAsset.GetAsset());
	if (!Anim)
	{
		BroadcastLog(FString::Printf(TEXT("    ERROR: Cannot load animation %s — skipping."), *AnimShortName));
		if (!AdvanceComboIndices()) { ++CurrentLevelIdx; BeginProcessLevel(); }
		else BeginNextCombo();
		return;
	}

	const int32 FPS = Input.ExporterConfig.IsValid() ? Input.ExporterConfig->FPS : 30;

	// ── 1. Spawn character ────────────────────────────────────────────────────
	AActor* Character = SpawnCharacterAtAnchor(World, Anchor, CharBP->GeneratedClass);
	if (!Character)
	{
		BroadcastLog(FString::Printf(TEXT("    ERROR: Failed to spawn character — skipping.")));
		if (!AdvanceComboIndices()) { ++CurrentLevelIdx; BeginProcessLevel(); }
		else BeginNextCombo();
		return;
	}
	SpawnedCharacter = Character;
	BroadcastLog(FString::Printf(TEXT("    Spawned character: %s"), *Character->GetActorNameOrLabel()));

	// ── 2. Create reference sequence ─────────────────────────────────────────
	ULevelSequence* RefSeq = CreateReferenceSequence(World, Character, Anim, ComboKey, FPS);
	if (!RefSeq)
	{
		BroadcastLog(TEXT("    ERROR: Failed to create reference sequence — skipping."));
		DestroySpawnedCharacter(World);
		if (!AdvanceComboIndices()) { ++CurrentLevelIdx; BeginProcessLevel(); }
		else BeginNextCombo();
		return;
	}
	CurrentRefSequence = RefSeq;
	BroadcastLog(FString::Printf(TEXT("    Reference sequence created: %s"), *RefSeq->GetPathName()));

	// ── 3. Instantiate generators and set reference ───────────────────────────
	InstantiateGenerators(World, RefSeq, Character);
	if (Generators.IsEmpty())
	{
		BroadcastLog(TEXT("    ERROR: No generators could be instantiated — skipping."));
		DeleteReferenceSequence();
		DestroySpawnedCharacter(World);
		if (!AdvanceComboIndices()) { ++CurrentLevelIdx; BeginProcessLevel(); }
		else BeginNextCombo();
		return;
	}

	// ── 4. Run generators ─────────────────────────────────────────────────────
	TArray<ACDGTrajectory*> Trajectories = RunGenerators(World);
	if (Trajectories.IsEmpty())
	{
		BroadcastLog(TEXT("    WARNING: Generators produced no trajectories — skipping."));
		DestroyGenerators();
		DeleteReferenceSequence();
		DestroySpawnedCharacter(World);
		if (!AdvanceComboIndices()) { ++CurrentLevelIdx; BeginProcessLevel(); }
		else BeginNextCombo();
		return;
	}
	BroadcastLog(FString::Printf(TEXT("    Generated %d trajectory/ies."), Trajectories.Num()));

	// ── 5. Export trajectories to level sequence ──────────────────────────────
	ULevelSequence* MasterSeq = ExportTrajectoriesAsLevelSequence(World, RefSeq, Trajectories, FPS);
	if (!MasterSeq)
	{
		BroadcastLog(TEXT("    ERROR: Level sequence export failed — skipping."));
		CleanupComboAssets(World);
		DestroyGenerators();
		DeleteReferenceSequence();
		DestroySpawnedCharacter(World);
		if (!AdvanceComboIndices()) { ++CurrentLevelIdx; BeginProcessLevel(); }
		else BeginNextCombo();
		return;
	}

	// ── 6. Build render config ────────────────────────────────────────────────
	const FString RootOutputDir = Input.ExporterConfig.IsValid()
		? Input.ExporterConfig->OutputDirectory
		: FPaths::ProjectSavedDir() / TEXT("BatchProcOutput");

	FTrajectoryRenderConfig RenderConfig;
	RenderConfig.DestinationRootDir      = RootOutputDir;
	RenderConfig.LevelNameOverride       = ComboKey;   // output → <RootDir>/<ComboKey>/OUTPUTS/
	RenderConfig.bExportIndexJSON        = false;      // we write our own named JSON
	RenderConfig.bOverwriteExistingOutput = Input.ExporterConfig.IsValid()
		? Input.ExporterConfig->bOverwriteExisting : false;
	RenderConfig.OutputResolutionOverride = FIntPoint(
		Input.ExporterConfig.IsValid() ? Input.ExporterConfig->ResolutionWidth  : 1920,
		Input.ExporterConfig.IsValid() ? Input.ExporterConfig->ResolutionHeight : 1080);
	RenderConfig.OutputFramerateOverride  = FPS;
	RenderConfig.ExportFormat             = Input.ExporterConfig.IsValid()
		? Input.ExporterConfig->ExportFormat : ECDGRenderOutputFormat::PNG_Sequence;
	RenderConfig.SpatialSampleCount       = Input.ExporterConfig.IsValid()
		? Input.ExporterConfig->SpatialSampleCount  : 1;
	RenderConfig.TemporalSampleCount      = Input.ExporterConfig.IsValid()
		? Input.ExporterConfig->TemporalSampleCount : 1;

	// ── 7. Start render (async — continuation fires in lambda) ────────────────
	// Capture everything by value / weak-ptr
	const FString ComboOutputDir = FPaths::Combine(RootOutputDir, ComboKey);
	const int32   FPSCapture     = FPS;

	TWeakObjectPtr<UCDGBatchProcExecService> WeakThis = this;

	bool bRenderStarted = CDGMRQInterface::RenderTrajectoriesWithSequence(
		MasterSeq, Trajectories, RenderConfig,
		[WeakThis, ComboOutputDir, ComboKey, FPSCapture](bool bSuccess)
		{
			if (!WeakThis.IsValid()) return;
			UCDGBatchProcExecService* Self = WeakThis.Get();

			Self->BroadcastLog(FString::Printf(
				TEXT("    Render %s: %s"),
				bSuccess ? TEXT("completed") : TEXT("FAILED"),
				*ComboKey));

			UWorld* W = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

			// 7a. Write combo index JSON
			if (bSuccess && W)
			{
				Self->WriteComboIndexJson(ComboOutputDir, ComboKey, FPSCapture);
			}

			// 7b. Clean up
			Self->CleanupComboAssets(W);
			Self->DestroyGenerators();
			Self->DeleteReferenceSequence();
			Self->DestroySpawnedCharacter(W);

			// 7c. Advance to next combo
			++Self->CompletedCombos;
			Self->OnProgressUpdated.Broadcast(Self->CompletedCombos, Self->TotalCombos);

			if (!Self->AdvanceComboIndices())
			{
				++Self->CurrentLevelIdx;
				Self->BeginProcessLevel();
			}
			else
			{
				Self->BeginNextCombo();
			}
		});

	if (!bRenderStarted)
	{
		BroadcastLog(TEXT("    ERROR: Failed to start MRQ render — cleaning up and skipping."));
		CleanupComboAssets(World);
		DestroyGenerators();
		DeleteReferenceSequence();
		DestroySpawnedCharacter(World);
		if (!AdvanceComboIndices()) { ++CurrentLevelIdx; BeginProcessLevel(); }
		else BeginNextCombo();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-combo steps
// ─────────────────────────────────────────────────────────────────────────────

AActor* UCDGBatchProcExecService::SpawnCharacterAtAnchor(
	UWorld* World,
	ACDGLevelSceneAnchor* Anchor,
	UClass* CharacterClass)
{
	if (!World || !Anchor || !CharacterClass) return nullptr;

	// Random XY offset within the dispersion disc; Z comes from the anchor itself.
	FVector SpawnXY = FVector(Anchor->GetActorLocation());
	if (Anchor->DispersionRadius > KINDA_SMALL_NUMBER)
	{
		const float Angle = FMath::FRand() * 2.f * PI;
		const float Dist  = FMath::Sqrt(FMath::FRand()) * Anchor->DispersionRadius;
		SpawnXY.X += FMath::Cos(Angle) * Dist;
		SpawnXY.Y += FMath::Sin(Angle) * Dist;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Character = World->SpawnActor<AActor>(CharacterClass, SpawnXY, FRotator::ZeroRotator, Params);
	if (!Character) return nullptr;

	// Actor roots are often at the capsule centre, not the feet.
	// Measure how far the root sits above the bounding-box bottom, then shift
	// the actor up so that its bottom aligns with the anchor's Z.
	const FBox Bounds = Character->GetComponentsBoundingBox(/*bNonColliding=*/true);
	if (Bounds.IsValid)
	{
		const float RootToBottom = Character->GetActorLocation().Z - Bounds.Min.Z;
		Character->SetActorLocation(FVector(SpawnXY.X, SpawnXY.Y, Anchor->GetActorLocation().Z + RootToBottom));
	}

	return Character;
}

ULevelSequence* UCDGBatchProcExecService::CreateReferenceSequence(
	UWorld* World,
	AActor* Character,
	UAnimationAsset* Anim,
	const FString& ComboKey,
	int32 FPS)
{
	if (!World || !Character || !Anim) return nullptr;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UFactory* Factory = FindLevelSequenceFactory();
	if (!Factory) return nullptr;

	const FString SeqName       = TEXT("CDGBatchRefSeq_") + ComboKey;
	const FString SeqPkgPath    = TEXT("/Game/CDGBatch_Temp");

	// Create or re-use existing asset at this path
	ULevelSequence* RefSeq = Cast<ULevelSequence>(
		AssetTools.CreateAsset(SeqName, SeqPkgPath, ULevelSequence::StaticClass(), Factory));
	if (!RefSeq) return nullptr;

	UMovieScene* MS = RefSeq->GetMovieScene();
	if (!MS) return nullptr;

	// ── Duration from animation ───────────────────────────────────────────────
	float AnimDuration = 1.0f;
	if (UAnimSequenceBase* AnimBase = Cast<UAnimSequenceBase>(Anim))
	{
		AnimDuration = FMath::Max(0.1f, AnimBase->GetPlayLength());
	}

	const int32 DurationTicks = FMath::Max(1,
		FMath::RoundToInt(AnimDuration * kTickResolution));

	RefSeq->Modify();
	MS->Modify();
	MS->SetDisplayRate(FFrameRate(FPS, 1));
	MS->SetTickResolutionDirectly(FFrameRate(kTickResolution, 1));
	MS->SetPlaybackRange(TRange<FFrameNumber>(0, DurationTicks));

	// ── Bind character actor ──────────────────────────────────────────────────
	FGuid CharGuid = MS->AddPossessable(Character->GetActorNameOrLabel(), Character->GetClass());
	RefSeq->BindPossessableObject(CharGuid, *Character, World);

	// ── Add skeletal-animation track on each SkeletalMeshComponent ───────────
	TArray<USkeletalMeshComponent*> SkelComps;
	Character->GetComponents<USkeletalMeshComponent>(SkelComps);

	for (USkeletalMeshComponent* SkelComp : SkelComps)
	{
		FGuid SkelGuid = MS->AddPossessable(SkelComp->GetName(), SkelComp->GetClass());
		if (FMovieScenePossessable* SkelPossessable = MS->FindPossessable(SkelGuid))
		{
			SkelPossessable->SetParent(CharGuid, MS);
		}
		RefSeq->BindPossessableObject(SkelGuid, *SkelComp, Character);

		UMovieSceneSkeletalAnimationTrack* AnimTrack =
			MS->AddTrack<UMovieSceneSkeletalAnimationTrack>(SkelGuid);
		if (AnimTrack)
		{
			UMovieSceneSkeletalAnimationSection* AnimSection =
				Cast<UMovieSceneSkeletalAnimationSection>(AnimTrack->CreateNewSection());
			if (AnimSection)
			{
				// Assign directly to avoid using the out-of-line constructor of FMovieSceneSkeletalAnimationParams
				AnimSection->Params.Animation = Cast<UAnimSequenceBase>(Anim);
				AnimSection->SetRange(TRange<FFrameNumber>(0, DurationTicks));
				AnimTrack->AddSection(*AnimSection);
			}
		}
	}

	RefSeq->MarkPackageDirty();

	// Persist so CDGLevelSeqSubsystem / shot sequences can reference it
	UPackage* Pkg = RefSeq->GetOutermost();
	const FString PkgFilename = FPackageName::LongPackageNameToFilename(
		Pkg->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Pkg, RefSeq, *PkgFilename, SaveArgs);

	return RefSeq;
}

void UCDGBatchProcExecService::InstantiateGenerators(
	UWorld* World,
	ULevelSequence* RefSeq,
	AActor* Character)
{
	DestroyGenerators();

	if (!Input.GeneratorConfig.IsValid() || Input.GeneratorConfig->GeneratorsJson.IsEmpty())
	{
		BroadcastLog(TEXT("    WARNING: GeneratorStackConfig is empty or invalid."));
		return;
	}

	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Input.GeneratorConfig->GeneratorsJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid()) return;

	const TArray<TSharedPtr<FJsonValue>>* GensArray = nullptr;
	if (!RootObj->TryGetArrayField(TEXT("generators"), GensArray) || !GensArray) return;

	for (const TSharedPtr<FJsonValue>& GenVal : *GensArray)
	{
		const TSharedPtr<FJsonObject>* GenObjPtr = nullptr;
		if (!GenVal->TryGetObject(GenObjPtr) || !GenObjPtr) continue;

		FString ClassName;
		if (!(*GenObjPtr)->TryGetStringField(TEXT("class"), ClassName)) continue;

		UClass* GenClass = FindObject<UClass>(nullptr, *ClassName);
		if (!GenClass) continue;

		UCDGTrajectoryGenerator* Gen = NewObject<UCDGTrajectoryGenerator>(World, GenClass);
		if (!Gen) continue;

		Gen->AddToRoot();

		// Restore config
		const TSharedPtr<FJsonObject>* CfgObjPtr = nullptr;
		if ((*GenObjPtr)->TryGetObjectField(TEXT("config"), CfgObjPtr) && CfgObjPtr)
		{
			Gen->FetchGeneratorConfig(*CfgObjPtr);
		}

		// Inject reference sequence + character actor
		Gen->ReferenceSequence    = RefSeq;
		Gen->PrimaryCharacterActor = Character;

		// If generator has bLetBatchProcessorFill set on the config asset, ensure
		// the FocusedAnchor is left at its default — no override needed here.

		Generators.Add(Gen);
	}

	BroadcastLog(FString::Printf(TEXT("    Instantiated %d generator(s)."), Generators.Num()));
}

TArray<ACDGTrajectory*> UCDGBatchProcExecService::RunGenerators(UWorld* World)
{
	TArray<ACDGTrajectory*> AllCreated;

	for (UCDGTrajectoryGenerator* Gen : Generators)
	{
		if (!Gen) continue;

		// Ensure the generator is outered to the current editor world
		if (Gen->GetOuter() != World)
		{
			Gen->Rename(nullptr, World,
				REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		}

		TArray<ACDGTrajectory*> Created = Gen->Generate();
		AllCreated.Append(Created);

		UE_LOG(LogCameraDatasetGenEditor, Log,
			TEXT("[BatchExec] %s produced %d trajectory/ies"),
			*Gen->GetGeneratorName().ToString(), Created.Num());
	}

	return AllCreated;
}

ULevelSequence* UCDGBatchProcExecService::ExportTrajectoriesAsLevelSequence(
	UWorld* World,
	ULevelSequence* RefSeq,
	const TArray<ACDGTrajectory*>& Trajectories,
	int32 FPS)
{
	if (!World || !RefSeq || Trajectories.IsEmpty()) return nullptr;

	UCDGLevelSeqSubsystem* LevelSeqSys = World->GetSubsystem<UCDGLevelSeqSubsystem>();
	if (!LevelSeqSys) return nullptr;

	// Get / create the master sequence
	LevelSeqSys->InitLevelSequence();
	ULevelSequence* MasterSeq = LevelSeqSys->GetActiveLevelSequence();
	if (!MasterSeq) return nullptr;

	const FFrameRate FrameRate(FPS, 1);
	UMovieScene* MasterMS = MasterSeq->GetMovieScene();
	MasterMS->SetDisplayRate(FrameRate);
	MasterMS->SetTickResolutionDirectly(FFrameRate(kTickResolution, 1));

	// Clear master's existing tracks
	for (UMovieSceneTrack* T : MasterMS->GetTracks()) MasterMS->RemoveTrack(*T);
	for (int32 i = MasterMS->GetSpawnableCount() - 1; i >= 0; --i)
		MasterMS->RemoveSpawnable(MasterMS->GetSpawnable(i).GetGuid());
	for (int32 i = MasterMS->GetPossessableCount() - 1; i >= 0; --i)
		MasterMS->RemovePossessable(MasterMS->GetPossessable(i).GetGuid());

	UMovieSceneCinematicShotTrack* ShotTrack = MasterMS->AddTrack<UMovieSceneCinematicShotTrack>();
	if (!ShotTrack) return nullptr;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	const FString MasterPackageName = MasterSeq->GetOutermost()->GetName();
	const FString MasterPackagePath = FPackageName::GetLongPackagePath(MasterPackageName);
	const FString MasterSeqShortName = FPackageName::GetShortName(MasterPackageName);

	UFactory* Factory = FindLevelSequenceFactory();
	if (!Factory) return nullptr;

	CreatedShotSequencePaths.Empty();
	FFrameNumber StartFrame = 0;

	for (ACDGTrajectory* Trajectory : Trajectories)
	{
		if (!Trajectory) continue;

		const double TrajDuration = Trajectory->GetTrajectoryDuration();
		const int32  DurationTicks = FMath::Max(1,
			FMath::RoundToInt(TrajDuration * kTickResolution));

		// ── Create (or reuse) shot sequence ──────────────────────────────────
		const FString ShotName = FString::Printf(TEXT("%s_Shot_%s"),
			*MasterSeqShortName, *Trajectory->TrajectoryName.ToString());
		const FString ShotPackageName = MasterPackagePath / ShotName;

		// Delete stale shot if it exists
		DeleteAssetByPackageName(ShotPackageName);

		ULevelSequence* ShotSeq = Cast<ULevelSequence>(
			AssetTools.CreateAsset(ShotName, MasterPackagePath, ULevelSequence::StaticClass(), Factory));
		if (!ShotSeq) continue;

		UMovieScene* ShotMS = ShotSeq->GetMovieScene();
		ShotSeq->Modify();
		ShotMS->Modify();
		ShotMS->SetDisplayRate(FrameRate);
		ShotMS->SetTickResolutionDirectly(FFrameRate(kTickResolution, 1));

		// ── Camera possessable + transform track ─────────────────────────────
		const FString CameraName = FString::Printf(TEXT("Cam_%s"), *Trajectory->TrajectoryName.ToString());

		// Destroy leftover cameras
		for (TActorIterator<ACineCameraActor> It(World); It; ++It)
		{
			if (It->GetActorLabel() == CameraName)
			{
				World->EditorDestroyActor(*It, true);
				break;
			}
		}

		FActorSpawnParameters CamSpawnParams;
		ACineCameraActor* CameraActor = World->SpawnActor<ACineCameraActor>(
			ACineCameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, CamSpawnParams);
		if (!CameraActor) continue;
		CameraActor->SetActorLabel(CameraName);

		FGuid CameraGuid = ShotMS->AddPossessable(CameraActor->GetActorLabel(), CameraActor->GetClass());
		ShotSeq->BindPossessableObject(CameraGuid, *CameraActor, World);

		// Camera cut track
		if (UMovieSceneCameraCutTrack* CutTrack = ShotMS->AddTrack<UMovieSceneCameraCutTrack>())
		{
			if (UMovieSceneCameraCutSection* CutSec =
				Cast<UMovieSceneCameraCutSection>(CutTrack->CreateNewSection()))
			{
				CutSec->SetRange(TRange<FFrameNumber>(0, DurationTicks));
				CutSec->SetCameraGuid(CameraGuid);
				CutTrack->AddSection(*CutSec);
			}
		}

		// Transform track — bake from trajectory keyframes
		if (UMovieScene3DTransformTrack* TformTrack =
			ShotMS->AddTrack<UMovieScene3DTransformTrack>(CameraGuid))
		{
			if (UMovieScene3DTransformSection* TformSec =
				Cast<UMovieScene3DTransformSection>(TformTrack->CreateNewSection()))
			{
				TformTrack->AddSection(*TformSec);
				TformSec->SetRange(TRange<FFrameNumber>(0, DurationTicks));

				FMovieSceneChannelProxy& ChProxy = TformSec->GetChannelProxy();
				TArrayView<FMovieSceneDoubleChannel*> DoubleChans =
					ChProxy.GetChannels<FMovieSceneDoubleChannel>();

				if (DoubleChans.Num() >= 6)
				{
					TArray<ACDGKeyframe*> KFs = Trajectory->GetSortedKeyframes();
					double CurTime = 0.0;

					auto AddKeyToChannel = [](FMovieSceneDoubleChannel* Ch, FFrameNumber T,
					                          double V, ECDGInterpolationMode Mode)
					{
						if (!Ch) return;
						ERichCurveInterpMode IM; ERichCurveTangentMode TM;
						switch (Mode)
						{
						case ECDGInterpolationMode::Linear:   IM = RCIM_Linear;   TM = RCTM_Auto; break;
						case ECDGInterpolationMode::Constant: IM = RCIM_Constant; TM = RCTM_Auto; break;
						default:                              IM = RCIM_Cubic;    TM = RCTM_Auto; break;
						}
						if (IM == RCIM_Constant)     Ch->AddConstantKey(T, V);
						else if (IM == RCIM_Linear)  Ch->AddLinearKey(T, V);
						else                         Ch->AddCubicKey(T, V, TM);
					};

					for (int32 k = 0; k < KFs.Num(); ++k)
					{
						ACDGKeyframe* KF = KFs[k];
						if (!KF) continue;
						if (k > 0) CurTime += KF->TimeToCurrentFrame;

						const FFrameNumber KT = FFrameNumber(
							static_cast<int32>(CurTime * kTickResolution));
						const FTransform Xform = KF->GetKeyframeTransform();
						const FVector  Loc     = Xform.GetLocation();
						const FRotator Rot     = Xform.GetRotation().Rotator();
						const bool bStay       = KF->TimeAtCurrentFrame > KINDA_SMALL_NUMBER;

						const ECDGInterpolationMode PM = bStay ?
							ECDGInterpolationMode::Constant : KF->InterpolationSettings.PositionInterpMode;
						const ECDGInterpolationMode RM = bStay ?
							ECDGInterpolationMode::Constant : KF->InterpolationSettings.RotationInterpMode;

						AddKeyToChannel(DoubleChans[0], KT, Loc.X, PM);
						AddKeyToChannel(DoubleChans[1], KT, Loc.Y, PM);
						AddKeyToChannel(DoubleChans[2], KT, Loc.Z, PM);
						AddKeyToChannel(DoubleChans[3], KT, Rot.Roll,  RM);
						AddKeyToChannel(DoubleChans[4], KT, Rot.Pitch, RM);
						AddKeyToChannel(DoubleChans[5], KT, Rot.Yaw,   RM);

						if (bStay)
						{
							CurTime += KF->TimeAtCurrentFrame;
							const FFrameNumber KT2 = FFrameNumber(
								static_cast<int32>(CurTime * kTickResolution));
							AddKeyToChannel(DoubleChans[0], KT2, Loc.X, KF->InterpolationSettings.PositionInterpMode);
							AddKeyToChannel(DoubleChans[1], KT2, Loc.Y, KF->InterpolationSettings.PositionInterpMode);
							AddKeyToChannel(DoubleChans[2], KT2, Loc.Z, KF->InterpolationSettings.PositionInterpMode);
							AddKeyToChannel(DoubleChans[3], KT2, Rot.Roll,  KF->InterpolationSettings.RotationInterpMode);
							AddKeyToChannel(DoubleChans[4], KT2, Rot.Pitch, KF->InterpolationSettings.RotationInterpMode);
							AddKeyToChannel(DoubleChans[5], KT2, Rot.Yaw,   KF->InterpolationSettings.RotationInterpMode);
						}
					}
				}
			}
		}

		// Lens tracks on camera component
		if (UCineCameraComponent* CamComp = CameraActor->GetCineCameraComponent())
		{
			FGuid CompGuid = ShotMS->AddPossessable(CamComp->GetName(), CamComp->GetClass());
			if (FMovieScenePossessable* CP = ShotMS->FindPossessable(CompGuid))
				CP->SetParent(CameraGuid, ShotMS);
			ShotSeq->BindPossessableObject(CompGuid, *CamComp, CameraActor);

			TArray<ACDGKeyframe*> KFs = Trajectory->GetSortedKeyframes();

			auto AddLensTrack = [&](const FName& PropName, const FString& PropPath,
			                        TFunction<float(const ACDGKeyframe*)> ValueGetter)
			{
				UMovieSceneFloatTrack* Track = ShotMS->AddTrack<UMovieSceneFloatTrack>(CompGuid);
				if (!Track) return;
				Track->SetPropertyNameAndPath(PropName, PropPath);
				UMovieSceneFloatSection* Sec = Cast<UMovieSceneFloatSection>(Track->CreateNewSection());
				if (!Sec) return;
				Track->AddSection(*Sec);
				Sec->SetRange(TRange<FFrameNumber>(0, DurationTicks));
				FMovieSceneFloatChannel& Ch = Sec->GetChannel();
				double T = 0.0;
				for (int32 k = 0; k < KFs.Num(); ++k)
				{
					if (!KFs[k]) continue;
					if (k > 0) T += KFs[k]->TimeToCurrentFrame;
					Ch.AddLinearKey(FFrameNumber(static_cast<int32>(T * kTickResolution)),
					                ValueGetter(KFs[k]));
					if (KFs[k]->TimeAtCurrentFrame > KINDA_SMALL_NUMBER)
					{
						T += KFs[k]->TimeAtCurrentFrame;
						Ch.AddLinearKey(FFrameNumber(static_cast<int32>(T * kTickResolution)),
						                ValueGetter(KFs[k]));
					}
				}
			};

			AddLensTrack(
				GET_MEMBER_NAME_CHECKED(UCineCameraComponent, CurrentFocalLength),
				TEXT("CurrentFocalLength"),
				[](const ACDGKeyframe* KF) { return KF->LensSettings.FocalLength; });

			AddLensTrack(
				GET_MEMBER_NAME_CHECKED(UCineCameraComponent, CurrentAperture),
				TEXT("CurrentAperture"),
				[](const ACDGKeyframe* KF) { return KF->LensSettings.Aperture; });
		}

		// ── Copy character animation from ref sequence into shot ──────────────
		// Bind the character possessable (same actor, different binding in this shot)
		if (SpawnedCharacter.IsValid())
		{
			AActor* CharActor = SpawnedCharacter.Get();
			FGuid CopyCharGuid = ShotMS->AddPossessable(
				CharActor->GetActorNameOrLabel(), CharActor->GetClass());
			ShotSeq->BindPossessableObject(CopyCharGuid, *CharActor, World);

			TArray<USkeletalMeshComponent*> SkelComps;
			CharActor->GetComponents<USkeletalMeshComponent>(SkelComps);

			for (USkeletalMeshComponent* SkelComp : SkelComps)
			{
				if (!SkelComp) continue;
				FGuid CopySkelGuid = ShotMS->AddPossessable(SkelComp->GetName(), SkelComp->GetClass());
				if (FMovieScenePossessable* P = ShotMS->FindPossessable(CopySkelGuid))
					P->SetParent(CopyCharGuid, ShotMS);
				ShotSeq->BindPossessableObject(CopySkelGuid, *SkelComp, CharActor);

				// Copy anim track from ref sequence by duplicating
				TArray<UMovieSceneTrack*> RefTracks;
				if (UMovieScene* RefMS = RefSeq->GetMovieScene())
				{
				// Find the skeletal animation tracks on this component in the ref sequence
				const UMovieScene* ConstRefMS = RefMS;
				for (const FMovieSceneBinding& RefBinding : ConstRefMS->GetBindings())
					{
						// Match by component name
						FString BindingName;
						if (const FMovieScenePossessable* Poss = RefMS->FindPossessable(RefBinding.GetObjectGuid()))
							BindingName = Poss->GetName();
						if (BindingName != SkelComp->GetName()) continue;

						for (UMovieSceneTrack* Track : RefBinding.GetTracks())
						{
							if (!Track->IsA<UMovieSceneSkeletalAnimationTrack>()) continue;
							if (UMovieSceneTrack* Dup = DuplicateObject<UMovieSceneTrack>(Track, ShotMS))
							{
								ShotMS->AddGivenTrack(Dup, CopySkelGuid);
							}
						}
					}
				}
			}
		}

		ShotMS->SetPlaybackRange(TRange<FFrameNumber>(0, DurationTicks));
		ShotSeq->MarkPackageDirty();

		// Persist shot sequence
		const FString ShotPkgFilename = FPackageName::LongPackageNameToFilename(
			ShotSeq->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(ShotSeq->GetOutermost(), ShotSeq, *ShotPkgFilename, SaveArgs);

		// Add to master cinematic shot track
		ShotTrack->AddSequence(ShotSeq, StartFrame, DurationTicks);
		StartFrame += FFrameNumber(DurationTicks);

		CreatedShotSequencePaths.Add(ShotSeq->GetOutermost()->GetName());
	}

	MasterMS->SetPlaybackRange(TRange<FFrameNumber>(0, StartFrame));
	MasterSeq->MarkPackageDirty();

	// Persist master
	const FString MasterPkgFilename = FPackageName::LongPackageNameToFilename(
		MasterSeq->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs MasterSaveArgs;
	MasterSaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(MasterSeq->GetOutermost(), MasterSeq, *MasterPkgFilename, MasterSaveArgs);

	return MasterSeq;
}

void UCDGBatchProcExecService::WriteComboIndexJson(
	const FString& ComboOutputDir,
	const FString& ComboKey,
	int32 FPS)
{
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.DirectoryExists(*ComboOutputDir))
		PF.CreateDirectoryTree(*ComboOutputDir);

	const FString JSONPath = FPaths::Combine(ComboOutputDir, ComboKey + TEXT(".json"));
	const bool bOK = TrajectorySL::SaveAllTrajectories(JSONPath, FPS, /*bPrettyPrint=*/true);

	if (bOK)
		BroadcastLog(FString::Printf(TEXT("    Index JSON written: %s"), *JSONPath));
	else
		BroadcastLog(FString::Printf(TEXT("    WARNING: Failed to write index JSON: %s"), *JSONPath));
}

void UCDGBatchProcExecService::CleanupComboAssets(UWorld* World)
{
	if (!World) return;

	// ── Delete trajectory + keyframe actors ───────────────────────────────────
	UCDGTrajectorySubsystem* TrajSys = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (TrajSys)
	{
		for (const FName& Name : TrajSys->GetTrajectoryNames())
			TrajSys->DeleteTrajectory(Name);
	}

	TArray<ACDGKeyframe*> Orphans;
	for (TActorIterator<ACDGKeyframe> It(World); It; ++It) Orphans.Add(*It);
	for (ACDGKeyframe* KF : Orphans) World->EditorDestroyActor(KF, true);

	// ── Delete camera actors created during export ─────────────────────────────
	TArray<ACineCameraActor*> Cameras;
	for (TActorIterator<ACineCameraActor> It(World); It; ++It) Cameras.Add(*It);
	for (ACineCameraActor* Cam : Cameras) World->EditorDestroyActor(Cam, true);

	// ── Delete shot sequence assets ───────────────────────────────────────────
	for (const FString& ShotPkg : CreatedShotSequencePaths)
	{
		DeleteAssetByPackageName(ShotPkg);
	}
	CreatedShotSequencePaths.Empty();

	// ── Delete master sequence ────────────────────────────────────────────────
	UCDGLevelSeqSubsystem* LevelSeqSys = World->GetSubsystem<UCDGLevelSeqSubsystem>();
	if (LevelSeqSys)
	{
		LevelSeqSys->DeleteLevelSequence();
	}
}

void UCDGBatchProcExecService::DestroySpawnedCharacter(UWorld* World)
{
	if (SpawnedCharacter.IsValid() && World)
	{
		World->EditorDestroyActor(SpawnedCharacter.Get(), true);
	}
	SpawnedCharacter.Reset();
}

void UCDGBatchProcExecService::DestroyGenerators()
{
	for (UCDGTrajectoryGenerator* Gen : Generators)
	{
		if (Gen && Gen->IsRooted()) Gen->RemoveFromRoot();
	}
	Generators.Empty();
}

void UCDGBatchProcExecService::DeleteReferenceSequence()
{
	if (CurrentRefSequence.IsValid())
	{
		const FString PkgName = CurrentRefSequence->GetOutermost()->GetName();
		CurrentRefSequence.Reset();
		DeleteAssetByPackageName(PkgName);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Index advancing
// ─────────────────────────────────────────────────────────────────────────────

bool UCDGBatchProcExecService::AdvanceComboIndices()
{
	// Advance innermost (Anim) first
	++CurrentAnimIdx;
	if (CurrentAnimIdx < Input.Animations.Num()) return true;

	CurrentAnimIdx = 0;
	++CurrentCharacterIdx;
	if (CurrentCharacterIdx < Input.Characters.Num()) return true;

	CurrentCharacterIdx = 0;
	++CurrentAnchorIdx;
	// Returns false when anchor index is exhausted (caller should move to next level)
	return CurrentAnchorIdx < CurrentAnchors.Num();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

FString UCDGBatchProcExecService::MakeComboKey(
	const FString& LevelShortName,
	const FString& AnchorName,
	const FString& CharShortName,
	const FString& AnimShortName) const
{
	return LevelShortName
		+ TEXT("_") + AnchorName
		+ TEXT("_") + CharShortName
		+ TEXT("_") + AnimShortName;
}

FVector UCDGBatchProcExecService::FindGroundPosition(
	UWorld* World,
	const FVector& Centre,
	float Radius,
	const AActor* IgnoredActor) const
{
	// Pick a random offset within the dispersion disc
	FVector Offset = FVector::ZeroVector;
	if (Radius > KINDA_SMALL_NUMBER)
	{
		const float Angle = FMath::FRand() * 2.f * PI;
		const float Dist  = FMath::Sqrt(FMath::FRand()) * Radius; // uniform over disc
		Offset = FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
	}

	const FVector TraceOrigin = Centre + Offset + FVector(0.f, 0.f, 2000.f);
	const FVector TraceEnd    = Centre + Offset - FVector(0.f, 0.f, 5000.f);

	FHitResult Hit;
	FCollisionQueryParams QP;
	QP.AddIgnoredActor(IgnoredActor);

	if (World->LineTraceSingleByChannel(Hit, TraceOrigin, TraceEnd, ECC_WorldStatic, QP))
	{
		return Hit.Location;
	}

	return Centre + Offset;
}

TArray<ACDGLevelSceneAnchor*> UCDGBatchProcExecService::GetAnchors(UWorld* World) const
{
	TArray<ACDGLevelSceneAnchor*> Out;
	if (!World) return Out;
	for (TActorIterator<ACDGLevelSceneAnchor> It(World); It; ++It)
	{
		Out.Add(*It);
	}
	return Out;
}

int32 UCDGBatchProcExecService::ComputeTotalCombos() const
{
	// We can't know anchor count until levels are loaded, so this is an estimate.
	// We refine dynamically; this just provides an early total-guess for the progress bar.
	// Use 1 anchor per level as a minimum estimate.
	return FMath::Max(1, Input.Levels.Num()) *
	       FMath::Max(1, Input.Characters.Num()) *
	       FMath::Max(1, Input.Animations.Num());
}

void UCDGBatchProcExecService::BroadcastLog(const FString& Msg)
{
	UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("[BatchExec] %s"), *Msg);
	OnLogMessage.Broadcast(Msg);
}

void UCDGBatchProcExecService::Finish(bool bSuccess)
{
	bIsRunning = false;

	FNotificationInfo Info(bSuccess
		? LOCTEXT("BatchDoneNotif",     "Batch processing completed successfully.")
		: LOCTEXT("BatchCancelNotif",   "Batch processing was cancelled or failed."));
	Info.ExpireDuration       = 5.f;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);

	BroadcastLog(bSuccess ? TEXT("Batch complete.") : TEXT("Batch stopped."));
	OnBatchCompleted.Broadcast(bSuccess);
}

#undef LOCTEXT_NAMESPACE
