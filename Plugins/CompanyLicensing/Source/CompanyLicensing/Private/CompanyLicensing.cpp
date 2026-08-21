#include "CompanyLicensing.h"
#include "LicenseManager.h"

#define LOCTEXT_NAMESPACE "FCompanyLicensingModule"

void FCompanyLicensingModule::StartupModule()
{
	FLicenseManager::Get().InitializeAndEnforce();
}

void FCompanyLicensingModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCompanyLicensingModule, CompanyLicensing)
