// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CDGCharacterAnchor.generated.h"

UENUM(BlueprintType)
enum class AnchorType : uint8
{
	CDG_ANCHOR_HEAD UMETA(DisplayName = "Head"),
	CDG_ANCHOR_PELVIS UMETA(DisplayName = "Pelvis"),
	CDG_ANCHOR_FEET_LEFT UMETA(DisplayName = "Left Foot"),
	CDG_ANCHOR_FEET_RIGHT UMETA(DisplayName = "Right Foot"),
	CDG_ANCHOR_HAND_LEFT UMETA(DisplayName = "Left Hand"),
	CDG_ANCHOR_HAND_RIGHT UMETA(DisplayName = "Right Hand")
};

UCLASS(Blueprintable, ClassGroup = "CameraDatasetGen", meta = (BlueprintSpawnableComponent))
class CAMERADATASETGEN_API UCDGCharacterAnchor : public USceneComponent
{
	GENERATED_BODY()

public:
	UCDGCharacterAnchor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor")
	AnchorType Type = AnchorType::CDG_ANCHOR_HEAD;
};

