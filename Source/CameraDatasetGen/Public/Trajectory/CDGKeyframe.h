// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Trajectory/CDGKeyframeVisualizer.h"
#include "CDGKeyframe.generated.h"

/**
 * Interpolation mode for camera parameters between keyframes
 * Based on ERichCurveInterpMode from Unreal's animation system
 */
UENUM(BlueprintType)
enum class ECDGInterpolationMode : uint8
{
	Linear UMETA(DisplayName = "Linear"),
	Cubic UMETA(DisplayName = "Cubic (Smooth)"),
	Constant UMETA(DisplayName = "Constant (Step)"),
	CubicClamped UMETA(DisplayName = "Cubic Clamped"),
	CustomTangent UMETA(DisplayName = "Custom Tangent")
};

/**
 * Tangent mode for custom interpolation control
 */
UENUM(BlueprintType)
enum class ECDGTangentMode : uint8
{
	Auto UMETA(DisplayName = "Auto"),
	User UMETA(DisplayName = "User"),
	Break UMETA(DisplayName = "Break")
};

/**
 * Speed interpolation mode for movement between keyframes
 */
UENUM(BlueprintType)
enum class ECDGSpeedInterpolationMode : uint8
{
	Linear UMETA(DisplayName = "Linear"),
	Cubic UMETA(DisplayName = "Cubic (Smooth)"),
	Constant UMETA(DisplayName = "Constant"),
	SlowIn UMETA(DisplayName = "Slow In"),
	SlowOut UMETA(DisplayName = "Slow Out"),
	SlowInOut UMETA(DisplayName = "Slow In/Out")
};

/**
 * Camera lens settings matching UCineCameraComponent
 * All properties can be keyframed and exported to Level Sequence
 */
USTRUCT(BlueprintType)
struct FCDGCameraLensSettings
{
	GENERATED_BODY()

	// Value bounds for UI and validation
	static constexpr float FocalLengthMin = 4.0f;
	static constexpr float FocalLengthMax = 1000.0f;
	static constexpr float FocalLengthSliderMin = 10.0f;
	static constexpr float FocalLengthSliderMax = 200.0f;
	
	static constexpr float FieldOfViewMin = 5.0f;
	static constexpr float FieldOfViewMax = 170.0f;
	static constexpr float FieldOfViewSliderMin = 10.0f;
	static constexpr float FieldOfViewSliderMax = 120.0f;
	
	static constexpr float ApertureMin = 1.2f;
	static constexpr float ApertureMax = 22.0f;
	
	static constexpr float FocusDistanceMin = 0.0f;
	static constexpr float FocusDistanceMax = 999999.0f;
	static constexpr float FocusDistanceSliderMin = 10.0f;
	static constexpr float FocusDistanceSliderMax = 10000.0f;
	
	static constexpr int32 DiaphragmBladeCountMin = 4;
	static constexpr int32 DiaphragmBladeCountMax = 16;

	/** Focal length in mm (controls FOV/zoom) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float FocalLength = 35.0f;

	/** Field of view in degrees (horizontal) - Automatically calculated from focal length */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "5.0", ClampMax = "170.0", UIMin = "10.0", UIMax = "120.0"))
	float FieldOfView = 54.43f;

	/** Aperture in f-stop (controls depth of field) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "1.2", ClampMax = "22.0"))
	float Aperture = 2.8f;

	/** Focus distance in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "0.0", ClampMax = "999999.0", UIMin = "10.0", UIMax = "10000.0"))
	float FocusDistance = 100000.0f;

	/** Diaphragm blade count (for bokeh shape) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "4", ClampMax = "16"))
	int32 DiaphragmBladeCount = 5;
};

/**
 * Camera filmback (sensor) settings
 */
USTRUCT(BlueprintType)
struct FCDGCameraFilmbackSettings
{
	GENERATED_BODY()

	// Value bounds for UI and validation
	static constexpr float SensorWidthMin = 1.0f;
	static constexpr float SensorWidthMax = 100.0f;
	
	static constexpr float SensorHeightMin = 1.0f;
	static constexpr float SensorHeightMax = 100.0f;
	
	static constexpr float SensorAspectRatioMin = 0.1f;
	static constexpr float SensorAspectRatioMax = 10.0f;

	/** Sensor width in mm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filmback", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float SensorWidth = 24.89f;

	/** Sensor height in mm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filmback", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float SensorHeight = 18.67f;

	/** Sensor aspect ratio (width/height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filmback", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float SensorAspectRatio = 1.33f;
};

/**
 * Spline interpolation settings for this keyframe
 * Controls how the trajectory curves through this point
 */
USTRUCT(BlueprintType)
struct FCDGSplineInterpolationSettings
{
	GENERATED_BODY()

