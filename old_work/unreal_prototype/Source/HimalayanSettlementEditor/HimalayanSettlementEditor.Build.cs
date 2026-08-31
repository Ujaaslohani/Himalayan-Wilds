using UnrealBuildTool;

public class HimalayanSettlementEditor : ModuleRules
{
	public HimalayanSettlementEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry",
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"HimalayanSettlement",
			"UnrealEd"
		});
	}
}
