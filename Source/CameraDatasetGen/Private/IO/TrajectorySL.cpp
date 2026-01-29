// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/TrajectorySL.h"
#include "Trajectory/CDGTrajectory.h"
#include "Trajectory/CDGKeyframe.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "LogCameraDatasetGen.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace TrajectorySL
{
	bool SaveAllTrajectories(const FString& FilePath, int32 FPS, bool bPrettyPrint)
	{
		FString JsonString;
		if (!SaveAllTrajectoriesAsString(JsonString, FPS, bPrettyPrint))
		{
			UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: Failed to generate JSON string"));
			return false;
		}

		// Write to file
		if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
		{
			UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: Failed to write file: %s"), *FilePath);
			return false;
		}

		UE_LOG(LogCameraDatasetGen, Log, TEXT("TrajectorySL: Successfully saved trajectories to: %s"), *FilePath);
		return true;
	}

	bool SaveAllTrajectoriesAsString(FString& OutJsonString, int32 FPS, bool bPrettyPrint)
	{
		// Get world
		UWorld* World = nullptr;
#if WITH_EDITOR
		if (GEditor)
		{
			World = GEditor->GetEditorWorldContext().World();
		}
#else

		if (GEngine)
		{
			World = GEngine->GetWorld();
		}
#endif

		if (!World)
		{
			UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: No valid world context found"));
			return false;
		}

		// Gather all trajectories (same order as CDGLevelSeqExporter)
		TArray<ACDGTrajectory*> Trajectories;
		for (TActorIterator<ACDGTrajectory> It(World); It; ++It)
		{
			Trajectories.Add(*It);
		}

		if (Trajectories.Num() == 0)
		{
			UE_LOG(LogCameraDatasetGen, Warning, TEXT("TrajectorySL: No trajectories found in the world"));
		}

		// Create root JSON object
		TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();

		// Add level name
		FString LevelName = World->GetMapName();
		LevelName.RemoveFromStart(World->StreamingLevelsPrefix); // Remove PIE prefix if present
		RootObject->SetStringField(TEXT("LevelName"), LevelName);

		// Create trajectories array
		TArray<TSharedPtr<FJsonValue>> TrajectoriesArray;

		// Process each trajectory (in the same order as CDGLevelSeqExporter)
		for (int32 TrajIndex = 0; TrajIndex < Trajectories.Num(); ++TrajIndex)
		{
			ACDGTrajectory* Trajectory = Trajectories[TrajIndex];
			if (!Trajectory)
			{
				continue;
			}

			TSharedPtr<FJsonObject> TrajObject = MakeShared<FJsonObject>();

			// Basic trajectory info
			TrajObject->SetNumberField(TEXT("TrajectoryIndex"), TrajIndex);
			TrajObject->SetStringField(TEXT("TrajectoryName"), Trajectory->TrajectoryName.ToString());
			TrajObject->SetStringField(TEXT("Prompt"), Trajectory->TextPrompt);
			TrajObject->SetNumberField(TEXT("Duration"), Trajectory->GetTrajectoryDuration());

			// Get sorted keyframes (same as CDGLevelSeqExporter)
			TArray<ACDGKeyframe*> SortedKeyframes = Trajectory->GetSortedKeyframes();
			TrajObject->SetNumberField(TEXT("KeyframeCount"), SortedKeyframes.Num());

			// ==================== KEYFRAMES DATA ====================
			TArray<TSharedPtr<FJsonValue>> KeyframesArray;
			
			double CurrentTimeSeconds = 0.0;
			for (int32 k = 0; k < SortedKeyframes.Num(); ++k)
			{
				ACDGKeyframe* Keyframe = SortedKeyframes[k];
				if (!Keyframe)
				{
					continue;
				}

				// Calculate time for this keyframe (same logic as CDGLevelSeqExporter)
				if (k > 0)
				{
					CurrentTimeSeconds += Keyframe->TimeToCurrentFrame;
				}

				TSharedPtr<FJsonObject> KeyframeObj = Internal::KeyframeToJson(Keyframe);
				KeyframeObj->SetNumberField(TEXT("KeyframeIndex"), k);
				KeyframeObj->SetNumberField(TEXT("TimeInTrajectory"), CurrentTimeSeconds);

				// Account for stay time
				if (Keyframe->TimeAtCurrentFrame > KINDA_SMALL_NUMBER)
				{
					CurrentTimeSeconds += Keyframe->TimeAtCurrentFrame;
				}

				KeyframesArray.Add(MakeShared<FJsonValueObject>(KeyframeObj));
			}

			TrajObject->SetArrayField(TEXT("KeyFrames"), KeyframesArray);

			// ==================== FRAMES DATA (Per-Frame Interpolation) ====================
			TArray<TSharedPtr<FJsonValue>> FramesArray = Internal::GenerateFrameData(Trajectory, FPS);
			TrajObject->SetArrayField(TEXT("Frames"), FramesArray);

			TrajectoriesArray.Add(MakeShared<FJsonValueObject>(TrajObject));
		}

		RootObject->SetArrayField(TEXT("Trajectories"), TrajectoriesArray);

		// Convert to JSON string
		FString OutputString;
		if (bPrettyPrint)
		{
			TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&OutputString);
			if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), JsonWriter))
			{
				UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: Failed to serialize JSON"));
				return false;
			}
		}
		else
		{
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = 
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
			if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), JsonWriter))
			{
				UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: Failed to serialize JSON"));
				return false;
			}
		}

		OutJsonString = OutputString;
		return true;
	}

	bool LoadAllTrajectories(const FString& FilePath)
	{
		// Read JSON file
		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
		{
			UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: Failed to read file: %s"), *FilePath);
			return false;
		}

		// Parse JSON
		TSharedPtr<FJsonObject> RootObject;
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(JsonReader, RootObject) || !RootObject.IsValid())
		{
			UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: Failed to parse JSON from file: %s"), *FilePath);
			return false;
		}

		// Get world
		UWorld* World = nullptr;
