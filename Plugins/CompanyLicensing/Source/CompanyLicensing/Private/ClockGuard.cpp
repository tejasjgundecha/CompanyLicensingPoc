#include "ClockGuard.h"

DEFINE_LOG_CATEGORY_STATIC(LogClockGuard, Log, All);

const TCHAR* FClockGuard::RegistrySubKey = TEXT("Software\\CompanyLicensing");
const TCHAR* FClockGuard::LastSeenValueName = TEXT("LastSeenUtc");
const TCHAR* FClockGuard::BootTimeValueName = TEXT("BootTimeEstimate");
const TCHAR* FClockGuard::AnchorSigValueName = TEXT("AnchorMac");

#if PLATFORM_WINDOWS
// Prevent OpenSSL ossl_typ.h from defining 'typedef struct ui_st UI'
#define OPENSSL_NO_UI_CONSOLE
#define OPENSSL_NO_UI
#define UI OPENSSL_UI

#include <openssl/hmac.h>
#include <openssl/evp.h>

#undef UI
#undef OPENSSL_NO_UI
#undef OPENSSL_NO_UI_CONSOLE

#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <winreg.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	static const uint8 ClockHmacKey[32] = {
		0x43, 0x6c, 0x6f, 0x63, 0x6b, 0x47, 0x75, 0x61,
		0x72, 0x64, 0x41, 0x6e, 0x74, 0x69, 0x54, 0x61,
		0x6d, 0x70, 0x65, 0x72, 0x4b, 0x65, 0x79, 0x32,
		0x30, 0x32, 0x36, 0x21, 0x23, 0x24, 0x25, 0x26
	};

	FString ComputeAnchorMac(const FString& LastSeen, const FString& BootTime)
	{
		FString Data = FString::Printf(TEXT("%s|%s"), *LastSeen, *BootTime);
		FTCHARToUTF8 Utf8Data(*Data);

		uint8 Digest[EVP_MAX_MD_SIZE] = {0};
		uint32 DigestLen = 0;

		HMAC(EVP_sha256(),
			 ClockHmacKey, sizeof(ClockHmacKey),
			 reinterpret_cast<const uint8*>(Utf8Data.Get()), Utf8Data.Length(),
			 Digest, &DigestLen);

		FString Hex;
		Hex.Reserve(DigestLen * 2);
		for (uint32 i = 0; i < DigestLen; ++i)
		{
			Hex += FString::Printf(TEXT("%02x"), Digest[i]);
		}
		return Hex;
	}

	bool ReadRegistryString(HKEY RootKey, const TCHAR* SubKey, const TCHAR* ValueName, FString& OutValue)
	{
		HKEY hKey = nullptr;
		if (RegOpenKeyExW(RootKey, SubKey, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS || hKey == nullptr)
		{
			return false;
		}

		bool bSuccess = false;
		DWORD Type = 0;
		DWORD DataSize = 0;

		if (RegQueryValueExW(hKey, ValueName, nullptr, &Type, nullptr, &DataSize) == ERROR_SUCCESS && Type == REG_SZ && DataSize > 0)
		{
			TArray<TCHAR> Buffer;
			Buffer.SetNum(DataSize / sizeof(TCHAR) + 1);

			if (RegQueryValueExW(hKey, ValueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(Buffer.GetData()), &DataSize) == ERROR_SUCCESS)
			{
				Buffer[DataSize / sizeof(TCHAR)] = TEXT('\0');
				OutValue = FString(Buffer.GetData());
				bSuccess = true;
			}
		}

		RegCloseKey(hKey);
		return bSuccess;
	}

	bool WriteRegistryString(HKEY RootKey, const TCHAR* SubKey, const TCHAR* ValueName, const FString& Value)
	{
		HKEY hKey = nullptr;
		if (RegCreateKeyExW(RootKey, SubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &hKey, nullptr) != ERROR_SUCCESS || hKey == nullptr)
		{
			return false;
		}

		const DWORD DataSize = (Value.Len() + 1) * sizeof(TCHAR);
		const LSTATUS Status = RegSetValueExW(hKey, ValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(*Value), DataSize);
		RegCloseKey(hKey);
		return Status == ERROR_SUCCESS;
	}

	bool CheckAnchorLocation(HKEY RootKey, const TCHAR* RootName, const FDateTime& NowUtc, int64 ToleranceSeconds, bool& bOutFound, FDateTime& OutLastSeen)
	{
		bOutFound = false;
		FString LastSeenStr;
		FString BootTimeStr;
		FString StoredMac;

		if (!ReadRegistryString(RootKey, FClockGuard::RegistrySubKey, FClockGuard::LastSeenValueName, LastSeenStr))
		{
			return false;
		}

		bOutFound = true;
		ReadRegistryString(RootKey, FClockGuard::RegistrySubKey, FClockGuard::BootTimeValueName, BootTimeStr);
		ReadRegistryString(RootKey, FClockGuard::RegistrySubKey, FClockGuard::AnchorSigValueName, StoredMac);

		// Validate cryptographic HMAC integrity seal (F7)
		const FString ExpectedMac = ComputeAnchorMac(LastSeenStr, BootTimeStr);
		if (StoredMac.IsEmpty() || StoredMac != ExpectedMac)
		{
			UE_LOG(LogClockGuard, Warning, TEXT("[%s] Anchor integrity MAC mismatch! Tampering or unauthorized fabrication detected."), RootName);
			return true; // Tampered anchor -> fail closed (rollback suspected)
		}

		if (FDateTime::ParseIso8601(*LastSeenStr, OutLastSeen))
		{
			if (NowUtc < (OutLastSeen - FTimespan::FromSeconds(ToleranceSeconds)))
			{
				FTimespan Delta = OutLastSeen - NowUtc;
				UE_LOG(LogClockGuard, Warning, TEXT("[%s] Clock rollback detected: current=%s, lastSeen=%s, delta=%.0f seconds"),
					RootName, *NowUtc.ToIso8601(), *OutLastSeen.ToIso8601(), Delta.GetTotalSeconds());
				return true;
			}
		}

		// Cross-check with uptime estimate
		ULONGLONG UptimeMs = GetTickCount64();
		FDateTime BootTimeEstimate = NowUtc - FTimespan::FromMilliseconds(UptimeMs);
		FDateTime StoredBootEstimate;
		if (!BootTimeStr.IsEmpty() && FDateTime::ParseIso8601(*BootTimeStr, StoredBootEstimate))
		{
			if (BootTimeEstimate < (StoredBootEstimate - FTimespan::FromSeconds(ToleranceSeconds)))
			{
				UE_LOG(LogClockGuard, Warning, TEXT("[%s] GetTickCount64 cross-check: rollback suspected"), RootName);
				return true;
			}
		}

		return false;
	}
}
#endif

