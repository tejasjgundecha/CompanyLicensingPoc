#include "ClockGuard.h"

DEFINE_LOG_CATEGORY_STATIC(LogClockGuard, Log, All);

const TCHAR* FClockGuard::RegistrySubKey = TEXT("Software\\CompanyLicensing");
const TCHAR* FClockGuard::LastSeenValueName = TEXT("LastSeenUtc");
const TCHAR* FClockGuard::BootTimeValueName = TEXT("BootTimeEstimate");

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winreg.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	bool ReadRegistryString(const TCHAR* SubKey, const TCHAR* ValueName, FString& OutValue)
	{
		HKEY hKey = nullptr;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, SubKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS || hKey == nullptr)
		{
			return false;
		}

		bool bSuccess = false;
		DWORD Type = 0;
		DWORD DataSize = 0;

		// First query to get the size
		if (RegQueryValueExW(hKey, ValueName, nullptr, &Type, nullptr, &DataSize) == ERROR_SUCCESS && Type == REG_SZ && DataSize > 0)
		{
			TArray<TCHAR> Buffer;
			Buffer.SetNum(DataSize / sizeof(TCHAR) + 1); // +1 for safety

			// Second query to get the actual data
			if (RegQueryValueExW(hKey, ValueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(Buffer.GetData()), &DataSize) == ERROR_SUCCESS)
			{
				// Ensure null termination
				Buffer[DataSize / sizeof(TCHAR)] = TEXT('\0');
				OutValue = FString(Buffer.GetData());
				bSuccess = true;
			}
		}

		RegCloseKey(hKey);
		return bSuccess;
	}

	bool WriteRegistryString(const TCHAR* SubKey, const TCHAR* ValueName, const FString& Value)
	{
		HKEY hKey = nullptr;
		if (RegCreateKeyExW(HKEY_CURRENT_USER, SubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS || hKey == nullptr)
		{
			return false;
		}

		const DWORD DataSize = (Value.Len() + 1) * sizeof(TCHAR);
		const LSTATUS Status = RegSetValueExW(hKey, ValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(*Value), DataSize);
		RegCloseKey(hKey);
		return Status == ERROR_SUCCESS;
	}
}
#endif

bool FClockGuard::DetectRollback(const FDateTime& NowUtc)
{
#if PLATFORM_WINDOWS
	FString LastSeenStr;
	bool bRollbackDetected = false;

	// Method 1: Registry timestamp comparison
	if (ReadRegistryString(RegistrySubKey, LastSeenValueName, LastSeenStr))
	{
		FDateTime LastSeenUtc;
		if (FDateTime::ParseIso8601(*LastSeenStr, LastSeenUtc))
		{
			FTimespan Delta = LastSeenUtc - NowUtc;
			if (NowUtc < (LastSeenUtc - FTimespan::FromSeconds(ToleranceSeconds)))
			{
				UE_LOG(LogClockGuard, Warning, TEXT("Clock rollback detected: current=%s, lastSeen=%s, delta=%.0f seconds"), *NowUtc.ToIso8601(), *LastSeenUtc.ToIso8601(), Delta.GetTotalSeconds());
				bRollbackDetected = true;
			}
		}
	}
	else
	{
		UE_LOG(LogClockGuard, Log, TEXT("No previous timestamp found, first run"));
	}

	// Method 2: GetTickCount64 cross-check
	ULONGLONG UptimeMs = GetTickCount64();
	FDateTime BootTimeEstimate = NowUtc - FTimespan::FromMilliseconds(UptimeMs);

	FString StoredBootEstimateStr;
	if (ReadRegistryString(RegistrySubKey, BootTimeValueName, StoredBootEstimateStr))
	{
		FDateTime StoredBootEstimate;
		if (FDateTime::ParseIso8601(*StoredBootEstimateStr, StoredBootEstimate))
		{
			if (BootTimeEstimate < (StoredBootEstimate - FTimespan::FromSeconds(ToleranceSeconds)))
			{
				UE_LOG(LogClockGuard, Warning, TEXT("GetTickCount64 cross-check: rollback suspected"));
				bRollbackDetected = true;
			}
		}
	}

	return bRollbackDetected;
#else
	UE_LOG(LogClockGuard, Warning, TEXT("ClockGuard is only implemented for Windows."));
	return false;
#endif
}

void FClockGuard::RecordTimestamp(const FDateTime& NowUtc)
{
#if PLATFORM_WINDOWS
	FString NowUtcStr = NowUtc.ToIso8601();
	if (WriteRegistryString(RegistrySubKey, LastSeenValueName, NowUtcStr))
	{
		UE_LOG(LogClockGuard, Log, TEXT("Timestamp recorded: %s"), *NowUtcStr);
	}

	ULONGLONG UptimeMs = GetTickCount64();
	FDateTime BootTimeEstimate = NowUtc - FTimespan::FromMilliseconds(UptimeMs);
	WriteRegistryString(RegistrySubKey, BootTimeValueName, BootTimeEstimate.ToIso8601());
#endif
}
