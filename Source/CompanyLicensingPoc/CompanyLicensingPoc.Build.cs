using UnrealBuildTool;

public class CompanyLicensingPoc : ModuleRules
{
	public CompanyLicensingPoc(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"CompanyLicensing"
		});
	}
}
