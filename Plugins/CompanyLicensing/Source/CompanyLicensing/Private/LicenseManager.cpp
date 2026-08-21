#include "LicenseManager.h"
#include "LicenseValidator.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogCompanyLicensing, Log, All);

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
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;

	if (IsRunningCommandlet())
	{
		UE_LOG(LogCompanyLicensing, Log, TEXT("Skipping license enforcement while a commandlet is running (cook/packaging)."));
		return;
	}

	FLicensePayload Loaded;
	if (!TryLoadLicenseFile(Loaded, LicenseFilePath))
	{
		State = ELicenseState::LicenseMissing;
		UE_LOG(LogCompanyLicensing, Error, TEXT("License file not found. Looked next to the executable and in the project directory."));
		FailAndExit(State);
		return;
	}

	Payload = Loaded;
	State = FLicenseValidator::Validate(Payload, GetClockNowUtc());
	UE_LOG(LogCompanyLicensing, Log, TEXT("License state=%s file=%s licenseId=%s"),
		LicenseStateToString(State),
		*LicenseFilePath,
		*Payload.LicenseId);

	if (!LicenseStateAllowsRun(State))
	{
		FailAndExit(State);
	}
}

bool FLicenseManager::TryLoadLicenseFile(FLicensePayload& OutPayload, FString& OutPath) const
{
	TArray<FString> Candidates;

	const FString ExeDir = FPaths::GetPath(FPlatformProcess::ExecutablePath());
	Candidates.Add(FPaths::Combine(ExeDir, TEXT("license.json")));
	Candidates.Add(FPaths::Combine(FPaths::LaunchDir(), TEXT("license.json")));
	Candidates.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("license.json")));
	Candidates.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Tools/licenses/valid/license.json")));

	for (FString Candidate : Candidates)
	{
		FPaths::NormalizeFilename(Candidate);
		if (FPaths::FileExists(Candidate))
		{
			FString JsonText;
			if (!FFileHelper::LoadFileToString(JsonText, *Candidate))
			{
				continue;
			}
			if (ParseLicenseJson(JsonText, OutPayload))
			{
				OutPayload.bLoadedFromDisk = true;
				OutPath = Candidate;
				return true;
			}
		}
	}

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
	Root->TryGetStringField(TEXT("signature"), OutPayload.SignatureBase64);
	return true;
}

void FLicenseManager::FailAndExit(ELicenseState FailedState) const
{
	const FText Message = LicenseStateUserMessage(FailedState);
	UE_LOG(LogCompanyLicensing, Error, TEXT("%s (%s)"), *Message.ToString(), LicenseStateToString(FailedState));

#if WITH_EDITOR
	if (GIsEditor)
	{
		UE_LOG(LogCompanyLicensing, Error, TEXT("License enforcement is non-fatal in the editor so the project can still be opened. Packaged builds will exit."));
		return;
	}
#endif

	FMessageDialog::Open(EAppMsgType::Ok, Message);
	FPlatformMisc::RequestExitWithStatus(true, 1);
}
