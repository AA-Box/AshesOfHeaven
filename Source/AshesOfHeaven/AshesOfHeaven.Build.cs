// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AshesOfHeaven : ModuleRules
{
	public AshesOfHeaven(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"AssetRegistry",
			"InputCore",
			"EnhancedInput",
			"DeveloperSettings",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"Niagara",
			"MetasoundEngine",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "AssetTools", "EditorScriptingUtilities", "UnrealEd", "UMGEditor", "NiagaraEditor", "MovieScene", "MovieSceneTracks" });
		}

		PublicIncludePaths.AddRange(new string[] {
			"AshesOfHeaven",
			"AshesOfHeaven/Variant_Horror",
			"AshesOfHeaven/Variant_Horror/UI",
			"AshesOfHeaven/Variant_Shooter",
			"AshesOfHeaven/Variant_Shooter/AI",
			"AshesOfHeaven/Variant_Shooter/UI",
			"AshesOfHeaven/Variant_Shooter/Weapons",
			"AshesOfHeaven/Gameplay"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