	// Value bounds for UI and validation
	static constexpr float TensionMin = -1.0f;
	static constexpr float TensionMax = 1.0f;
	
	static constexpr float BiasMin = -1.0f;
	static constexpr float BiasMax = 1.0f;

	/** Interpolation mode for position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation")
	ECDGInterpolationMode PositionInterpMode = ECDGInterpolationMode::Cubic;

	/** Interpolation mode for rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation")
	ECDGInterpolationMode RotationInterpMode = ECDGInterpolationMode::Cubic;

	/** Use quaternion interpolation for rotation (prevents gimbal lock) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation")
	bool bUseQuaternionInterpolation = true;

	/** Tangent mode for position curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation")
	ECDGTangentMode PositionTangentMode = ECDGTangentMode::Auto;

	/** Tangent mode for rotation curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation")
	ECDGTangentMode RotationTangentMode = ECDGTangentMode::Auto;

	/** Custom arrive tangent for position (if TangentMode is User or Break) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation|Custom Tangents")
	FVector PositionArriveTangent = FVector::ZeroVector;

	/** Custom leave tangent for position (if TangentMode is User or Break) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation|Custom Tangents")
	FVector PositionLeaveTangent = FVector::ZeroVector;

	/** Custom arrive tangent for rotation (if TangentMode is User or Break) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation|Custom Tangents")
	FRotator RotationArriveTangent = FRotator::ZeroRotator;

	/** Custom leave tangent for rotation (if TangentMode is User or Break) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation|Custom Tangents")
	FRotator RotationLeaveTangent = FRotator::ZeroRotator;

	/** Tension for automatic tangent calculation (-1 to 1, 0 = normal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation|Auto Tangents", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Tension = 0.0f;

	/** Bias for automatic tangent calculation (-1 to 1, 0 = normal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation|Auto Tangents", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Bias = 0.0f;
};

/**
 * CDGKeyframe Actor
 * 
 * Stores all necessary transform and camera parameters for a single keyframe point.
 * Always assigned to a trajectory with a specific order for spline-based camera path generation.
 * 
 * Features:
 * - Stores transform (location, rotation, scale)
 * - Stores cinematic camera parameters (focal length, aperture, focus distance, etc.)
 * - Stores spline interpolation settings (interpolation mode, tangents, etc.)
 * - Renders 3D camera frustum indicator in editor (hidden during play/render)
 * - Automatically assigned to a unique trajectory on creation
 * - Exports to Level Sequence with proper keyframe data
 * 
 * Based on research from CameraPathResearch.md:
 * - Transform data maps to UMovieScene3DTransformSection channels
 * - Camera parameters map to UCineCameraComponent properties
 * - Interpolation settings map to FMovieSceneDoubleChannel tangent data
 */
UCLASS(Blueprintable, ClassGroup = "CameraDatasetGen", meta = (BlueprintSpawnableComponent))
class CAMERADATASETGEN_API ACDGKeyframe : public AActor
{
	GENERATED_BODY()
	
public:	
	ACDGKeyframe();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditImport() override;
	virtual void PostLoad() override;
	virtual void PostActorCreated() override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
#endif

public:
	// ==================== TRAJECTORY ASSIGNMENT ====================
	
	// Value bounds for UI and validation
	static constexpr int32 OrderInTrajectoryMin = 0;
	static constexpr int32 OrderInTrajectoryMax = 9999;
	static constexpr int32 OrderInTrajectorySliderMin = 0;
	static constexpr int32 OrderInTrajectorySliderMax = 100;
	
	static constexpr float TimeHintMin = 0.0f;
	static constexpr float TimeHintMax = 9999.0f;
	static constexpr float TimeHintSliderMin = 0.0f;
	static constexpr float TimeHintSliderMax = 60.0f;
	
	/** Name of the trajectory this keyframe belongs to (always assigned, automatically generated on creation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	FName TrajectoryName;

	/** Order of this keyframe within its trajectory (0-based index) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory", meta = (ClampMin = "0", ClampMax = "9999", UIMin = "0", UIMax = "100"))
	int32 OrderInTrajectory = 0;

	/** Time/duration hint for this keyframe (in seconds, for export reference) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory", meta = (ClampMin = "0.0", ClampMax = "9999.0", UIMin = "0.0", UIMax = "60.0"))
	float TimeHint = 0.0f;

	// ==================== TIMING ====================

	/** Duration from previous keyframe to current (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float TimeToCurrentFrame = 0.5f;

	/** Duration to remain stationary at current keyframe (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float TimeAtCurrentFrame = 0.0f;

	/** Speed interpolation mode for movement to this keyframe */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	ECDGSpeedInterpolationMode SpeedInterpolationMode = ECDGSpeedInterpolationMode::Linear;

