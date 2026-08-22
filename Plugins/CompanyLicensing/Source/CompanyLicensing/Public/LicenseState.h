#pragma once

#include "CoreMinimal.h"

enum class ELicenseState : uint8
{
	LicenseValid,
	LicenseExpired,
	LicenseMissing,
	LicenseTampered,
	LicenseInvalid,
	OfflineGraceValid,
	OfflineGraceExpired,
	MachineMismatch,
	ClockRollback
};

inline const TCHAR* LicenseStateToString(ELicenseState State)
{
	switch (State)
	{
	case ELicenseState::LicenseValid: return TEXT("LICENSE_VALID");
	case ELicenseState::LicenseExpired: return TEXT("LICENSE_EXPIRED");
	case ELicenseState::LicenseMissing: return TEXT("LICENSE_MISSING");
	case ELicenseState::LicenseTampered: return TEXT("LICENSE_TAMPERED");
	case ELicenseState::LicenseInvalid: return TEXT("LICENSE_INVALID");
	case ELicenseState::OfflineGraceValid: return TEXT("OFFLINE_GRACE_VALID");
	case ELicenseState::OfflineGraceExpired: return TEXT("OFFLINE_GRACE_EXPIRED");
	case ELicenseState::MachineMismatch: return TEXT("LICENSE_MACHINE_MISMATCH");
	case ELicenseState::ClockRollback: return TEXT("LICENSE_CLOCK_ROLLBACK");
	default: return TEXT("LICENSE_INVALID");
	}
}

inline bool LicenseStateAllowsRun(ELicenseState State)
{
	return State == ELicenseState::LicenseValid || State == ELicenseState::OfflineGraceValid;
}

inline FText LicenseStateUserMessage(ELicenseState State)
{
	if (State == ELicenseState::OfflineGraceExpired)
	{
		return NSLOCTEXT("CompanyLicensing", "GraceExpired", "License validation period has expired.");
	}
	if (State == ELicenseState::MachineMismatch)
	{
		return NSLOCTEXT("CompanyLicensing", "MachineMismatch", "This license is not authorized for this machine.");
	}
	if (State == ELicenseState::ClockRollback)
	{
		return NSLOCTEXT("CompanyLicensing", "ClockRollback", "System clock tampering detected.");
	}
	return NSLOCTEXT("CompanyLicensing", "ValidationFailed", "License validation failed.");
}
