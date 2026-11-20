// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Tandem : ModuleRules
{
	public Tandem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"PhysicsCore",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Tandem",
			"Tandem/Vehicle",
			"Tandem/Variant_Platforming",
			"Tandem/Variant_Platforming/Animation",
			"Tandem/Variant_Combat",
			"Tandem/Variant_Combat/AI",
			"Tandem/Variant_Combat/Animation",
			"Tandem/Variant_Combat/Gameplay",
			"Tandem/Variant_Combat/Interfaces",
			"Tandem/Variant_Combat/UI",
			"Tandem/Variant_SideScrolling",
			"Tandem/Variant_SideScrolling/AI",
			"Tandem/Variant_SideScrolling/Gameplay",
			"Tandem/Variant_SideScrolling/Interfaces",
			"Tandem/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
