#pragma once

#include "CoreMinimal.h"
#include "LicenseState.h"
#include "LicenseTypes.h"

/**
 * Owns startup license validation for the packaged application.
 * Invoked from the CompanyLicensing plugin at PostConfigInit.
 */
class COMPANYLICENSING_API FLicenseManager
{
public:
	static FLicenseManager& Get();

	/** Load, verify, and enforce the local license. Safe to call once at startup. */
	void InitializeAndEnforce();

	ELicenseState GetState() const { return State; }
	const FLicensePayload& GetPayload() const { return Payload; }
	FString GetLicenseFilePath() const { return LicenseFilePath; }

	static FDateTime GetClockNowUtc();

private:
	FLicenseManager() = default;

	bool TryLoadLicenseFile(FLicensePayload& OutPayload, FString& OutPath) const;
	static bool ParseLicenseJson(const FString& JsonText, FLicensePayload& OutPayload);
	void ShowSuccessDialog(ELicenseState SuccessState) const;
	void FailAndExit(ELicenseState FailedState) const;

	ELicenseState State = ELicenseState::LicenseInvalid;
	FLicensePayload Payload;
	FString LicenseFilePath;
	bool bInitialized = false;
};
