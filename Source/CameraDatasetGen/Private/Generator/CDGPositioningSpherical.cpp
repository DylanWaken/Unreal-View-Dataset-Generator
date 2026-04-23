// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generator/CDGPositioningSpherical.h"
#include "Trajectory/CDGTrajectorySubsystem.h"
#include "Anchor/CDGCharacterAnchor.h"
#include "LogCameraDatasetGen.h"

#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"

// ==================== IDENTITY ====================

FName UCDGPositioningSpherical::GetGeneratorName_Implementation() const
{
	return FName("SphericalPositioning");
}

FText UCDGPositioningSpherical::GetTip_Implementation() const
{
	return NSLOCTEXT("CDGGenerators", "SphericalPositioningTip",
		"Samples camera viewpoints uniformly from a spherical shell around the "
		"subject's focused anchor. Each position passes line-of-sight checks "
		"against world geometry before being forwarded to the Movement stage.");
}

// ==================== GENERATION ====================

TArray<FCDGCameraPlacement> UCDGPositioningSpherical::GeneratePlacements_Implementation()
{
	TArray<FCDGCameraPlacement> Placements;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGPositioningSpherical: No valid world context."));
		return Placements;
	}

	if (!PrimaryCharacterActor)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGPositioningSpherical: PrimaryCharacterActor is not set."));
		return Placements;
	}

	if (RadiusMax < RadiusMin || RadiusMax <= 0.0f)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGPositioningSpherical: Invalid radius range [%.1f, %.1f]. "
			     "RadiusMax must be >= RadiusMin and > 0."),
			RadiusMin, RadiusMax);
		return Placements;
	}

	UCDGTrajectorySubsystem* Subsystem = World->GetSubsystem<UCDGTrajectorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraDatasetGen, Error,
			TEXT("UCDGPositioningSpherical: UCDGTrajectorySubsystem not found in world."));
		return Placements;
	}

	const int32 SeedToUse = (RandomSeed < 0) ? FMath::Rand() : RandomSeed;
	FRandomStream RNG(SeedToUse);

	UE_LOG(LogCameraDatasetGen, Log,
		TEXT("UCDGPositioningSpherical: Count=%d Radius=[%.1f,%.1f] Seed=%d"),
		Count, RadiusMin, RadiusMax, SeedToUse);

	// Anchor center — first-frame world position of the focused anchor
	const FVector AnchorCenter = GetCurrentAnchorWorldLocation();

	// ---- Detect floor Z ----
	float FloorZ = PrimaryCharacterActor->GetActorLocation().Z;
	{
		const FVector SweepOrigin = PrimaryCharacterActor->GetActorLocation() + FVector(0.f, 0.f, 200.f);
		const FVector SweepEnd   = PrimaryCharacterActor->GetActorLocation() - FVector(0.f, 0.f, 5000.f);

		FCollisionQueryParams FloorParams(TEXT("CDGSphericalPos_Floor"), false);
		FloorParams.AddIgnoredActor(PrimaryCharacterActor.Get());

		FCollisionObjectQueryParams FloorObjParams;
		FloorObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
		FloorObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		FHitResult FloorHit;
		if (World->SweepSingleByObjectType(
			FloorHit, SweepOrigin, SweepEnd, FQuat::Identity,
			FloorObjParams, FCollisionShape::MakeSphere(CameraCollisionRadius), FloorParams)
			&& FloorHit.bBlockingHit)
		{
			FloorZ = FloorHit.ImpactPoint.Z;
		}
	}

	// ---- Sampling loop ----
	int32 GeneratedCount = 0;
	int32 TotalAttempts  = 0;
	const int32 MaxAttempts = Count * MaxSamplingAttemptsPerShot;

	while (GeneratedCount < Count && TotalAttempts < MaxAttempts)
	{
		++TotalAttempts;

		const FVector Candidate = SampleUniformSphericalShell(AnchorCenter, RadiusMin, RadiusMax, RNG);

		if (Candidate.Z < FloorZ - CameraCollisionRadius)
		{
			continue;
		}

		if (!HasClearLineOfSight(World, Candidate, AnchorCenter))
		{
			continue;
		}

		FCDGCameraPlacement Placement;
		Placement.Position     = Candidate;
		Placement.TrajectoryName = Subsystem->GenerateUniqueTrajectoryName(TEXT("SphericalPos"));

		Placements.Add(Placement);
		++GeneratedCount;
	}

	if (GeneratedCount < Count)
	{
		UE_LOG(LogCameraDatasetGen, Warning,
			TEXT("UCDGPositioningSpherical: Only generated %d / %d placements after %d attempts."),
			GeneratedCount, Count, TotalAttempts);
	}
	else
	{
		UE_LOG(LogCameraDatasetGen, Log,
			TEXT("UCDGPositioningSpherical: Generated %d placements."), GeneratedCount);
	}

	return Placements;
}

// ==================== HELPERS ====================

