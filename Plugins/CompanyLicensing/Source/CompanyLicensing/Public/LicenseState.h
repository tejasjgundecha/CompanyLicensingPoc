#pragma once

#include "CoreMinimal.h"
#include "LicenseTypes.h"

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

inline FText LicenseStateUserMessage(ELicenseState State, const FLicensePayload& Payload = FLicensePayload())
{
	switch (State)
	{
	case ELicenseState::LicenseValid:
	{
		if (!Payload.LicenseId.IsEmpty())
		{
			return FText::Format(
				NSLOCTEXT("CompanyLicensing", "LicenseValidDetails", "License validated successfully.\n\nLicense ID: {0}\nSubscription Valid Until: {1}"),
				FText::FromString(Payload.LicenseId),
				FText::FromString(Payload.SubscriptionValidUntil)
			);
		}
		return NSLOCTEXT("CompanyLicensing", "LicenseValid", "License validated successfully.");
	}
	case ELicenseState::OfflineGraceValid:
	{
		if (!Payload.LicenseId.IsEmpty())
		{
			return FText::Format(
				NSLOCTEXT("CompanyLicensing", "OfflineGraceValidDetails", "Running in offline grace mode.\n\nLicense ID: {0}\nOffline Grace Until: {1}\nSubscription Valid Until: {2}"),
				FText::FromString(Payload.LicenseId),
				FText::FromString(Payload.OfflineGraceUntil),
				FText::FromString(Payload.SubscriptionValidUntil)
			);
		}
		return NSLOCTEXT("CompanyLicensing", "OfflineGraceValid", "Running in offline grace mode.");
	}
	case ELicenseState::LicenseExpired:
	{
		if (!Payload.SubscriptionValidUntil.IsEmpty())
		{
			return FText::Format(
				NSLOCTEXT("CompanyLicensing", "LicenseExpiredDetails", "Your subscription license has expired.\n\nExpired on: {0}\nPlease renew your license to continue using the application."),
				FText::FromString(Payload.SubscriptionValidUntil)
			);
		}
		return NSLOCTEXT("CompanyLicensing", "LicenseExpired", "Your subscription license has expired.\n\nPlease renew your license to continue using the application.");
	}
	case ELicenseState::OfflineGraceExpired:
	{
		if (!Payload.OfflineGraceUntil.IsEmpty())
		{
			return FText::Format(
				NSLOCTEXT("CompanyLicensing", "GraceExpiredDetails", "Your offline grace period has expired.\n\nGrace period ended on: {0}\nPlease connect or update your license to continue."),
				FText::FromString(Payload.OfflineGraceUntil)
			);
		}
		return NSLOCTEXT("CompanyLicensing", "GraceExpired", "Your offline grace period has expired.\n\nPlease connect or update your license to continue.");
	}
	case ELicenseState::MachineMismatch:
	{
		return NSLOCTEXT("CompanyLicensing", "MachineMismatch", "This license is not authorized for this computer.\n\nPlease contact support to obtain an authorized license.");
	}
	case ELicenseState::LicenseMissing:
	{
		return NSLOCTEXT("CompanyLicensing", "LicenseMissing", "No license found for this application.\n\nPlease install a valid license to continue.");
	}
	case ELicenseState::ClockRollback:
	{
		return NSLOCTEXT("CompanyLicensing", "ClockRollback", "System clock rollback detected.\n\nPlease ensure your system date and time are set correctly.");
	}
	case ELicenseState::LicenseTampered:
	{
		return NSLOCTEXT("CompanyLicensing", "LicenseTampered", "License verification failed.\n\nThe license data appears to be modified or corrupted. Please reinstall an original valid license.");
	}
	case ELicenseState::LicenseInvalid:
	default:
	{
		return NSLOCTEXT("CompanyLicensing", "LicenseInvalid", "The provided license is invalid for this application.\n\nPlease contact support to obtain a valid license.");
	}
	}
}
