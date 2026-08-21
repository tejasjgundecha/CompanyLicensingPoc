#pragma once

#include "CoreMinimal.h"

class COMPANYLICENSING_API FSignatureVerifier
{
public:
	/** Verify an Ed25519 signature over CanonicalUtf8 using the embedded public key. */
	static bool VerifyEd25519(const TArray<uint8>& CanonicalUtf8, const TArray<uint8>& Signature);
};
