#include "LicenseValidator.h"
#include "SignatureVerifier.h"
#include "MachineFingerprint.h"
#include "Misc/Base64.h"

#if PLATFORM_WINDOWS
#define OPENSSL_NO_UI_CONSOLE
#define OPENSSL_NO_UI
#define UI OPENSSL_UI
#endif

#include <openssl/hmac.h>
#include <openssl/evp.h>

#if PLATFORM_WINDOWS
#undef UI
#undef OPENSSL_NO_UI
#undef OPENSSL_NO_UI_CONSOLE
#endif

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

void FLicenseValidator::DeriveSessionKey(
	const FLicensePayload& Payload,
	const TArray<uint8>& SignatureBytes,
	bool bIsValid,
	TArray<uint8>& OutSessionKey)
{
	OutSessionKey.SetNumZeroed(32);

	// Fixed build salt for session key derivation
	static const uint8 SessionSalt[16] = {
		0x43, 0x6f, 0x6d, 0x70, 0x61, 0x6e, 0x79, 0x4c,
		0x69, 0x63, 0x53, 0x61, 0x6c, 0x74, 0x5f, 0x31
	};

	// If the state is not valid or signature is malformed, feed dummy/zeroed material
	// so the derived key is wrong/invalid without giving an attacker an early exit gate.
	TArray<uint8> Ikm;
	if (bIsValid && SignatureBytes.Num() == 64)
	{
		Ikm.Append(SignatureBytes);
		FTCHARToUTF8 LicenseIdUtf8(*Payload.LicenseId);
		Ikm.Append(reinterpret_cast<const uint8*>(LicenseIdUtf8.Get()), LicenseIdUtf8.Length());
		FTCHARToUTF8 FingerprintUtf8(*Payload.MachineFingerprint);
		Ikm.Append(reinterpret_cast<const uint8*>(FingerprintUtf8.Get()), FingerprintUtf8.Length());
	}
	else
	{
		// Constant dummy noise for failure path
		Ikm.SetNumZeroed(32);
	}

	// HKDF-Extract: PRK = HMAC-Hash(Salt, IKM)
	uint8 Prk[EVP_MAX_MD_SIZE] = {0};
	uint32 PrkLen = 0;
	HMAC(EVP_sha256(),
		SessionSalt, sizeof(SessionSalt),
		Ikm.GetData(), Ikm.Num(),
		Prk, &PrkLen);

	// HKDF-Expand: OKM = HMAC-Hash(PRK, info || 0x01)
	const char* InfoStr = "CompanyLicensing/SessionKey/v1";
	TArray<uint8> InfoWithCounter;
	InfoWithCounter.Append(reinterpret_cast<const uint8*>(InfoStr), strlen(InfoStr));
	InfoWithCounter.Add(0x01);

	uint8 Okm[EVP_MAX_MD_SIZE] = {0};
	uint32 OkmLen = 0;
	HMAC(EVP_sha256(),
		Prk, PrkLen,
		InfoWithCounter.GetData(), InfoWithCounter.Num(),
		Okm, &OkmLen);

	// Wipe PRK
	FMemory::Memzero(Prk, sizeof(Prk));

	if (OkmLen >= 32)
	{
		FMemory::Memcpy(OutSessionKey.GetData(), Okm, 32);
	}
	FMemory::Memzero(Okm, sizeof(Okm));
}
