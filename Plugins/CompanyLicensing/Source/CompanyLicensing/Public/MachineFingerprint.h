#pragma once

#include "CoreMinimal.h"

/**
 * Computes a SHA-256 fingerprint of the current machine's hardware identifiers.
 * Windows-only: BIOS UUID, motherboard serial, and OS install ID.
 * Returns an empty string on non-Windows platforms.
 */
class COMPANYLICENSING_API FMachineFingerprint
{
public:
	/**
	 * Compute the SHA-256 hex fingerprint for the current machine.
	 * On non-Windows platforms, returns an empty string.
	 */
	static FString ComputeFingerprint();
};
