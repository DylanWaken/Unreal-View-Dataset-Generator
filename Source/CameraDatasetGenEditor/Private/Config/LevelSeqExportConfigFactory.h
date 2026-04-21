// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "LevelSeqExportConfigFactory.generated.h"

/**
 * Factory that allows ULevelSeqExportConfig assets to be created from the Content Browser.
 */
UCLASS()
class ULevelSeqExportConfigFactory : public UFactory
{
	GENERATED_BODY()

public:
	ULevelSeqExportConfigFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override { return true; }
};
