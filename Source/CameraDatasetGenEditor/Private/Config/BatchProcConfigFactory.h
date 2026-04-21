// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "BatchProcConfigFactory.generated.h"

/**
 * Factory that allows UBatchProcConfig assets to be created from the Content Browser.
 */
UCLASS()
class UBatchProcConfigFactory : public UFactory
{
	GENERATED_BODY()

public:
	UBatchProcConfigFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override { return true; }
};
