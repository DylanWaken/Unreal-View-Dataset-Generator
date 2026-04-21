// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CDGLevelSceneAnchor.generated.h"

/**
 * ACDGLevelSceneAnchor
 *
 * An editor-placed actor that marks a spawn point in the level where a character
 * will be automatically positioned during batch processing.
 *
 * The DispersionRadius defines the radius of the random scatter zone around this
 * anchor — at runtime the batch processor will place the character somewhere
 * within this circle on the ground plane.
 *
 * A flat circle, center cross and upward arrow are drawn in the editor viewport
 * to visualise the placement zone.  The actor is hidden during play/render.
 */
UCLASS(Blueprintable, ClassGroup = "CameraDatasetGen",
	meta = (BlueprintSpawnableComponent))
class CAMERADATASETGEN_API ACDGLevelSceneAnchor : public AActor
{
	GENERATED_BODY()

public:
	ACDGLevelSceneAnchor();

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
#endif

public:
	// ── Placement ─────────────────────────────────────────────────────────────

	/**
	 * Radius (in cm) of the scatter zone around this anchor.
	 * The batch processor will randomly place the character within this circle
	 * on the XY ground plane at the anchor's location.
	 * Set to 0 for a fixed, exact placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Anchor",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
	float DispersionRadius = 150.0f;

	// ── Visualization ─────────────────────────────────────────────────────────

	/** Color used for the editor viewport visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FLinearColor AnchorColor = FLinearColor(0.2f, 0.85f, 0.95f, 1.0f);

	// ── Components ────────────────────────────────────────────────────────────

protected:
	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	/** Draws the placement circle and arrow in the editor */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UCDGSceneAnchorVisualizer> Visualizer;

	/** Invisible sphere used for click-selection in the editor */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USphereComponent> SelectionSphere;
#endif

private:
	void SyncVisualizer();
};
