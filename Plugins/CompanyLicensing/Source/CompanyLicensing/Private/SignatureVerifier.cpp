#include "SignatureVerifier.h"
#include "LicensePublicKey.h"

#if PLATFORM_WINDOWS
// Prevent OpenSSL ossl_typ.h from defining 'typedef struct ui_st UI'
// which collides with Unreal Engine's UI namespace.
#define OPENSSL_NO_UI_CONSOLE
#define OPENSSL_NO_UI
#endif

#include <openssl/evp.h>

bool FSignatureVerifier::VerifyEd25519(const TArray<uint8>& CanonicalUtf8, const TArray<uint8>& Signature)
{
	if (Signature.Num() != 64 || CanonicalUtf8.Num() <= 0)
	{
		return false;
	}

	EVP_PKEY* PublicKey = EVP_PKEY_new_raw_public_key(
		EVP_PKEY_ED25519,
		nullptr,
		CompanyLicensing::EmbeddedPublicKey,
		32);
	if (!PublicKey)
	{
		return false;
	}

	EVP_MD_CTX* Context = EVP_MD_CTX_new();
	if (!Context)
	{
		EVP_PKEY_free(PublicKey);
		return false;
	}

	int32 InitOk = EVP_DigestVerifyInit(Context, nullptr, nullptr, nullptr, PublicKey);
	int32 VerifyOk = 0;
	if (InitOk == 1)
	{
		VerifyOk = EVP_DigestVerify(
			Context,
			Signature.GetData(),
			static_cast<size_t>(Signature.Num()),
			CanonicalUtf8.GetData(),
			static_cast<size_t>(CanonicalUtf8.Num()));
	}

	EVP_MD_CTX_free(Context);
	EVP_PKEY_free(PublicKey);
	return VerifyOk == 1;
}
