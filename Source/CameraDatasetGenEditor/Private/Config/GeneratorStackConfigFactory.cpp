// Copyright Epic Games, Inc. All Rights Reserved.

#include "Config/GeneratorStackConfigFactory.h"
#include "Config/GeneratorStackConfig.h"

UGeneratorStackConfigFactory::UGeneratorStackConfigFactory()
{
	SupportedClass = UGeneratorStackConfig::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UGeneratorStackConfigFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UGeneratorStackConfig>(InParent, InClass, InName, Flags);
}
