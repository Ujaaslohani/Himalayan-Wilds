using UnrealBuildTool;

public class HimalayanSettlement : ModuleRules
{
	public HimalayanSettlement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AIModule",
			"GameplayTasks",
			"NavigationSystem"
		});
	}
}
