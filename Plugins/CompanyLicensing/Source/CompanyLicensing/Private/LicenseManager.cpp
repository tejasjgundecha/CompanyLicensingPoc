#include "LicenseManager.h"
#include "LicenseValidator.h"
#include "ClockGuard.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/MessageDialog.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CoreMisc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogCompanyLicensing, Log, All);

namespace
{
	void ShowNativeDialog(const FText& Message, bool bIsSuccess)
	{
#if PLATFORM_WINDOWS
		const TCHAR* Title = bIsSuccess ? TEXT("Company Licensing") : TEXT("License Error");
		const UINT Flags = (bIsSuccess ? MB_ICONINFORMATION : MB_ICONERROR) | MB_OK | MB_TOPMOST | MB_SETFOREGROUND;
		::MessageBoxW(NULL, *Message.ToString(), Title, Flags);
#else
		FMessageDialog::Open(EAppMsgType::Ok, Message);
#endif
	}

	void DebugLogToFile(const FString& Message)
	{
		static FString DebugLogPath;
		if (DebugLogPath.IsEmpty())
		{
			DebugLogPath = FPaths::Combine(
				FPaths::GetPath(FPlatformProcess::ExecutablePath()),
				TEXT("license_debug.log"));
			// Clear previous log on first call
			FFileHelper::SaveStringToFile(TEXT(""), *DebugLogPath);
		}
		FString Line = FString::Printf(TEXT("[%s] %s\n"), *FDateTime::UtcNow().ToIso8601(), *Message);
		FFileHelper::SaveStringToFile(Line, *DebugLogPath,
			FFileHelper::EEncodingOptions::AutoDetect,
			&IFileManager::Get(),
			FILEWRITE_Append);
	}
}

FLicenseManager& FLicenseManager::Get()
{
	static FLicenseManager Singleton;
	return Singleton;
}

FDateTime FLicenseManager::GetClockNowUtc()
{
#if !UE_BUILD_SHIPPING
	FString FakeNow;
	if (FParse::Value(FCommandLine::Get(), TEXT("-LicenseFakeNow="), FakeNow))
	{
		FDateTime Parsed;
		if (FDateTime::ParseIso8601(*FakeNow, Parsed))
		{
			UE_LOG(LogCompanyLicensing, Warning, TEXT("Using development clock override from command line: %s"), *FakeNow);
			return Parsed;
		}
	}

	FakeNow = FPlatformMisc::GetEnvironmentVariable(TEXT("COMPANY_LICENSING_FAKE_NOW"));
	if (!FakeNow.IsEmpty())
	{
		FDateTime Parsed;
		if (FDateTime::ParseIso8601(*FakeNow, Parsed))
		{
			UE_LOG(LogCompanyLicensing, Warning, TEXT("Using development clock override from environment: %s"), *FakeNow);
			return Parsed;
		}
	}
#endif
	return FDateTime::UtcNow();
}

