#pragma once

#include "CoreMinimal.h"
#include "LicenseState.h"
#include "LicenseTypes.h"

class COMPANYLICENSING_API FLicenseValidator
{
public:
	static ELicenseState Validate(const FLicensePayload& Payload, const FDateTime& NowUtc);

	/** Cryptographically derive session key material (F1) using HKDF. */
	static void DeriveSessionKey(
		const FLicensePayload& Payload,
		const TArray<uint8>& SignatureBytes,
		bool bIsValid,
		TArray<uint8>& OutSessionKey);
};
