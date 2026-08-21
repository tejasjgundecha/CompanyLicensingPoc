using UnrealBuildTool;

public class CompanyLicensing : ModuleRules
{
	public CompanyLicensing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"ApplicationCore"
		});

		// Engine-provided OpenSSL. Used only to verify Ed25519 signatures.
		// The private signing key is never linked into this module.
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
	}
}