void FLicenseManager::InitializeAndEnforce()
{
	DebugLogToFile(TEXT("========== License Manager InitializeAndEnforce() ENTERED =========="));

	if (bInitialized)
	{
		DebugLogToFile(TEXT("Already initialized, returning early"));
		return;
	}
	bInitialized = true;

	UE_LOG(LogCompanyLicensing, Warning, TEXT("========== License Manager Initialization Starting =========="));

	if (IsRunningCommandlet())
	{
		UE_LOG(LogCompanyLicensing, Log, TEXT("Skipping license enforcement while a commandlet is running (cook/packaging)."));
		UE_LOG(LogCompanyLicensing, Warning, TEXT("========== License Manager Initialization Skipped (Commandlet) =========="));
		return;
	}

	FLicensePayload Loaded;
	if (!TryLoadLicenseFile(Loaded, LicenseFilePath))
	{
		State = ELicenseState::LicenseMissing;
		UE_LOG(LogCompanyLicensing, Error, TEXT("License file not found. Looked next to the executable and in the project directory."));
		UE_LOG(LogCompanyLicensing, Warning, TEXT("========== License Manager Initialization Failed: License Missing =========="));
		DebugLogToFile(TEXT("License file not found."));
		FailAndExit(State);
		return;
	}

	Payload = Loaded;
	const FDateTime NowUtc = GetClockNowUtc();

	UE_LOG(LogCompanyLicensing, Log, TEXT("License successfully loaded from: %s"), *LicenseFilePath);
	UE_LOG(LogCompanyLicensing, Log, TEXT("  License ID: %s"), *Payload.LicenseId);
	UE_LOG(LogCompanyLicensing, Log, TEXT("  Project ID: %s"), *Payload.ProjectId);
	UE_LOG(LogCompanyLicensing, Log, TEXT("  Current UTC Time: %s"), *NowUtc.ToIso8601());

	// Clock rollback check (Windows Shipping builds only — non-Shipping supports fake clocks)
#if PLATFORM_WINDOWS && UE_BUILD_SHIPPING
	UE_LOG(LogCompanyLicensing, Log, TEXT("Performing clock rollback detection (Windows Shipping)..."));
	if (FClockGuard::DetectRollback(NowUtc))
	{
		State = ELicenseState::ClockRollback;
		UE_LOG(LogCompanyLicensing, Error, TEXT("License state=%s — system clock rollback detected"),
			LicenseStateToString(State));
		UE_LOG(LogCompanyLicensing, Warning, TEXT("========== License Manager Initialization Failed: Clock Rollback =========="));
		DebugLogToFile(TEXT("Clock rollback detected."));
		FailAndExit(State);
		return;
	}
	UE_LOG(LogCompanyLicensing, Log, TEXT("Clock rollback check passed"));
#else
	UE_LOG(LogCompanyLicensing, Log, TEXT("Skipping clock rollback detection (not Windows Shipping)"));
#endif

	UE_LOG(LogCompanyLicensing, Log, TEXT("Starting license validation..."));
	State = FLicenseValidator::Validate(Payload, NowUtc);
	UE_LOG(LogCompanyLicensing, Log, TEXT("License validation completed"));
	UE_LOG(LogCompanyLicensing, Log, TEXT("License state=%s file=%s licenseId=%s"),
		LicenseStateToString(State),
		*LicenseFilePath,
		*Payload.LicenseId);

	DebugLogToFile(FString::Printf(TEXT("Validation completed. State=%s"), LicenseStateToString(State)));

	if (!LicenseStateAllowsRun(State))
	{
		UE_LOG(LogCompanyLicensing, Error, TEXT("License validation failed with state: %s"), LicenseStateToString(State));
		UE_LOG(LogCompanyLicensing, Warning, TEXT("========== License Manager Initialization Failed: Invalid License =========="));
		DebugLogToFile(TEXT("License validation failed. Exiting."));
		FailAndExit(State);
		return;
	}

	UE_LOG(LogCompanyLicensing, Log, TEXT("License validation successful!"));

	// Record timestamp after successful validation (Windows only)
#if PLATFORM_WINDOWS
	UE_LOG(LogCompanyLicensing, Log, TEXT("Recording timestamp for rollback detection..."));
	FClockGuard::RecordTimestamp(NowUtc);
	UE_LOG(LogCompanyLicensing, Log, TEXT("Timestamp recorded"));
#endif

	UE_LOG(LogCompanyLicensing, Warning, TEXT("========== License Manager Initialization Successful =========="));
	DebugLogToFile(TEXT("========== License Manager Initialization Successful =========="));

	ShowSuccessDialog(State);
}

