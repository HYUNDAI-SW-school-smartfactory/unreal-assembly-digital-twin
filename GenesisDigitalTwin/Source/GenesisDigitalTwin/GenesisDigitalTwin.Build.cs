using UnrealBuildTool;

public class GenesisDigitalTwin : ModuleRules
{
	public GenesisDigitalTwin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"FF_MQTT_Sync"
		});
	}
}
