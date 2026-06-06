// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class JrpgCombatTesting : ModuleRules
{
	public JrpgCombatTesting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Each UI .cpp keeps its own anonymous-namespace helpers (ColText, Font,
		// etc.); unity bundling would merge them into one TU and collide. Keep
		// this module non-unity so those stay file-local.
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "NavigationSystem", "JRPGCombat" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
