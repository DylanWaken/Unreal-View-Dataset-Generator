// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeneratorStackConfig.generated.h"

/**
 * Content-browser asset that persists a generator stack configuration.
 *
 * GeneratorsJson holds the same JSON format produced by OnExportConfigClicked
 * (array of { "class": "...", "config": { ... } } objects), so the existing
 * SerializeGeneratorConfig / FetchGeneratorConfig API is reused directly.
 *
 * bLetBatchProcessorFill is stored alongside so the full UI state can be
 * round-tripped through a single asset.
 */
UCLASS(BlueprintType)
class CAMERADATASETGENEDITOR_API UGeneratorStackConfig : public UObject
{
	GENERATED_BODY()

public:
	/** Serialized generator stack — same JSON structure as the file-based Export Config */
	UPROPERTY(BlueprintReadOnly, Category = "Generator Config")
	FString GeneratorsJson;

	/** When true the reference sequence and primary actor slots are left for the batch processor to fill at runtime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator Config")
	bool bLetBatchProcessorFill = false;
};
