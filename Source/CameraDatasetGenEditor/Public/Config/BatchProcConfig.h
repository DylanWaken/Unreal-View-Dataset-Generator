// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Config/GeneratorStackConfig.h"
#include "Config/LevelSeqExportConfig.h"
#include "BatchProcConfig.generated.h"

/**
 * Content-browser asset that persists a full batch processing job:
 * a set of levels, skeletal-mesh characters, and animations to process,
 * together with references to the generator stack and exporter configs.
 */
UCLASS(BlueprintType)
class CAMERADATASETGENEDITOR_API UBatchProcConfig : public UObject
{
	GENERATED_BODY()

public:
	/** Soft paths to level (UWorld) assets to process in the batch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch Assets")
	TArray<FSoftObjectPath> Levels;

	/** Soft paths to USkeletalMesh assets (characters) to use in the batch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch Assets")
	TArray<FSoftObjectPath> SkeletalMeshes;

	/** Soft paths to UAnimationAsset assets to use in the batch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch Assets")
	TArray<FSoftObjectPath> Animations;

	/** Generator stack config to use for trajectory generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config Assets")
	TObjectPtr<UGeneratorStackConfig> GeneratorConfig;

	/** Level sequence exporter config to use for rendering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config Assets")
	TObjectPtr<ULevelSeqExportConfig> ExporterConfig;
};
