using UnrealBuildTool;

public class HimalayanSettlement : ModuleRules
{
	public HimalayanSettlement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AIModule",
			"GameplayTasks",
			"NavigationSystem"
		});
	}
}
