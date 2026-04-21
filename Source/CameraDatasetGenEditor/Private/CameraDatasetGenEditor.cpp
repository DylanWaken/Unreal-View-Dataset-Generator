// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraDatasetGenEditor.h"
#include "UI/TopButton/TopButtonStyle.h"
#include "UI/TopButton/TopButton.h"
#include "UI/KeyFrameEditor/CDGKeyframeContextMenu.h"
#include "UI/CameraPreviewEditor/CDGCameraPreviewContextMenu.h"
#include "LogCameraDatasetGenEditor.h"
#include "Config/LevelSeqExportConfig.h"
#include "Config/GeneratorStackConfig.h"
#include "Config/BatchProcConfig.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetTypeActions_Base.h"
#include "AssetTypeCategories.h"

#define LOCTEXT_NAMESPACE "FCameraDatasetGenEditorModule"

// ---------------------------------------------------------------------------
// Asset type actions
// ---------------------------------------------------------------------------

class FLevelSeqExportConfigTypeActions : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override
	{
		return NSLOCTEXT("AssetTypeActions", "LevelSeqExportConfig", "Level Seq Export Config");
	}
	virtual FColor GetTypeColor() const override { return FColor(64, 130, 255); }
	virtual UClass* GetSupportedClass() const override { return ULevelSeqExportConfig::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};

class FGeneratorStackConfigTypeActions : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override
	{
		return NSLOCTEXT("AssetTypeActions", "GeneratorStackConfig", "Generator Stack Config");
	}
	virtual FColor GetTypeColor() const override { return FColor(80, 200, 120); }
	virtual UClass* GetSupportedClass() const override { return UGeneratorStackConfig::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};

class FBatchProcConfigTypeActions : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override
	{
		return NSLOCTEXT("AssetTypeActions", "BatchProcConfig", "Batch Proc Config");
	}
	virtual FColor GetTypeColor() const override { return FColor(220, 140, 60); }
	virtual UClass* GetSupportedClass() const override { return UBatchProcConfig::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};

// ---------------------------------------------------------------------------

void FCameraDatasetGenEditorModule::StartupModule()
{
	// Initialize the button style (icon management)
	FTopButtonStyle::Initialize();
	
	// Register the custom icon BEFORE creating the button
	FTopButtonStyle::SetSVGIcon("CustomIcon", "Icons/CamData");
	
	// Create the toolbar button with the custom icon
	TopButton = MakeUnique<FTopButton>("CustomIcon");

	// Register asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		LevelSeqExportConfigTypeActions = MakeShareable(new FLevelSeqExportConfigTypeActions());
		AssetTools.RegisterAssetTypeActions(LevelSeqExportConfigTypeActions.ToSharedRef());

		GeneratorStackConfigTypeActions = MakeShareable(new FGeneratorStackConfigTypeActions());
		AssetTools.RegisterAssetTypeActions(GeneratorStackConfigTypeActions.ToSharedRef());

		BatchProcConfigTypeActions = MakeShareable(new FBatchProcConfigTypeActions());
		AssetTools.RegisterAssetTypeActions(BatchProcConfigTypeActions.ToSharedRef());
	}

	// Register a callback to initialize the context menu once the LevelEditor module is loaded
	// This is necessary because LevelEditor may not be loaded yet during StartupModule
	ModuleLoadedDelegateHandle = FModuleManager::Get().OnModulesChanged().AddRaw(
		this, &FCameraDatasetGenEditorModule::OnModulesChanged);

	// If LevelEditor is already loaded, initialize the context menu immediately
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		OnModulesChanged("LevelEditor", EModuleChangeReason::ModuleLoaded);
	}
}

void FCameraDatasetGenEditorModule::ShutdownModule()
{
	// Unregister the module loaded delegate
	if (ModuleLoadedDelegateHandle.IsValid())
	{
		FModuleManager::Get().OnModulesChanged().Remove(ModuleLoadedDelegateHandle);
		ModuleLoadedDelegateHandle.Reset();
	}

	// Unregister asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		if (LevelSeqExportConfigTypeActions.IsValid())
		{
			AssetTools.UnregisterAssetTypeActions(LevelSeqExportConfigTypeActions.ToSharedRef());
			LevelSeqExportConfigTypeActions.Reset();
		}
		if (GeneratorStackConfigTypeActions.IsValid())
		{
			AssetTools.UnregisterAssetTypeActions(GeneratorStackConfigTypeActions.ToSharedRef());
			GeneratorStackConfigTypeActions.Reset();
		}
		if (BatchProcConfigTypeActions.IsValid())
		{
			AssetTools.UnregisterAssetTypeActions(BatchProcConfigTypeActions.ToSharedRef());
			BatchProcConfigTypeActions.Reset();
		}
	}

	// Shutdown the camera preview context menu
	if (CameraPreviewContextMenu.IsValid())
	{
		CameraPreviewContextMenu->Shutdown();
		CameraPreviewContextMenu.Reset();
	}

	// Shutdown the keyframe context menu
	if (KeyframeContextMenu.IsValid())
	{
		KeyframeContextMenu->Shutdown();
		KeyframeContextMenu.Reset();
	}

	// Clean up the toolbar button
	TopButton.Reset();
	
	// Shutdown the button style
	FTopButtonStyle::Shutdown();
}

void FCameraDatasetGenEditorModule::OnModulesChanged(FName InModuleName, EModuleChangeReason InChangeReason)
{
	if (InModuleName == "LevelEditor" && InChangeReason == EModuleChangeReason::ModuleLoaded)
	{
		// Initialize the keyframe context menu now that LevelEditor is loaded
		if (!KeyframeContextMenu.IsValid())
		{
			KeyframeContextMenu = MakeShared<FCDGKeyframeContextMenu>();
			KeyframeContextMenu->Initialize();
			
			UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGKeyframe context menu registered successfully"));
		}

		// Initialize the camera preview context menu now that LevelEditor is loaded
		if (!CameraPreviewContextMenu.IsValid())
		{
			CameraPreviewContextMenu = MakeShared<FCDGCameraPreviewContextMenu>();
			CameraPreviewContextMenu->Initialize();
			
			UE_LOG(LogCameraDatasetGenEditor, Log, TEXT("CDGCameraPreview context menu registered successfully"));
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCameraDatasetGenEditorModule, CameraDatasetGenEditor)

