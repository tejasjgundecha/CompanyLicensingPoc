#pragma once

#include "CoreMinimal.h"
#include "LicenseState.h"
#include "LicenseTypes.h"
#include "Containers/Ticker.h"

/**
 * Owns startup license validation and runtime session lifecycle for the packaged application.
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

	/** Cryptographic session key (F1). Zeroed or dummy if license is invalid or bypassed. */
	const TArray<uint8>& GetSessionKey() const { return SessionKey; }
	bool HasValidSessionKey() const;

	/** Distributed runtime re-validation (F4). Can be invoked before gated features. */
	bool RevalidateSession();

	static FDateTime GetClockNowUtc();

private:
	FLicenseManager() = default;
	~FLicenseManager();

	bool TryLoadLicenseFile(FLicensePayload& OutPayload, FString& OutPath) const;
	static bool ParseLicenseJson(const FString& JsonText, FLicensePayload& OutPayload);
	void ShowSuccessDialog(ELicenseState SuccessState) const;
	void FailAndExit(ELicenseState FailedState) const;

	bool OnPeriodicRevalidation(float DeltaTime);

	ELicenseState State = ELicenseState::LicenseInvalid;
	FLicensePayload Payload;
	FString LicenseFilePath;
	TArray<uint8> SessionKey;
	FTSTickerDelegateHandle PeriodicTickerHandle;
	bool bInitialized = false;
};
