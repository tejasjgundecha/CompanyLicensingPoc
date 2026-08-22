#pragma once

#include "CoreMinimal.h"

struct FLicensePayload
{
	FString LicenseId;
	FString ProjectId;
	FString IssuedAt;
	FString SubscriptionValidUntil;
	FString OfflineGraceUntil;
	FString LastValidation;
	FString MachineFingerprint;
	FString SignatureBase64;

	bool bLoadedFromDisk = false;
};

namespace CompanyLicensing
{
	/** Must match the expected identifier compiled into the application. */
	inline const TCHAR* ExpectedProjectId()
	{
		return TEXT("demo-project");
	}

	/** Canonical UTF-8 payload that is signed/verified. Does not include signature. */
	inline FString BuildCanonicalPayload(const FLicensePayload& Payload)
	{
		return FString::Printf(
			TEXT("licenseId=%s\nprojectId=%s\nissuedAt=%s\nsubscriptionValidUntil=%s\nofflineGraceUntil=%s\nlastValidation=%s\nmachineFingerprint=%s\n"),
			*Payload.LicenseId,
			*Payload.ProjectId,
			*Payload.IssuedAt,
			*Payload.SubscriptionValidUntil,
			*Payload.OfflineGraceUntil,
			*Payload.LastValidation,
			*Payload.MachineFingerprint);
	}
}
