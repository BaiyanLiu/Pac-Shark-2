// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PacShark : ModuleRules
{
	public PacShark(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		// RenderCore: the white texture used for batched canvas triangles.
		PrivateDependencyModuleNames.AddRange(new string[] { "MediaAssets", "AudioMixer", "ImageCore", "ApplicationCore", "RenderCore" });
	}
}
