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

	/**
	 * Called when "Initialize Level Sequence" is clicked
	 * Creates the unique Level Sequence for this level if it doesn't exist
	 */
	static void OnInitLevelSequence();

	/**
	 * Called when "Delete Level Sequence" is clicked
	 * Deletes the unique Level Sequence for this level
	 */
	static void OnDeleteLevelSequence();

	/**
	 * Called when "Export to Level Sequence" is clicked
	 * Opens the export window
	 */
	static void OnExportToLevelSequence();
};