bool FClockGuard::DetectRollback(const FDateTime& NowUtc)
{
#if PLATFORM_WINDOWS
	bool bHklmFound = false;
	bool bHkcuFound = false;
	FDateTime HklmLastSeen;
	FDateTime HkcuLastSeen;

	// Check primary anchor in HKLM
	const bool bHklmRollback = CheckAnchorLocation(HKEY_LOCAL_MACHINE, TEXT("HKLM"), NowUtc, ToleranceSeconds, bHklmFound, HklmLastSeen);
	if (bHklmRollback)
	{
		return true;
	}

	// Check secondary anchor in HKCU
	const bool bHkcuRollback = CheckAnchorLocation(HKEY_CURRENT_USER, TEXT("HKCU"), NowUtc, ToleranceSeconds, bHkcuFound, HkcuLastSeen);
	if (bHkcuRollback)
	{
		return true;
	}

	// Cross-check: If both exist, HKCU must not claim a time significantly ahead or behind HKLM
	if (bHklmFound && bHkcuFound)
	{
		FTimespan Drift = HklmLastSeen - HkcuLastSeen;
		if (FMath::Abs(Drift.GetTotalSeconds()) > ToleranceSeconds)
		{
			UE_LOG(LogClockGuard, Warning, TEXT("HKLM and HKCU clock anchors diverge by %.0f seconds. Inconsistency detected."), Drift.GetTotalSeconds());
			return true;
		}
	}

	if (!bHklmFound && !bHkcuFound)
	{
		UE_LOG(LogClockGuard, Log, TEXT("No previous timestamp found in HKLM or HKCU"));
	}

	return false;
#else
	UE_LOG(LogClockGuard, Warning, TEXT("ClockGuard is only implemented for Windows."));
	return false;
#endif
}

void FClockGuard::RecordTimestamp(const FDateTime& NowUtc)
{
#if PLATFORM_WINDOWS
	const FString NowUtcStr = NowUtc.ToIso8601();
	const ULONGLONG UptimeMs = GetTickCount64();
	const FDateTime BootTimeEstimate = NowUtc - FTimespan::FromMilliseconds(UptimeMs);
	const FString BootTimeStr = BootTimeEstimate.ToIso8601();
	const FString Mac = ComputeAnchorMac(NowUtcStr, BootTimeStr);

	// 1. Attempt write to HKLM (elevated/admin primary location)
	const bool bWroteHklm = WriteRegistryString(HKEY_LOCAL_MACHINE, RegistrySubKey, LastSeenValueName, NowUtcStr) &&
							WriteRegistryString(HKEY_LOCAL_MACHINE, RegistrySubKey, BootTimeValueName, BootTimeStr) &&
							WriteRegistryString(HKEY_LOCAL_MACHINE, RegistrySubKey, AnchorSigValueName, Mac);

	if (bWroteHklm)
	{
		UE_LOG(LogClockGuard, Log, TEXT("Timestamp anchor recorded to HKLM with HMAC seal: %s"), *NowUtcStr);
	}

	// 2. Always write to HKCU (standard user accessible secondary location)
	WriteRegistryString(HKEY_CURRENT_USER, RegistrySubKey, LastSeenValueName, NowUtcStr);
	WriteRegistryString(HKEY_CURRENT_USER, RegistrySubKey, BootTimeValueName, BootTimeStr);
	WriteRegistryString(HKEY_CURRENT_USER, RegistrySubKey, AnchorSigValueName, Mac);
	UE_LOG(LogClockGuard, Log, TEXT("Timestamp anchor recorded to HKCU with HMAC seal: %s"), *NowUtcStr);
#endif
}
