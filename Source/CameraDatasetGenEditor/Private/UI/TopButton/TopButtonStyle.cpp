// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TopButton/TopButtonStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"
#include "Interfaces/IPluginManager.h"
#include "LogCameraDatasetGenEditor.h"

TUniquePtr<FSlateStyleSet> FTopButtonStyle::StyleInstance;

void FTopButtonStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FTopButtonStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

FName FTopButtonStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("TopButtonStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)

const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TUniquePtr<FSlateStyleSet> FTopButtonStyle::Create()
{
	TUniquePtr<FSlateStyleSet> Style = MakeUnique<FSlateStyleSet>(GetStyleSetName());
	
	// Set the content root to the plugin's Resources folder
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("CameraDatasetGen");
	if (!Plugin.IsValid())
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Failed to find CameraDatasetGen plugin!"));
		return Style;
	}
	
	FString ContentRoot = Plugin->GetBaseDir() / TEXT("Resources");
	Style->SetContentRoot(ContentRoot);
	
	// Set a default icon using the plugin's icon
	Style->Set("TopButton.Icon", new IMAGE_BRUSH("Icon128", Icon40x40));
	Style->Set("TopButton.Icon.Small", new IMAGE_BRUSH("Icon128", Icon20x20));
	
	return Style;
}

void FTopButtonStyle::SetIcon(const FString& StyleName, const FString& ResourcePath)
{
	FSlateStyleSet* Style = StyleInstance.Get();
	if (!Style)
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Style instance is null!"));
		return;
	}

	FString FullName = FString::Printf(TEXT("TopButton.%s"), *StyleName);
	Style->Set(*FullName, new IMAGE_BRUSH(ResourcePath, Icon40x40));

	FString SmallName = FString::Printf(TEXT("TopButton.%s.Small"), *StyleName);
	Style->Set(*SmallName, new IMAGE_BRUSH(ResourcePath, Icon20x20));
}

void FTopButtonStyle::SetSVGIcon(const FString& StyleName, const FString& ResourcePath)
{
	FSlateStyleSet* Style = StyleInstance.Get();
	if (!Style)
	{
		UE_LOG(LogCameraDatasetGenEditor, Error, TEXT("Style instance is null!"));
		return;
	}

	FString FullName = FString::Printf(TEXT("TopButton.%s"), *StyleName);
	Style->Set(*FullName, new IMAGE_BRUSH_SVG(ResourcePath, Icon40x40));

	FString SmallName = FString::Printf(TEXT("TopButton.%s.Small"), *StyleName);
	Style->Set(*SmallName, new IMAGE_BRUSH_SVG(ResourcePath, Icon20x20));
}

#undef IMAGE_BRUSH
#undef IMAGE_BRUSH_SVG

const ISlateStyle& FTopButtonStyle::Get()
{
	check(StyleInstance);
	return *StyleInstance;
}

