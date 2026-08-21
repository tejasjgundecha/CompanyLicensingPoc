#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CompanyLicensingPocHUD.generated.h"

UCLASS()
class COMPANYLICENSINGPOC_API ACompanyLicensingPocHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
