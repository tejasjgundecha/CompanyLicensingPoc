#include "LicenseValidator.h"
#include "SignatureVerifier.h"
#include "MachineFingerprint.h"
#include "Misc/Base64.h"

namespace
{
	bool ParseIsoUtc(const FString& Text, FDateTime& OutDateTime)
	{
		if (Text.IsEmpty())
		{
			return false;
		}
		return FDateTime::ParseIso8601(*Text, OutDateTime);
	}
}

ELicenseState FLicenseValidator::Validate(const FLicensePayload& Payload, const FDateTime& NowUtc)
{
	if (!Payload.bLoadedFromDisk)
	{
		return ELicenseState::LicenseMissing;
	}

	TArray<uint8> SignatureBytes;
	if (!FBase64::Decode(Payload.SignatureBase64, SignatureBytes) || SignatureBytes.Num() != 64)
	{
		return ELicenseState::LicenseInvalid;
	}

	const FString Canonical = CompanyLicensing::BuildCanonicalPayload(Payload);
	FTCHARToUTF8 CanonicalUtf8(*Canonical);
	TArray<uint8> CanonicalBytes;
	CanonicalBytes.Append(reinterpret_cast<const uint8*>(CanonicalUtf8.Get()), CanonicalUtf8.Length());

	if (!FSignatureVerifier::VerifyEd25519(CanonicalBytes, SignatureBytes))
	{
		return ELicenseState::LicenseTampered;
	}

	if (Payload.ProjectId != CompanyLicensing::ExpectedProjectId())
	{
		return ELicenseState::LicenseInvalid;
	}

	// Machine fingerprint check (Windows only, skipped if license has no fingerprint)
#if PLATFORM_WINDOWS
	if (!Payload.MachineFingerprint.IsEmpty())
	{
		FString CurrentFingerprint = FMachineFingerprint::ComputeFingerprint();
		if (CurrentFingerprint != Payload.MachineFingerprint)
		{
			return ELicenseState::MachineMismatch;
		}
	}
#endif

	FDateTime SubscriptionEnd;
	FDateTime GraceEnd;
	if (!ParseIsoUtc(Payload.SubscriptionValidUntil, SubscriptionEnd) ||
		!ParseIsoUtc(Payload.OfflineGraceUntil, GraceEnd))
	{
		return ELicenseState::LicenseInvalid;
	}

	if (NowUtc > SubscriptionEnd)
	{
		return ELicenseState::LicenseExpired;
	}

	if (NowUtc > GraceEnd)
	{
		return ELicenseState::OfflineGraceExpired;
	}

	FDateTime LastValidation;
	const bool bHasLastValidation = ParseIsoUtc(Payload.LastValidation, LastValidation);
	if (bHasLastValidation && NowUtc <= GraceEnd && LastValidation < NowUtc && NowUtc <= SubscriptionEnd)
	{
		// Signed lastValidation is in the past, but we are still inside the offline window.
		return ELicenseState::OfflineGraceValid;
	}

	return ELicenseState::LicenseValid;
}
