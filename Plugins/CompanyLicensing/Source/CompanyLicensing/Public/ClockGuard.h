#pragma once

#include "CoreMinimal.h"

/**
 * Detects system clock rollback using Windows Registry persistence and GetTickCount64.
 * Windows-only; returns false (no rollback) on other platforms.
 */
class COMPANYLICENSING_API FClockGuard
{
public:
	/**
	 * Check if the system clock appears to have been rolled back.
	 * Compares NowUtc against a previously persisted timestamp in the Windows Registry.
	 * Uses a 5-minute tolerance to accommodate NTP drift corrections.
	 * @return true if a rollback is detected.
	 */
	static bool DetectRollback(const FDateTime& NowUtc);

	/**
	 * Record the current timestamp in the Windows Registry after successful validation.
	 * Call this only when the license has been validated successfully.
	 */
	static void RecordTimestamp(const FDateTime& NowUtc);

private:
	/** Tolerance for clock drift (NTP corrections). Rollbacks smaller than this are ignored. */
	static constexpr int64 ToleranceSeconds = 300; // 5 minutes

	/** Windows Registry path for persisted timestamps. */
	static const TCHAR* RegistrySubKey;
	static const TCHAR* LastSeenValueName;
	static const TCHAR* BootTimeValueName;
};
