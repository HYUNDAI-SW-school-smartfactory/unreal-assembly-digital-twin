using UnrealBuildTool;

public class GenesisDigitalTwinTarget : TargetRules
{
	public GenesisDigitalTwinTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
	}
}
