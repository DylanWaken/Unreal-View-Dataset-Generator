// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

/**
 * Manages dropdown menu content for the top toolbar button
 * Provides actions like adding new keyframes
 */
class FTopButtonDropdown
{
public:
	/**
	 * Creates the dropdown menu widget
	 * @return The dropdown menu widget
	 */
	static TSharedRef<SWidget> MakeDropdownMenu();

private:
	/**
	 * Called when "Add New Keyframe" is clicked
	 * Spawns a new CDGKeyframe actor in the editor world
	 */
	static void OnAddNewKeyframe();
};

