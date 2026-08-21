#include "SignatureVerifier.h"
#include "LicensePublicKey.h"

#if PLATFORM_WINDOWS
// Windows.h defines UI which collides with OpenSSL.
#pragma push_macro("UI")
#undef UI
#endif

#include <openssl/evp.h>

#if PLATFORM_WINDOWS
#pragma pop_macro("UI")
#endif

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
