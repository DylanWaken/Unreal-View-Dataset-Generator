// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "CDGTrajectory.generated.h"

class ACDGKeyframe;
class UCDGTrajectoryVisualizer;

/**
 * CDGTrajectory Actor
 * 
 * An actor that represents a camera trajectory path.
 * Contains a spline component and manages a set of keyframes.
 * 
 * Features:
 * - Owns a spline component that defines the path
 * - Holds references to keyframes assigned to this trajectory
 * - Visualizes the trajectory path in the editor viewport
 * - Automatically updates when keyframes are modified
 * - Can be exported to Level Sequence
 */
UCLASS(Blueprintable, ClassGroup = "CameraDatasetGen", meta = (BlueprintSpawnableComponent))
class CAMERADATASETGEN_API ACDGTrajectory : public AActor
{
	GENERATED_BODY()
	
public:	
	ACDGTrajectory();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	virtual void PostActorCreated() override;
#endif

public:
	// ==================== TRAJECTORY PROPERTIES ====================

	/** Name of this trajectory (must be unique per world) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	FName TrajectoryName;

	/** Text prompt associated with this trajectory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory", meta = (MultiLine = "true"))
	FString TextPrompt;

	/** Color for trajectory visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory|Visualization")
	FLinearColor TrajectoryColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);

	/** Whether to show this trajectory in the viewport */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory|Visualization")
	bool bShowTrajectory = true;

	/** Whether this trajectory forms a closed loop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	bool bClosedLoop = false;

	/** Line thickness for trajectory visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory|Visualization", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float LineThickness = 3.0f;

	/** Number of segments to use for spline visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory|Visualization", meta = (ClampMin = "10", ClampMax = "500"))
	int32 VisualizationSegments = 50;

	// ==================== COMPONENTS ====================

	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Spline component that defines the trajectory path */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> SplineComponent;

#if WITH_EDITORONLY_DATA
	/** Visualizer component for trajectory rendering */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCDGTrajectoryVisualizer> VisualizerComponent;
#endif

	// ==================== KEYFRAME MANAGEMENT ====================

	/** All keyframes assigned to this trajectory */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trajectory")
	TArray<TObjectPtr<ACDGKeyframe>> Keyframes;

	/** Add a keyframe to this trajectory */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void AddKeyframe(ACDGKeyframe* Keyframe);

	/** Remove a keyframe from this trajectory */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void RemoveKeyframe(ACDGKeyframe* Keyframe);

	/** Check if this trajectory contains a keyframe */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	bool ContainsKeyframe(ACDGKeyframe* Keyframe) const;

	/** Get all keyframes sorted by order */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	TArray<ACDGKeyframe*> GetSortedKeyframes() const;

	/** Get the number of keyframes */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	int32 GetKeyframeCount() const { return Keyframes.Num(); }

	/** Check if this trajectory is valid (has at least 2 keyframes) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	bool IsValid() const { return Keyframes.Num() >= 2; }

	/** Check if this trajectory is empty (no keyframes) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	bool IsEmpty() const { return Keyframes.Num() == 0; }

	// ==================== SPLINE GENERATION ====================

	/** Rebuild the spline from keyframes */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void RebuildSpline();

	/** Mark the spline as needing rebuild */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void MarkNeedsRebuild() { bNeedsRebuild = true; }

	/** Sample position along trajectory at alpha (0-1) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	FVector SamplePosition(float Alpha) const;

	/** Sample rotation along trajectory at alpha (0-1) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	FRotator SampleRotation(float Alpha) const;

	/** Sample full transform along trajectory at alpha (0-1) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	FTransform SampleTransform(float Alpha) const;

	/** Get the total duration of the trajectory in seconds */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	float GetTrajectoryDuration() const;

	// ==================== UTILITY ====================

	/** Sort keyframes by their OrderInTrajectory */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void SortKeyframes();

	/** Auto-assign orders to keyframes based on their current positions */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void AutoAssignKeyframeOrders();
	
	/** Called when a keyframe's order is manually changed - sorts and reassigns all orders */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void OnKeyframeOrderManuallyChanged(ACDGKeyframe* ChangedKeyframe);

	/** Validate keyframes (remove null references, check for duplicates) */
	UFUNCTION(BlueprintCallable, Category = "CDGTrajectory")
	void ValidateKeyframes();
	
	/** Find the best insertion order for a new keyframe based on spline proximity */
	int32 FindBestInsertionOrder(const FVector& KeyframeLocation) const;

	/** Update the visualizer component */
	void UpdateVisualizer();

protected:
	// ==================== INTERNAL STATE ====================

	/** Whether the spline needs to be regenerated */
	bool bNeedsRebuild = true;

	// ==================== INTERNAL METHODS ====================

	/** Generate spline points from keyframes */
	void GenerateSplineFromKeyframes();

	/** Apply interpolation settings to spline points */
	void ApplyInterpolationSettings();

	/** Convert CDG interpolation mode to Unreal spline point type */
	ESplinePointType::Type ConvertInterpolationMode(ECDGInterpolationMode Mode) const;
};

