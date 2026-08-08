// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class JrpgCombatTestingEditorTarget : TargetRules
{
	public JrpgCombatTestingEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// V7 matches the settings the installed 5.8 UnrealEditor was built with. An editor
		// target sharing build products with it may not differ on the warning-level
		// properties, so V5 fails outright here.
		DefaultBuildSettings = BuildSettingsVersion.V7;
		// Left at 5_6 to avoid include-order churn during the 5.8 migration. Deprecated:
		// unsupported from 5.9, so this needs bumping before the next engine upgrade.
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.Add("JrpgCombatTesting");
	}
}
