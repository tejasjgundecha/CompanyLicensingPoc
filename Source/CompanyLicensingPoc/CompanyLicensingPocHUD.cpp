#include "CompanyLicensingPocHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "LicenseManager.h"

void ACompanyLicensingPocHUD::DrawHUD()
{
	Super::DrawHUD();

	const ELicenseState State = FLicenseManager::Get().GetState();
	const bool bValid = LicenseStateAllowsRun(State);

	const FString Message = bValid 
		? TEXT("License Valid - Application Running") 
		: FString::Printf(TEXT("LICENSE ERROR: %s - Refusing Execution"), LicenseStateToString(State));

	const FLinearColor Color = bValid ? FLinearColor::Green : FLinearColor::Red;

	if (GEngine && GEngine->GetSmallFont() && Canvas)
	{
		FCanvasTextItem TextItem(FVector2D(64.0f, 64.0f), FText::FromString(Message), GEngine->GetSmallFont(), Color);
		TextItem.Scale = FVector2D(2.0f, 2.0f);
		Canvas->DrawItem(TextItem);
	}
}