bool FLicenseManager::TryLoadLicenseFile(FLicensePayload& OutPayload, FString& OutPath) const
{
	TArray<FString> Candidates;

	const FString ExeDir = FPaths::GetPath(FPlatformProcess::ExecutablePath());
	// 1. Check directory containing the executable
	Candidates.Add(FPaths::Combine(ExeDir, TEXT("license.json")));

	// 2. Check launch / working directory
	Candidates.Add(FPaths::Combine(FPaths::LaunchDir(), TEXT("license.json")));

	// 3. Check packaged root directory (where main launcher executable resides)
	Candidates.Add(FPaths::Combine(FPaths::RootDir(), TEXT("license.json")));

#if WITH_EDITOR
	// 4. In Editor only: check project root directory
	Candidates.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("license.json")));
#endif

	UE_LOG(LogCompanyLicensing, Log, TEXT("Searching for license file in %d locations"), Candidates.Num());

	for (FString Candidate : Candidates)
	{
		FPaths::NormalizeFilename(Candidate);
		UE_LOG(LogCompanyLicensing, Log, TEXT("  Checking: %s"), *Candidate);

		if (FPaths::FileExists(Candidate))
		{
			UE_LOG(LogCompanyLicensing, Log, TEXT("    Found! Loading... %s"), *Candidate);
			FString JsonText;
			if (!FFileHelper::LoadFileToString(JsonText, *Candidate))
			{
				UE_LOG(LogCompanyLicensing, Warning, TEXT("    Failed to read file"));
				continue;
			}

			if (ParseLicenseJson(JsonText, OutPayload))
			{
				OutPayload.bLoadedFromDisk = true;
				OutPath = Candidate;
				UE_LOG(LogCompanyLicensing, Log, TEXT("    Successfully parsed license"));
				return true;
			}
			else
			{
				UE_LOG(LogCompanyLicensing, Warning, TEXT("    Failed to parse JSON"));
			}
		}
	}

	UE_LOG(LogCompanyLicensing, Warning, TEXT("License file not found in any location"));
	return false;
}

bool FLicenseManager::ParseLicenseJson(const FString& JsonText, FLicensePayload& OutPayload)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	Root->TryGetStringField(TEXT("licenseId"), OutPayload.LicenseId);
	Root->TryGetStringField(TEXT("projectId"), OutPayload.ProjectId);
	Root->TryGetStringField(TEXT("issuedAt"), OutPayload.IssuedAt);
	Root->TryGetStringField(TEXT("subscriptionValidUntil"), OutPayload.SubscriptionValidUntil);
	Root->TryGetStringField(TEXT("offlineGraceUntil"), OutPayload.OfflineGraceUntil);
	Root->TryGetStringField(TEXT("lastValidation"), OutPayload.LastValidation);
	Root->TryGetStringField(TEXT("machineFingerprint"), OutPayload.MachineFingerprint);
	Root->TryGetStringField(TEXT("signature"), OutPayload.SignatureBase64);
	return true;
}

void FLicenseManager::ShowSuccessDialog(ELicenseState SuccessState) const
{
	const FText Message = LicenseStateUserMessage(SuccessState, Payload);
	UE_LOG(LogCompanyLicensing, Log, TEXT("License Valid Dialog: %s"), *Message.ToString());
	ShowNativeDialog(Message, true);
}

void FLicenseManager::FailAndExit(ELicenseState FailedState) const
{
	const FText Message = LicenseStateUserMessage(FailedState, Payload);
	UE_LOG(LogCompanyLicensing, Error, TEXT("%s (%s)"), *Message.ToString(), LicenseStateToString(FailedState));

	ShowNativeDialog(Message, false);

#if WITH_EDITOR
	if (GIsEditor)
	{
		UE_LOG(LogCompanyLicensing, Error, TEXT("License enforcement is non-fatal in the editor so the project can still be opened. Packaged builds will exit."));
		return;
	}
#endif

	// Ensure immediate termination
	UE_LOG(LogCompanyLicensing, Error, TEXT("Exiting application due to license validation failure"));
	FPlatformMisc::RequestExit(true);

#if PLATFORM_WINDOWS
	::ExitProcess(1);
#endif
}
