using UnrealBuildTool;

public class CompanyLicensingPocEditorTarget : TargetRules
{
	public CompanyLicensingPocEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CompanyLicensingPoc");
	}
}
