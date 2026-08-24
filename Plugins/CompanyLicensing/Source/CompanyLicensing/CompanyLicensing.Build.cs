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

		// Prevent OpenSSL from defining a UI identifier that conflicts with Unreal's UI
		// (ossl_typ.h typedef struct ui_st UI;). Define this for all translation units
		// in this module when compiling on Windows.
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PublicDefinitions.Add("OPENSSL_NO_UI");
			PublicDefinitions.Add("OPENSSL_NO_UI_CONSOLE");
		}
	}
}
