// Copyright Epic Games, Inc. All Rights Reserved.

#include "Config/BatchProcConfigFactory.h"
#include "Config/BatchProcConfig.h"

UBatchProcConfigFactory::UBatchProcConfigFactory()
{
	SupportedClass = UBatchProcConfig::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UBatchProcConfigFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UBatchProcConfig>(InParent, InClass, InName, Flags);
}
