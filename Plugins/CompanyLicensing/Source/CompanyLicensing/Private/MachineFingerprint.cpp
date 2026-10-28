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
#pragma pack(push, 1)
	struct FRawSmbiosData
	{
		uint8  Used20CallingMethod;
		uint8  SMBIOSMajorVersion;
		uint8  SMBIOSMinorVersion;
		uint8  DmiRevision;
		uint32 Length;
		uint8  SMBIOSTableData[1];
	};

	struct FSmbiosHeader
	{
		uint8  Type;
		uint8  Length;
		uint16 Handle;
	};
#pragma pack(pop)

	bool GetSmbiosInfo(FString& OutUuid, FString& OutBoardSerial)
	{
		OutUuid = TEXT("UNKNOWN");
		OutBoardSerial = TEXT("UNKNOWN");

		const DWORD Signature = 0x52534D42; // 'RSMB'
		const DWORD BufferSize = GetSystemFirmwareTable(Signature, 0, nullptr, 0);
		if (BufferSize == 0)
		{
			return false;
		}

		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(BufferSize);
		const DWORD BytesRead = GetSystemFirmwareTable(Signature, 0, Buffer.GetData(), BufferSize);
		if (BytesRead == 0 || BytesRead > BufferSize)
		{
			return false;
		}

		const FRawSmbiosData* SmbiosData = reinterpret_cast<const FRawSmbiosData*>(Buffer.GetData());
		if (SmbiosData->Length > BufferSize)
		{
			return false;
		}

		const uint8* Current = SmbiosData->SMBIOSTableData;
		const uint8* TableEnd = SmbiosData->SMBIOSTableData + SmbiosData->Length;

		bool bFoundUuid = false;
		bool bFoundSerial = false;

		while (Current + sizeof(FSmbiosHeader) <= TableEnd)
		{
			const FSmbiosHeader* Header = reinterpret_cast<const FSmbiosHeader*>(Current);
			if (Header->Length < sizeof(FSmbiosHeader))
			{
				break;
			}

			const uint8* FormattedEnd = Current + Header->Length;
			if (FormattedEnd > TableEnd)
			{
				break;
			}

			// Type 1: System Information (UUID at offset 0x08, length 16)
			if (Header->Type == 1 && Header->Length >= 0x19 && !bFoundUuid)
			{
				const uint8* Uuid = Current + 0x08;
				bool bAllZero = true;
				bool bAllFF = true;
				for (int32 i = 0; i < 16; ++i)
				{
					if (Uuid[i] != 0x00) bAllZero = false;
					if (Uuid[i] != 0xFF) bAllFF = false;
				}

				if (!bAllZero && !bAllFF)
				{
					const uint32 Data1 = *reinterpret_cast<const uint32*>(Uuid);
					const uint16 Data2 = *reinterpret_cast<const uint16*>(Uuid + 4);
					const uint16 Data3 = *reinterpret_cast<const uint16*>(Uuid + 6);
					OutUuid = FString::Printf(TEXT("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"),
						Data1, Data2, Data3,
						Uuid[8], Uuid[9], Uuid[10], Uuid[11],
						Uuid[12], Uuid[13], Uuid[14], Uuid[15]);
					bFoundUuid = true;
				}
			}

			// Type 2: BaseBoard Information (SerialNumber string index at offset 0x07)
			if (Header->Type == 2 && Header->Length >= 0x08 && !bFoundSerial)
			{
				const uint8 SerialIndex = *(Current + 0x07);
				if (SerialIndex > 0)
				{
					const char* Str = reinterpret_cast<const char*>(FormattedEnd);
					uint8 CurrentIndex = 1;
					while (reinterpret_cast<const uint8*>(Str) < TableEnd && *Str != '\0')
					{
						if (CurrentIndex == SerialIndex)
						{
							OutBoardSerial = FString(ANSI_TO_TCHAR(Str)).TrimStartAndEnd().ToUpper();
							bFoundSerial = true;
							break;
						}
						Str += strlen(Str) + 1;
						CurrentIndex++;
					}
				}
			}

			// Strings area ends with a double null character
			const uint8* Scan = FormattedEnd;
			while (Scan + 1 < TableEnd)
			{
				if (Scan[0] == 0 && Scan[1] == 0)
				{
					Scan += 2;
					break;
				}
				Scan++;
			}
			Current = Scan;

			if (Header->Type == 127) // End of table
			{
				break;
			}
		}

		return bFoundUuid || bFoundSerial;
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
	FString BiosUuid;
	FString MotherboardSerial;
	GetSmbiosInfo(BiosUuid, MotherboardSerial);

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
