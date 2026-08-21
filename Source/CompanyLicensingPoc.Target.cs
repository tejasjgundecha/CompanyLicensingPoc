using UnrealBuildTool;

public class CompanyLicensingPocTarget : TargetRules
{
	public CompanyLicensingPocTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CompanyLicensingPoc");
	}
}
