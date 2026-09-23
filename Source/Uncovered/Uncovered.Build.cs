// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Uncovered : ModuleRules
{
	public Uncovered(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"UMG",
			"NavigationSystem",
			"LevelSequence",
			"AIModule",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore" });
		
		PublicIncludePaths.AddRange(new string[] { "Uncovered" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
