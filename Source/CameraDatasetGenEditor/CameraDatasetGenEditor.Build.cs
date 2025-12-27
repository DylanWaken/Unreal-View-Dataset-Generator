// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CameraDatasetGenEditor : ModuleRules
{
	public CameraDatasetGenEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CameraDatasetGen",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
	PrivateDependencyModuleNames.AddRange(
		new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"LevelEditor",
			"Projects",
			"InputCore",
			"ToolMenus",  // Required for UToolMenus toolbar extension
			"PropertyEditor",  // Required for details panel access
			"RenderCore",  // Required for GWhiteTexture
			// ... add private dependencies that you statically link with here ...	
		}
		);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}

