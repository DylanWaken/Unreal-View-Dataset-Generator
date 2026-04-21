// Copyright Epic Games, Inc. All Rights Reserved.

#include "Config/LevelSeqExportConfigFactory.h"
#include "Config/LevelSeqExportConfig.h"
#include "Misc/Paths.h"

ULevelSeqExportConfigFactory::ULevelSeqExportConfigFactory()
{
	SupportedClass = ULevelSeqExportConfig::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* ULevelSeqExportConfigFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	ULevelSeqExportConfig* Config = NewObject<ULevelSeqExportConfig>(InParent, InClass, InName, Flags);
	Config->OutputDirectory = FPaths::ProjectSavedDir() / TEXT("MovieRenders");
	return Config;
}
