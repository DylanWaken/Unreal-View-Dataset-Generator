// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MRQInterface/CDGMRQInterface.h"
#include "LevelSeqExportConfig.generated.h"

/**
 * Serializable content asset that stores generalized export/render settings for
 * the Level Sequence Exporter. Contains no trajectory, character, or level-sequence
 * references — only the portable render/quality parameters.
 */
UCLASS(BlueprintType)
class CAMERADATASETGENEDITOR_API ULevelSeqExportConfig : public UObject
{
	GENERATED_BODY()

public:
	// ---- Export Settings ----

	/** Frames per second for the exported level sequence */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings", meta = (ClampMin = "1", ClampMax = "240"))
	int32 FPS = 30;

	/** Clear existing tracks in the level sequence before exporting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings")
	bool bClearLevelSequence = true;

	// ---- Render Settings ----

	/** Output directory where rendered frames/video will be written */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render Settings")
	FString OutputDirectory;

	/** Output resolution width in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render Settings", meta = (ClampMin = "1", ClampMax = "7680"))
	int32 ResolutionWidth = 1920;

	/** Output resolution height in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render Settings", meta = (ClampMin = "1", ClampMax = "4320"))
	int32 ResolutionHeight = 1080;

	/** Output file format */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render Settings")
	ECDGRenderOutputFormat ExportFormat = ECDGRenderOutputFormat::PNG_Sequence;

	/** Write an Index.json file alongside the rendered output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render Settings")
	bool bExportIndexJSON = true;

	/** Overwrite files that already exist in the output directory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render Settings")
	bool bOverwriteExisting = false;

	// ---- Quality Settings ----

	/** Spatial (anti-aliasing) sample count */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality Settings", meta = (ClampMin = "1", ClampMax = "32"))
	int32 SpatialSampleCount = 1;

	/** Temporal (motion-blur) sample count */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality Settings", meta = (ClampMin = "1", ClampMax = "32"))
	int32 TemporalSampleCount = 1;

	// ---- Other ----

	/** Keep the exported Level Sequence asset after rendering completes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Other")
	bool bKeepExportedLevelSequence = false;
};