#if WITH_EDITOR
		if (GEditor)
		{
			World = GEditor->GetEditorWorldContext().World();
		}
#else
		if (GEngine)
		{
			World = GEngine->GetWorld();
		}
#endif

		if (!World)
		{
			UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: No valid world context found"));
			return false;
		}

		// Get trajectory subsystem
		UCDGTrajectorySubsystem* TrajectorySubsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
		if (!TrajectorySubsystem)
		{
			UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: Failed to get CDGTrajectorySubsystem"));
			return false;
		}

		// Get trajectories array
		const TArray<TSharedPtr<FJsonValue>>* TrajectoriesArray;
		if (!RootObject->TryGetArrayField(TEXT("Trajectories"), TrajectoriesArray))
		{
			UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: No 'Trajectories' array found in JSON"));
			return false;
		}

		int32 TrajectoriesLoaded = 0;
		int32 KeyframesLoaded = 0;

		// Process each trajectory
		for (const TSharedPtr<FJsonValue>& TrajValue : *TrajectoriesArray)
		{
			const TSharedPtr<FJsonObject>* TrajObj;
			if (!TrajValue->TryGetObject(TrajObj))
			{
				continue;
			}

			// Get trajectory name
			FString TrajectoryName;
			if (!(*TrajObj)->TryGetStringField(TEXT("TrajectoryName"), TrajectoryName))
			{
				UE_LOG(LogCameraDatasetGen, Warning, TEXT("TrajectorySL: Trajectory missing 'TrajectoryName', skipping"));
				continue;
			}

			// Get keyframes array
			const TArray<TSharedPtr<FJsonValue>>* KeyframesArray;
			if (!(*TrajObj)->TryGetArrayField(TEXT("KeyFrames"), KeyframesArray) || KeyframesArray->Num() == 0)
			{
				UE_LOG(LogCameraDatasetGen, Warning, TEXT("TrajectorySL: Trajectory '%s' has no keyframes, skipping"), *TrajectoryName);
				continue;
			}

			// Get text prompt (optional)
			FString TextPrompt;
			(*TrajObj)->TryGetStringField(TEXT("Prompt"), TextPrompt);

			UE_LOG(LogCameraDatasetGen, Log, TEXT("TrajectorySL: Loading trajectory '%s' with %d keyframes"), 
				*TrajectoryName, KeyframesArray->Num());

			// Create keyframes for this trajectory
			TArray<ACDGKeyframe*> CreatedKeyframes;

			for (const TSharedPtr<FJsonValue>& KeyframeValue : *KeyframesArray)
			{
				const TSharedPtr<FJsonObject>* KeyframeObj;
				if (!KeyframeValue->TryGetObject(KeyframeObj))
				{
					continue;
				}

				// Get the original order from JSON (defaults to 0 if not found)
				int32 KeyframeOrder = 0;
				(*KeyframeObj)->TryGetNumberField(TEXT("OrderInTrajectory"), KeyframeOrder);

				ACDGKeyframe* NewKeyframe = Internal::LoadKeyframeFromJson(World, *KeyframeObj, FName(*TrajectoryName), KeyframeOrder);
				if (NewKeyframe)
				{
					CreatedKeyframes.Add(NewKeyframe);
					KeyframesLoaded++;
				}
			}

			if (CreatedKeyframes.Num() > 0)
			{
				// Set text prompt on the first keyframe's trajectory (trajectory is auto-created by subsystem)
				if (ACDGTrajectory* Trajectory = TrajectorySubsystem->GetTrajectory(FName(*TrajectoryName)))
				{
					Trajectory->TextPrompt = TextPrompt;
					Trajectory->MarkPackageDirty();
				}

				TrajectoriesLoaded++;
				UE_LOG(LogCameraDatasetGen, Log, TEXT("TrajectorySL: Successfully loaded trajectory '%s' with %d keyframes"), 
					*TrajectoryName, CreatedKeyframes.Num());
			}
		}

		UE_LOG(LogCameraDatasetGen, Log, TEXT("TrajectorySL: Loaded %d trajectories with %d total keyframes from: %s"), 
			TrajectoriesLoaded, KeyframesLoaded, *FilePath);

		return TrajectoriesLoaded > 0;
	}

	namespace Internal
	{
		TSharedPtr<FJsonObject> KeyframeToJson(ACDGKeyframe* Keyframe)
		{
			if (!Keyframe)
			{
				return nullptr;
			}

			TSharedPtr<FJsonObject> KeyframeObj = MakeShared<FJsonObject>();

			// Basic info
			KeyframeObj->SetStringField(TEXT("KeyframeName"), Keyframe->GetName());
			KeyframeObj->SetStringField(TEXT("KeyframeLabel"), Keyframe->KeyframeLabel);
			KeyframeObj->SetStringField(TEXT("Notes"), Keyframe->Notes);
			KeyframeObj->SetNumberField(TEXT("OrderInTrajectory"), Keyframe->OrderInTrajectory);

			// Timing
			TSharedPtr<FJsonObject> TimingObj = MakeShared<FJsonObject>();
			TimingObj->SetNumberField(TEXT("TimeToCurrentFrame"), Keyframe->TimeToCurrentFrame);
			TimingObj->SetNumberField(TEXT("TimeAtCurrentFrame"), Keyframe->TimeAtCurrentFrame);
			TimingObj->SetNumberField(TEXT("TimeHint"), Keyframe->TimeHint);
			TimingObj->SetStringField(TEXT("SpeedInterpolationMode"), 
				StaticEnum<ECDGSpeedInterpolationMode>()->GetNameStringByValue((int64)Keyframe->SpeedInterpolationMode));
			KeyframeObj->SetObjectField(TEXT("Timing"), TimingObj);

			// Transform
			FTransform Transform = Keyframe->GetKeyframeTransform();
			TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();
			
			FVector Location = Transform.GetLocation();
			TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
			LocationObj->SetNumberField(TEXT("X"), Location.X);
			LocationObj->SetNumberField(TEXT("Y"), Location.Y);
			LocationObj->SetNumberField(TEXT("Z"), Location.Z);
			TransformObj->SetObjectField(TEXT("Location"), LocationObj);

			FRotator Rotation = Transform.GetRotation().Rotator();
			TSharedPtr<FJsonObject> RotationObj = MakeShared<FJsonObject>();
			RotationObj->SetNumberField(TEXT("Pitch"), Rotation.Pitch);
			RotationObj->SetNumberField(TEXT("Yaw"), Rotation.Yaw);
			RotationObj->SetNumberField(TEXT("Roll"), Rotation.Roll);
			TransformObj->SetObjectField(TEXT("Rotation"), RotationObj);

			FVector Scale = Transform.GetScale3D();
			TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
			ScaleObj->SetNumberField(TEXT("X"), Scale.X);
			ScaleObj->SetNumberField(TEXT("Y"), Scale.Y);
			ScaleObj->SetNumberField(TEXT("Z"), Scale.Z);
			TransformObj->SetObjectField(TEXT("Scale"), ScaleObj);

			KeyframeObj->SetObjectField(TEXT("Transform"), TransformObj);

			// Lens Settings
			TSharedPtr<FJsonObject> LensObj = MakeShared<FJsonObject>();
			LensObj->SetNumberField(TEXT("FocalLength"), Keyframe->LensSettings.FocalLength);
			LensObj->SetNumberField(TEXT("FieldOfView"), Keyframe->LensSettings.FieldOfView);
			LensObj->SetNumberField(TEXT("Aperture"), Keyframe->LensSettings.Aperture);
			LensObj->SetNumberField(TEXT("FocusDistance"), Keyframe->LensSettings.FocusDistance);
			LensObj->SetNumberField(TEXT("DiaphragmBladeCount"), Keyframe->LensSettings.DiaphragmBladeCount);
			KeyframeObj->SetObjectField(TEXT("LensSettings"), LensObj);

			// Filmback Settings
			TSharedPtr<FJsonObject> FilmbackObj = MakeShared<FJsonObject>();
			FilmbackObj->SetNumberField(TEXT("SensorWidth"), Keyframe->FilmbackSettings.SensorWidth);
			FilmbackObj->SetNumberField(TEXT("SensorHeight"), Keyframe->FilmbackSettings.SensorHeight);
			FilmbackObj->SetNumberField(TEXT("SensorAspectRatio"), Keyframe->FilmbackSettings.SensorAspectRatio);
			KeyframeObj->SetObjectField(TEXT("FilmbackSettings"), FilmbackObj);

			// Interpolation Settings
			TSharedPtr<FJsonObject> InterpObj = MakeShared<FJsonObject>();
			InterpObj->SetStringField(TEXT("PositionInterpMode"), 
				StaticEnum<ECDGInterpolationMode>()->GetNameStringByValue((int64)Keyframe->InterpolationSettings.PositionInterpMode));
			InterpObj->SetStringField(TEXT("RotationInterpMode"), 
				StaticEnum<ECDGInterpolationMode>()->GetNameStringByValue((int64)Keyframe->InterpolationSettings.RotationInterpMode));
			InterpObj->SetBoolField(TEXT("bUseQuaternionInterpolation"), Keyframe->InterpolationSettings.bUseQuaternionInterpolation);
			InterpObj->SetStringField(TEXT("PositionTangentMode"), 
				StaticEnum<ECDGTangentMode>()->GetNameStringByValue((int64)Keyframe->InterpolationSettings.PositionTangentMode));
			InterpObj->SetStringField(TEXT("RotationTangentMode"), 
				StaticEnum<ECDGTangentMode>()->GetNameStringByValue((int64)Keyframe->InterpolationSettings.RotationTangentMode));
			InterpObj->SetNumberField(TEXT("Tension"), Keyframe->InterpolationSettings.Tension);
			InterpObj->SetNumberField(TEXT("Bias"), Keyframe->InterpolationSettings.Bias);

			// Custom tangents
			TSharedPtr<FJsonObject> PosArriveObj = MakeShared<FJsonObject>();
			PosArriveObj->SetNumberField(TEXT("X"), Keyframe->InterpolationSettings.PositionArriveTangent.X);
			PosArriveObj->SetNumberField(TEXT("Y"), Keyframe->InterpolationSettings.PositionArriveTangent.Y);
			PosArriveObj->SetNumberField(TEXT("Z"), Keyframe->InterpolationSettings.PositionArriveTangent.Z);
			InterpObj->SetObjectField(TEXT("PositionArriveTangent"), PosArriveObj);

			TSharedPtr<FJsonObject> PosLeaveObj = MakeShared<FJsonObject>();
			PosLeaveObj->SetNumberField(TEXT("X"), Keyframe->InterpolationSettings.PositionLeaveTangent.X);
			PosLeaveObj->SetNumberField(TEXT("Y"), Keyframe->InterpolationSettings.PositionLeaveTangent.Y);
			PosLeaveObj->SetNumberField(TEXT("Z"), Keyframe->InterpolationSettings.PositionLeaveTangent.Z);
			InterpObj->SetObjectField(TEXT("PositionLeaveTangent"), PosLeaveObj);

			TSharedPtr<FJsonObject> RotArriveObj = MakeShared<FJsonObject>();
			RotArriveObj->SetNumberField(TEXT("Pitch"), Keyframe->InterpolationSettings.RotationArriveTangent.Pitch);
			RotArriveObj->SetNumberField(TEXT("Yaw"), Keyframe->InterpolationSettings.RotationArriveTangent.Yaw);
			RotArriveObj->SetNumberField(TEXT("Roll"), Keyframe->InterpolationSettings.RotationArriveTangent.Roll);
			InterpObj->SetObjectField(TEXT("RotationArriveTangent"), RotArriveObj);

			TSharedPtr<FJsonObject> RotLeaveObj = MakeShared<FJsonObject>();
			RotLeaveObj->SetNumberField(TEXT("Pitch"), Keyframe->InterpolationSettings.RotationLeaveTangent.Pitch);
			RotLeaveObj->SetNumberField(TEXT("Yaw"), Keyframe->InterpolationSettings.RotationLeaveTangent.Yaw);
			RotLeaveObj->SetNumberField(TEXT("Roll"), Keyframe->InterpolationSettings.RotationLeaveTangent.Roll);
			InterpObj->SetObjectField(TEXT("RotationLeaveTangent"), RotLeaveObj);

			KeyframeObj->SetObjectField(TEXT("InterpolationSettings"), InterpObj);

			// Visualization settings
			TSharedPtr<FJsonObject> VisObj = MakeShared<FJsonObject>();
			VisObj->SetBoolField(TEXT("bShowCameraFrustum"), Keyframe->bShowCameraFrustum);
			VisObj->SetBoolField(TEXT("bShowTrajectoryLine"), Keyframe->bShowTrajectoryLine);
			VisObj->SetNumberField(TEXT("FrustumSize"), Keyframe->FrustumSize);
			
			FLinearColor Color = Keyframe->KeyframeColor;
			TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
			ColorObj->SetNumberField(TEXT("R"), Color.R);
			ColorObj->SetNumberField(TEXT("G"), Color.G);
			ColorObj->SetNumberField(TEXT("B"), Color.B);
			ColorObj->SetNumberField(TEXT("A"), Color.A);
			VisObj->SetObjectField(TEXT("KeyframeColor"), ColorObj);

			KeyframeObj->SetObjectField(TEXT("Visualization"), VisObj);

			return KeyframeObj;
		}

		TArray<TSharedPtr<FJsonValue>> GenerateFrameData(ACDGTrajectory* Trajectory, int32 FPS)
		{
			TArray<TSharedPtr<FJsonValue>> FramesArray;

			if (!Trajectory || FPS <= 0)
			{
				return FramesArray;
			}

			TArray<ACDGKeyframe*> SortedKeyframes = Trajectory->GetSortedKeyframes();
			if (SortedKeyframes.Num() < 2)
			{
				// Need at least 2 keyframes for interpolation
				return FramesArray;
			}

			const float Duration = Trajectory->GetTrajectoryDuration();
			const int32 TotalFrames = FMath::Max(1, FMath::RoundToInt(Duration * FPS));
			const float DeltaTime = Duration / TotalFrames;

			// Build time array for keyframes (same logic as CDGLevelSeqExporter)
			TArray<float> KeyframeTimes;
			float CurrentTime = 0.0f;
			KeyframeTimes.Add(CurrentTime);

			for (int32 k = 1; k < SortedKeyframes.Num(); ++k)
			{
				ACDGKeyframe* Keyframe = SortedKeyframes[k];
				if (!Keyframe)
				{
					continue;
				}

				CurrentTime += Keyframe->TimeToCurrentFrame;
				KeyframeTimes.Add(CurrentTime);

				// Account for stay time
				if (Keyframe->TimeAtCurrentFrame > KINDA_SMALL_NUMBER)
				{
					CurrentTime += Keyframe->TimeAtCurrentFrame;
				}
			}

			// Generate per-frame data
			for (int32 FrameIndex = 0; FrameIndex < TotalFrames; ++FrameIndex)
			{
				const float FrameTime = FrameIndex * DeltaTime;
				TSharedPtr<FJsonObject> FrameObj = MakeShared<FJsonObject>();

				FrameObj->SetNumberField(TEXT("FrameIndex"), FrameIndex);
				FrameObj->SetNumberField(TEXT("Time"), FrameTime);

				// Find the two keyframes we're interpolating between
				int32 KeyframeIndexA = 0;
				int32 KeyframeIndexB = 1;
				float Alpha = 0.0f;

				for (int32 k = 0; k < KeyframeTimes.Num() - 1; ++k)
				{
					if (FrameTime >= KeyframeTimes[k] && FrameTime <= KeyframeTimes[k + 1])
					{
						KeyframeIndexA = k;
						KeyframeIndexB = k + 1;
						
						float TimeDelta = KeyframeTimes[k + 1] - KeyframeTimes[k];
						if (TimeDelta > KINDA_SMALL_NUMBER)
						{
							Alpha = (FrameTime - KeyframeTimes[k]) / TimeDelta;
						}
						break;
					}
				}

				// Handle edge case: frame time beyond last keyframe
				if (FrameTime > KeyframeTimes.Last())
				{
					KeyframeIndexA = SortedKeyframes.Num() - 1;
					KeyframeIndexB = SortedKeyframes.Num() - 1;
					Alpha = 0.0f;
				}

				ACDGKeyframe* KeyframeA = SortedKeyframes[KeyframeIndexA];
				ACDGKeyframe* KeyframeB = SortedKeyframes[KeyframeIndexB];

				// Interpolate transform
				FTransform InterpTransform = Internal::InterpolateTransform(KeyframeA, KeyframeB, Alpha);
				FVector Location = InterpTransform.GetLocation();
				FRotator Rotation = InterpTransform.GetRotation().Rotator();

				TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
				LocationObj->SetNumberField(TEXT("X"), Location.X);
				LocationObj->SetNumberField(TEXT("Y"), Location.Y);
				LocationObj->SetNumberField(TEXT("Z"), Location.Z);
				FrameObj->SetObjectField(TEXT("Translation"), LocationObj);

				TSharedPtr<FJsonObject> RotationObj = MakeShared<FJsonObject>();
				RotationObj->SetNumberField(TEXT("Pitch"), Rotation.Pitch);
				RotationObj->SetNumberField(TEXT("Yaw"), Rotation.Yaw);
				RotationObj->SetNumberField(TEXT("Roll"), Rotation.Roll);
				FrameObj->SetObjectField(TEXT("Rotation"), RotationObj);

				// Interpolate focal length
				float FocalLength = Internal::InterpolateFocalLength(KeyframeA, KeyframeB, Alpha);
				FrameObj->SetNumberField(TEXT("FocalLength"), FocalLength);

				// Add other camera parameters (interpolated)
				float Aperture = FMath::Lerp(KeyframeA->LensSettings.Aperture, KeyframeB->LensSettings.Aperture, Alpha);
				float FocusDistance = FMath::Lerp(KeyframeA->LensSettings.FocusDistance, KeyframeB->LensSettings.FocusDistance, Alpha);
				
				FrameObj->SetNumberField(TEXT("Aperture"), Aperture);
				FrameObj->SetNumberField(TEXT("FocusDistance"), FocusDistance);

				// Add keyframe blend info
				FrameObj->SetNumberField(TEXT("KeyframeIndexA"), KeyframeIndexA);
				FrameObj->SetNumberField(TEXT("KeyframeIndexB"), KeyframeIndexB);
				FrameObj->SetNumberField(TEXT("BlendAlpha"), Alpha);

				FramesArray.Add(MakeShared<FJsonValueObject>(FrameObj));
			}

			return FramesArray;
		}

		FTransform InterpolateTransform(ACDGKeyframe* KeyframeA, ACDGKeyframe* KeyframeB, float Alpha)
		{
			if (!KeyframeA)
			{
				return KeyframeB ? KeyframeB->GetKeyframeTransform() : FTransform::Identity;
			}
			if (!KeyframeB)
			{
				return KeyframeA->GetKeyframeTransform();
			}

			FTransform TransformA = KeyframeA->GetKeyframeTransform();
			FTransform TransformB = KeyframeB->GetKeyframeTransform();

			// Linear interpolation (can be enhanced with interpolation mode later)
			FVector Location = FMath::Lerp(TransformA.GetLocation(), TransformB.GetLocation(), Alpha);
			
			FQuat Rotation;
			if (KeyframeA->InterpolationSettings.bUseQuaternionInterpolation)
			{
				Rotation = FQuat::Slerp(TransformA.GetRotation(), TransformB.GetRotation(), Alpha);
			}
			else
			{
				FRotator RotA = TransformA.GetRotation().Rotator();
				FRotator RotB = TransformB.GetRotation().Rotator();
				Rotation = FMath::Lerp(RotA, RotB, Alpha).Quaternion();
			}

			FVector Scale = FMath::Lerp(TransformA.GetScale3D(), TransformB.GetScale3D(), Alpha);

			FTransform Result;
			Result.SetLocation(Location);
			Result.SetRotation(Rotation);
			Result.SetScale3D(Scale);

			return Result;
		}

		float InterpolateFocalLength(ACDGKeyframe* KeyframeA, ACDGKeyframe* KeyframeB, float Alpha)
		{
			if (!KeyframeA)
			{
				return KeyframeB ? KeyframeB->LensSettings.FocalLength : 35.0f;
			}
			if (!KeyframeB)
			{
				return KeyframeA->LensSettings.FocalLength;
			}

			return FMath::Lerp(KeyframeA->LensSettings.FocalLength, KeyframeB->LensSettings.FocalLength, Alpha);
		}

		ACDGKeyframe* LoadKeyframeFromJson(UWorld* World, const TSharedPtr<FJsonObject>& KeyframeObj, FName TrajectoryName, int32 Order)
		{
			if (!World || !KeyframeObj.IsValid())
			{
				return nullptr;
			}

			// Get trajectory subsystem
			UCDGTrajectorySubsystem* TrajectorySubsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
			if (!TrajectorySubsystem)
			{
				UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: Failed to get CDGTrajectorySubsystem"));
				return nullptr;
			}

			// Get transform from JSON
			const TSharedPtr<FJsonObject>* TransformObj;
			if (!KeyframeObj->TryGetObjectField(TEXT("Transform"), TransformObj))
			{
				UE_LOG(LogCameraDatasetGen, Warning, TEXT("TrajectorySL: Keyframe missing Transform data"));
				return nullptr;
			}

			// Parse location
			const TSharedPtr<FJsonObject>* LocationObj;
			FVector Location = FVector::ZeroVector;
			if ((*TransformObj)->TryGetObjectField(TEXT("Location"), LocationObj))
			{
				(*LocationObj)->TryGetNumberField(TEXT("X"), Location.X);
				(*LocationObj)->TryGetNumberField(TEXT("Y"), Location.Y);
				(*LocationObj)->TryGetNumberField(TEXT("Z"), Location.Z);
			}

			// Parse rotation
			const TSharedPtr<FJsonObject>* RotationObj;
			FRotator Rotation = FRotator::ZeroRotator;
			if ((*TransformObj)->TryGetObjectField(TEXT("Rotation"), RotationObj))
			{
				(*RotationObj)->TryGetNumberField(TEXT("Pitch"), Rotation.Pitch);
				(*RotationObj)->TryGetNumberField(TEXT("Yaw"), Rotation.Yaw);
				(*RotationObj)->TryGetNumberField(TEXT("Roll"), Rotation.Roll);
			}

			// Spawn keyframe actor
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			
			ACDGKeyframe* NewKeyframe = World->SpawnActor<ACDGKeyframe>(ACDGKeyframe::StaticClass(), Location, Rotation, SpawnParams);
			if (!NewKeyframe)
			{
				UE_LOG(LogCameraDatasetGen, Error, TEXT("TrajectorySL: Failed to spawn keyframe actor"));
				return nullptr;
			}

			// Store the auto-generated trajectory name from PostActorCreated
			FName PreviousTrajectoryName = NewKeyframe->TrajectoryName;

			// Set the correct trajectory name and order from JSON
			NewKeyframe->TrajectoryName = TrajectoryName;
			NewKeyframe->OrderInTrajectory = Order;

			// Notify subsystem to move keyframe from auto-generated trajectory to correct trajectory
			if (PreviousTrajectoryName != TrajectoryName && !PreviousTrajectoryName.IsNone())
			{
				TrajectorySubsystem->OnKeyframeTrajectoryNameChanged(NewKeyframe, PreviousTrajectoryName);
			}

			UE_LOG(LogCameraDatasetGen, Verbose, TEXT("TrajectorySL: Created keyframe '%s' for trajectory '%s' with order %d"), 
				*NewKeyframe->GetName(), *TrajectoryName.ToString(), Order);

			// Get timing info
			const TSharedPtr<FJsonObject>* TimingObj;
			if (KeyframeObj->TryGetObjectField(TEXT("Timing"), TimingObj))
			{
				(*TimingObj)->TryGetNumberField(TEXT("TimeToCurrentFrame"), NewKeyframe->TimeToCurrentFrame);
				(*TimingObj)->TryGetNumberField(TEXT("TimeAtCurrentFrame"), NewKeyframe->TimeAtCurrentFrame);
				(*TimingObj)->TryGetNumberField(TEXT("TimeHint"), NewKeyframe->TimeHint);
			}

			// Get lens settings
			const TSharedPtr<FJsonObject>* LensObj;
			if (KeyframeObj->TryGetObjectField(TEXT("LensSettings"), LensObj))
			{
				(*LensObj)->TryGetNumberField(TEXT("FocalLength"), NewKeyframe->LensSettings.FocalLength);
				(*LensObj)->TryGetNumberField(TEXT("FieldOfView"), NewKeyframe->LensSettings.FieldOfView);
				(*LensObj)->TryGetNumberField(TEXT("Aperture"), NewKeyframe->LensSettings.Aperture);
				(*LensObj)->TryGetNumberField(TEXT("FocusDistance"), NewKeyframe->LensSettings.FocusDistance);
				
				int32 BladeCount;
				if ((*LensObj)->TryGetNumberField(TEXT("DiaphragmBladeCount"), BladeCount))
				{
					NewKeyframe->LensSettings.DiaphragmBladeCount = BladeCount;
				}
			}

			// Get filmback settings
			const TSharedPtr<FJsonObject>* FilmbackObj;
			if (KeyframeObj->TryGetObjectField(TEXT("FilmbackSettings"), FilmbackObj))
			{
				(*FilmbackObj)->TryGetNumberField(TEXT("SensorWidth"), NewKeyframe->FilmbackSettings.SensorWidth);
				(*FilmbackObj)->TryGetNumberField(TEXT("SensorHeight"), NewKeyframe->FilmbackSettings.SensorHeight);
				(*FilmbackObj)->TryGetNumberField(TEXT("SensorAspectRatio"), NewKeyframe->FilmbackSettings.SensorAspectRatio);
			}

			// Get interpolation settings
			const TSharedPtr<FJsonObject>* InterpObj;
			if (KeyframeObj->TryGetObjectField(TEXT("InterpolationSettings"), InterpObj))
			{
				// Get interpolation modes as strings and convert them
				FString PosInterpModeStr, RotInterpModeStr;
				if ((*InterpObj)->TryGetStringField(TEXT("PositionInterpMode"), PosInterpModeStr))
				{
					int64 EnumValue = StaticEnum<ECDGInterpolationMode>()->GetValueByNameString(PosInterpModeStr);
					if (EnumValue != INDEX_NONE)
					{
						NewKeyframe->InterpolationSettings.PositionInterpMode = (ECDGInterpolationMode)EnumValue;
					}
				}
				
				if ((*InterpObj)->TryGetStringField(TEXT("RotationInterpMode"), RotInterpModeStr))
				{
					int64 EnumValue = StaticEnum<ECDGInterpolationMode>()->GetValueByNameString(RotInterpModeStr);
					if (EnumValue != INDEX_NONE)
					{
						NewKeyframe->InterpolationSettings.RotationInterpMode = (ECDGInterpolationMode)EnumValue;
					}
				}

				(*InterpObj)->TryGetBoolField(TEXT("bUseQuaternionInterpolation"), NewKeyframe->InterpolationSettings.bUseQuaternionInterpolation);
				(*InterpObj)->TryGetNumberField(TEXT("Tension"), NewKeyframe->InterpolationSettings.Tension);
				(*InterpObj)->TryGetNumberField(TEXT("Bias"), NewKeyframe->InterpolationSettings.Bias);
			}

			// Get metadata
			FString Label, Notes;
			if (KeyframeObj->TryGetStringField(TEXT("KeyframeLabel"), Label))
			{
				NewKeyframe->KeyframeLabel = Label;
			}
			if (KeyframeObj->TryGetStringField(TEXT("Notes"), Notes))
			{
				NewKeyframe->Notes = Notes;
			}

			// Mark for saving
			NewKeyframe->MarkPackageDirty();

			return NewKeyframe;
		}
	}
}