	// ==================== CAMERA PARAMETERS ====================
	
	/** Lens settings (focal length, aperture, focus distance, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FCDGCameraLensSettings LensSettings;

	/** Filmback/sensor settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FCDGCameraFilmbackSettings FilmbackSettings;

	// ==================== SPLINE INTERPOLATION ====================
	
	/** Interpolation settings for this keyframe */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation")
	FCDGSplineInterpolationSettings InterpolationSettings;

	// ==================== VISUALIZATION ====================
	
	// Value bounds for UI and validation
	static constexpr float FrustumSizeMin = 10.0f;
	static constexpr float FrustumSizeMax = 1000.0f;
	static constexpr float FrustumSizeSliderMin = 10.0f;
	static constexpr float FrustumSizeSliderMax = 500.0f;
	
	/** Show camera frustum in editor viewport */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool bShowCameraFrustum = true;

	/** Show trajectory line to next keyframe */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool bShowTrajectoryLine = true;

	/** Color of this keyframe's visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FLinearColor KeyframeColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);

	/** Size of the camera frustum visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization", meta = (ClampMin = "10.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "500.0"))
	float FrustumSize = 100.0f;

	// ==================== METADATA ====================
	
	/** Optional label for this keyframe */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metadata")
	FString KeyframeLabel;

	/** Optional notes for this keyframe */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metadata", meta = (MultiLine = true))
	FString Notes;

	// ==================== COMPONENTS ====================

protected:
	/** Root scene component for transform */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	/** Visualizer component for camera frustum rendering */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCDGKeyframeVisualizer> VisualizerComponent;

	/** Invisible sphere component for easier selection in editor */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USphereComponent> SelectionSphere;
#endif

	// ==================== PUBLIC METHODS ====================

public:
	/** Get the world transform of this keyframe */
	UFUNCTION(BlueprintCallable, Category = "CDGKeyframe")
	FTransform GetKeyframeTransform() const;

	/** Set the world transform of this keyframe */
	UFUNCTION(BlueprintCallable, Category = "CDGKeyframe")
	void SetKeyframeTransform(const FTransform& NewTransform);

	/** Calculate FOV from focal length and sensor size */
	UFUNCTION(BlueprintCallable, Category = "CDGKeyframe")
	float CalculateFOVFromFocalLength() const;

	/** Calculate focal length from FOV and sensor size */
	UFUNCTION(BlueprintCallable, Category = "CDGKeyframe")
	float CalculateFocalLengthFromFOV() const;

	/** Update FOV based on current focal length */
	UFUNCTION(BlueprintCallable, Category = "CDGKeyframe")
	void UpdateFOVFromFocalLength();

	/** Update focal length based on current FOV */
	UFUNCTION(BlueprintCallable, Category = "CDGKeyframe")
	void UpdateFocalLengthFromFOV();

	/** Check if this keyframe is assigned to a trajectory (always true, keyframes are always assigned) */
	UFUNCTION(BlueprintCallable, Category = "CDGKeyframe")
	bool IsAssignedToTrajectory() const { return !TrajectoryName.IsNone(); }

	/** Get a unique identifier for this keyframe */
	UFUNCTION(BlueprintCallable, Category = "CDGKeyframe")
	FString GetKeyframeID() const;

	// ==================== RENDERING CONTROL ====================

	/** Check if we should hide the actor (during play, render, MRQ) */
	bool ShouldHideActor() const;

	/** Update visibility based on current mode */
	void UpdateVisibility();

	/** Update the visualizer component */
	void UpdateVisualizer();

	/** Get the display color for this keyframe (from trajectory or default to white) */
	UFUNCTION(BlueprintCallable, Category = "CDGKeyframe")
	FLinearColor GetVisualizationColor() const;

#if WITH_EDITOR
	// ==================== EDITOR VISUALIZATION ====================

	/** Notify the trajectory subsystem that this keyframe has changed */
	void NotifyTrajectorySubsystem();

private:
	/** Previous trajectory name (for tracking changes) */
	FName PreviousTrajectoryName;
#endif
};

