// Copyright Epic Games, Inc. All Rights Reserved.

#include "Anchor/CDGLevelSceneAnchor.h"
#include "Anchor/CDGSceneAnchorVisualizer.h"
#include "Components/SphereComponent.h"

ACDGLevelSceneAnchor::ACDGLevelSceneAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

#if WITH_EDITORONLY_DATA
	Visualizer = CreateDefaultSubobject<UCDGSceneAnchorVisualizer>(TEXT("AnchorVisualizer"));
	if (Visualizer)
	{
		Visualizer->SetupAttachment(RootComponent);
		Visualizer->bHiddenInGame = true;
		Visualizer->DispersionRadius = DispersionRadius;
		Visualizer->AnchorColor      = AnchorColor;
	}

	SelectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SelectionSphere"));
	if (SelectionSphere)
	{
		SelectionSphere->SetupAttachment(RootComponent);
		SelectionSphere->SetSphereRadius(30.f);
		SelectionSphere->SetHiddenInGame(true);
		SelectionSphere->SetVisibility(false);
		SelectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SelectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		SelectionSphere->bIsEditorOnly = true;
		SelectionSphere->ShapeColor = FColor::Transparent;
	}
#endif

	SetActorHiddenInGame(true);
}

void ACDGLevelSceneAnchor::BeginPlay()
{
	Super::BeginPlay();
	SetActorHiddenInGame(true);
}

void ACDGLevelSceneAnchor::SyncVisualizer()
{
#if WITH_EDITORONLY_DATA
	if (Visualizer)
	{
		Visualizer->UpdateVisualization();
	}
#endif
}

#if WITH_EDITOR
void ACDGLevelSceneAnchor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SyncVisualizer();
}

void ACDGLevelSceneAnchor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	SyncVisualizer();
}
#endif
