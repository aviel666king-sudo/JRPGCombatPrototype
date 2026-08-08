// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class JrpgCombatTestingTarget : TargetRules
{
	public JrpgCombatTestingTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Kept in step with JrpgCombatTestingEditor.Target.cs — see the note there.
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.Add("JrpgCombatTesting");
	}
}
