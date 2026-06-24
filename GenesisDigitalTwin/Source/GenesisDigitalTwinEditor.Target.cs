using UnrealBuildTool;

public class GenesisDigitalTwinEditorTarget : TargetRules
{
	public GenesisDigitalTwinEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
	}
}