FVector UCDGPositioningSpherical::GetCurrentAnchorWorldLocation() const
{
	if (!PrimaryCharacterActor)
	{
		return FVector::ZeroVector;
	}

	TArray<UCDGCharacterAnchor*> AnchorComponents;
	PrimaryCharacterActor->GetComponents<UCDGCharacterAnchor>(AnchorComponents);

	for (UCDGCharacterAnchor* Anchor : AnchorComponents)
	{
		if (Anchor && Anchor->Type == FocusedAnchor)
		{
			return Anchor->GetComponentLocation();
		}
	}

	UE_LOG(LogCameraDatasetGen, Warning,
		TEXT("UCDGPositioningSpherical: No UCDGCharacterAnchor of requested type found on '%s'. "
		     "Using actor root location."),
		*PrimaryCharacterActor->GetActorLabel());

	return PrimaryCharacterActor->GetActorLocation();
}

FVector UCDGPositioningSpherical::SampleUniformSphericalShell(
	const FVector& Center, float MinR, float MaxR, FRandomStream& RNG) const
{
	const double R3Min = (double)MinR * (double)MinR * (double)MinR;
	const double R3Max = (double)MaxR * (double)MaxR * (double)MaxR;
	const double U     = (double)RNG.GetFraction();
	const float  R     = (float)FMath::Pow(R3Min + U * (R3Max - R3Min), 1.0 / 3.0);

	const float CosTheta = RNG.FRandRange(-1.0f, 1.0f);
	const float SinTheta = FMath::Sqrt(FMath::Max(0.0f, 1.0f - CosTheta * CosTheta));
	const float Phi      = RNG.FRandRange(0.0f, 2.0f * PI);

	const FVector Direction(
		SinTheta * FMath::Cos(Phi),
		SinTheta * FMath::Sin(Phi),
		CosTheta);

	return Center + Direction * R;
}

bool UCDGPositioningSpherical::HasClearLineOfSight(
	UWorld* World, const FVector& From, const FVector& To) const
{
	FCollisionQueryParams QueryParams(TEXT("CDGSphericalPos_LOS"), false);
	QueryParams.bReturnPhysicalMaterial = false;
	if (PrimaryCharacterActor)
	{
		QueryParams.AddIgnoredActor(PrimaryCharacterActor.Get());
	}

	const FCollisionShape ProbeShape = FCollisionShape::MakeSphere(CameraCollisionRadius);

	FHitResult CameraHit;
	if (World->SweepSingleByChannel(
		CameraHit, To, From, FQuat::Identity, ECC_Camera, ProbeShape, QueryParams))
	{
		return false;
	}

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult ObjectHit;
	if (World->SweepSingleByObjectType(
		ObjectHit, To, From, FQuat::Identity, ObjParams, ProbeShape, QueryParams)
		&& ObjectHit.bBlockingHit)
	{
		return false;
	}

	return true;
}

// ==================== SERIALIZATION ====================

void UCDGPositioningSpherical::SerializeGeneratorConfig(TSharedPtr<FJsonObject>& OutJson) const
{
	if (!OutJson.IsValid())
	{
		OutJson = MakeShared<FJsonObject>();
	}

	OutJson->SetNumberField(TEXT("Count"),                     (double)Count);
	OutJson->SetNumberField(TEXT("RadiusMin"),                 (double)RadiusMin);
	OutJson->SetNumberField(TEXT("RadiusMax"),                 (double)RadiusMax);
	OutJson->SetNumberField(TEXT("MaxSamplingAttemptsPerShot"),(double)MaxSamplingAttemptsPerShot);
	OutJson->SetNumberField(TEXT("RandomSeed"),                (double)RandomSeed);
	OutJson->SetNumberField(TEXT("CameraCollisionRadius"),     (double)CameraCollisionRadius);
	OutJson->SetNumberField(TEXT("FocusedAnchor"),             (double)(uint8)FocusedAnchor);
}

void UCDGPositioningSpherical::FetchGeneratorConfig(const TSharedPtr<FJsonObject>& InJson)
{
	if (!InJson.IsValid()) return;

	if (InJson->HasField(TEXT("Count")))
		Count = FMath::Max(1, (int32)InJson->GetNumberField(TEXT("Count")));
	if (InJson->HasField(TEXT("RadiusMin")))
		RadiusMin = (float)InJson->GetNumberField(TEXT("RadiusMin"));
	if (InJson->HasField(TEXT("RadiusMax")))
		RadiusMax = (float)InJson->GetNumberField(TEXT("RadiusMax"));
	if (InJson->HasField(TEXT("MaxSamplingAttemptsPerShot")))
		MaxSamplingAttemptsPerShot = FMath::Max(1, (int32)InJson->GetNumberField(TEXT("MaxSamplingAttemptsPerShot")));
	if (InJson->HasField(TEXT("RandomSeed")))
		RandomSeed = (int32)InJson->GetNumberField(TEXT("RandomSeed"));
	if (InJson->HasField(TEXT("CameraCollisionRadius")))
		CameraCollisionRadius = FMath::Max(1.0f, (float)InJson->GetNumberField(TEXT("CameraCollisionRadius")));
	if (InJson->HasField(TEXT("FocusedAnchor")))
	{
		const uint8 AnchorVal    = (uint8)InJson->GetNumberField(TEXT("FocusedAnchor"));
		const uint8 MaxAnchorVal = (uint8)AnchorType::CDG_ANCHOR_HAND_RIGHT;
		if (AnchorVal <= MaxAnchorVal)
		{
			FocusedAnchor = (AnchorType)AnchorVal;
		}
	}
}
