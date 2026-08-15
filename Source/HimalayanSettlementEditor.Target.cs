using UnrealBuildTool;
using System.Collections.Generic;

public class HimalayanSettlementEditorTarget : TargetRules
{
	public HimalayanSettlementEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HimalayanSettlement");
		ExtraModuleNames.Add("HimalayanSettlementEditor");
	}
}
