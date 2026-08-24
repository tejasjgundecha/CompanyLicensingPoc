#include "MachineFingerprint.h"

DEFINE_LOG_CATEGORY_STATIC(LogMachineFingerprint, Log, All);

#if PLATFORM_WINDOWS
// Prevent OpenSSL ossl_typ.h from defining 'typedef struct ui_st UI'
// which collides with Unreal Engine's UI namespace. Define a temporary
// macro that remaps the identifier while including OpenSSL headers only.
#define OPENSSL_NO_UI_CONSOLE
#define OPENSSL_NO_UI
#define UI OPENSSL_UI
#endif

#include <openssl/evp.h>

#if PLATFORM_WINDOWS
// Undo temporary macro to avoid affecting engine headers
#undef UI
#undef OPENSSL_NO_UI
#undef OPENSSL_NO_UI_CONSOLE
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <winreg.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	FString RunWmicQuery(const FString& WmicArgs)
	{
		FString Output;
		void* ReadPipe = nullptr;
		void* WritePipe = nullptr;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

		if (ReadPipe == nullptr || WritePipe == nullptr)
		{
			FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
			return TEXT("UNKNOWN");
		}

		FString Cmd = TEXT("wmic");
		FProcHandle ProcHandle = FPlatformProcess::CreateProc(
			*Cmd, *WmicArgs, false, true, true, nullptr, 0, nullptr, WritePipe, ReadPipe);

		if (!ProcHandle.IsValid())
		{
			FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
			return TEXT("UNKNOWN");
		}

		while (FPlatformProcess::IsProcRunning(ProcHandle))
		{
			FString PipeOut = FPlatformProcess::ReadPipe(ReadPipe);
			if (!PipeOut.IsEmpty())
			{
				Output += PipeOut;
			}
			FPlatformProcess::Sleep(0.01f);
		}

		FString PipeOut = FPlatformProcess::ReadPipe(ReadPipe);
		if (!PipeOut.IsEmpty())
		{
			Output += PipeOut;
		}
		FPlatformProcess::CloseProc(ProcHandle);

		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

		// Parse output: looking for KEY=VALUE
		TArray<FString> Lines;
		Output.ParseIntoArrayLines(Lines, true);
		for (const FString& Line : Lines)
		{
			int32 EqualsIdx = 0;
			if (Line.FindChar('=', EqualsIdx))
			{
				return Line.Mid(EqualsIdx + 1).TrimStartAndEnd();
			}
		}

		return TEXT("UNKNOWN");
	}

	FString Sha256Hex(const FString& Input)
	{
		FTCHARToUTF8 Utf8String(*Input);

		EVP_MD_CTX* Context = EVP_MD_CTX_new();
		if (!Context)
		{
			return FString();
		}

		const EVP_MD* Sha256Md = EVP_sha256();
		if (!Sha256Md)
		{
			EVP_MD_CTX_free(Context);
			return FString();
		}

		if (EVP_DigestInit_ex(Context, Sha256Md, nullptr) != 1)
		{
			EVP_MD_CTX_free(Context);
			return FString();
		}

		if (EVP_DigestUpdate(Context, Utf8String.Get(), Utf8String.Length()) != 1)
		{
			EVP_MD_CTX_free(Context);
			return FString();
		}

		uint8 Hash[EVP_MAX_MD_SIZE] = {0};  // Initialize to zero
		uint32 HashLen = 0;
		if (EVP_DigestFinal_ex(Context, Hash, &HashLen) != 1 || HashLen == 0 || HashLen > EVP_MAX_MD_SIZE)
		{
			EVP_MD_CTX_free(Context);
			return FString();
		}

		EVP_MD_CTX_free(Context);

		FString HexStr;
		HexStr.Reserve(HashLen * 2);
		for (uint32 i = 0; i < HashLen; ++i)
		{
			HexStr += FString::Printf(TEXT("%02x"), Hash[i]);
		}
		return HexStr;
	}
}

#endif // PLATFORM_WINDOWS

FString FMachineFingerprint::ComputeFingerprint()
{
#if PLATFORM_WINDOWS
	FString BiosUuid = RunWmicQuery(TEXT("path Win32_ComputerSystemProduct get UUID /value")).ToUpper();
	FString MotherboardSerial = RunWmicQuery(TEXT("path Win32_BaseBoard get SerialNumber /value")).ToUpper();
	FString MachineGuid = TEXT("UNKNOWN");

	HKEY hKey = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS && hKey != nullptr)
	{
		WCHAR Value[256] = {0};  // Initialize to zero
		DWORD Size = sizeof(Value) - sizeof(WCHAR);  // Leave room for null terminator
		if (RegQueryValueExW(hKey, L"MachineGuid", nullptr, nullptr, (LPBYTE)Value, &Size) == ERROR_SUCCESS)
		{
			// Ensure null termination even if registry value is oversized
			Value[255] = L'\0';
			MachineGuid = FString(Value).TrimStartAndEnd();
		}
		RegCloseKey(hKey);
	}

	FString Combined = FString::Printf(TEXT("%s|%s|%s"), *BiosUuid, *MotherboardSerial, *MachineGuid);
	FString Fingerprint = Sha256Hex(Combined);

	if (Fingerprint.Len() >= 16)
	{
		UE_LOG(LogMachineFingerprint, Log, TEXT("Machine fingerprint: %s...%s"), 
			*Fingerprint.Left(8), *Fingerprint.Right(8));
	}

	return Fingerprint;
#else
	UE_LOG(LogMachineFingerprint, Warning, TEXT("Machine fingerprint is only supported on Windows"));
	return FString();
#endif
}
