// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"

class FSlateStyleSet;
class ISlateStyle;

/**
 * Style manager for the top toolbar button
 * Handles icon registration and slate style management
 */
class FTopButtonStyle
{
public:
	/** Initialize the style set and register with Slate */
	static void Initialize();
	
	/** Unregister the style set and clean up */
	static void Shutdown();

	/** Get the style set instance */
	static const ISlateStyle& Get();
	
	/** Get the style set name */
	static FName GetStyleSetName();

	/** Set a PNG/image icon for the button */
	static void SetIcon(const FString& StyleName, const FString& ResourcePath);
	
	/** Set an SVG icon for the button */
	static void SetSVGIcon(const FString& StyleName, const FString& ResourcePath);

private:
	/** Create the style set */
	static TUniquePtr<FSlateStyleSet> Create();
	
	/** The style set instance */
	static TUniquePtr<FSlateStyleSet> StyleInstance;
};

