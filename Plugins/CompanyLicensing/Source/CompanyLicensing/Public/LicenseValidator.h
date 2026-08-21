#pragma once

#include "CoreMinimal.h"
#include "LicenseState.h"
#include "LicenseTypes.h"

class COMPANYLICENSING_API FLicenseValidator
{
public:
	static ELicenseState Validate(const FLicensePayload& Payload, const FDateTime& NowUtc);
};
