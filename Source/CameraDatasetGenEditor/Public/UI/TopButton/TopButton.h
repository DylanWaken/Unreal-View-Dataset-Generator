// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Manages a custom button on the Level Editor's top toolbar
 * This class handles registration, UI creation, and cleanup
 */
class FTopButton
{
public:
	/** Constructor - sets up the toolbar extension 
	 * @param IconStyleName - The icon style name to use (defaults to "Icon")
	 */
	FTopButton(const FString& IconStyleName = TEXT("Icon"));
	
	/** Destructor - cleans up the toolbar extension */
	~FTopButton();

private:
	/** Extend the level editor toolbar with our button */
	void ExtendLevelEditorToolbar();
	
	/** Called when the button is clicked - opens the dropdown menu */
	TSharedRef<SWidget> OnGetMenuContent();

private:
	/** The current icon style name being used */
	FString CurrentIconStyleName;
};

