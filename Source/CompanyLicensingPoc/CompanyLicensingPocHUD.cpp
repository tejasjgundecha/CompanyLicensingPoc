#include "CompanyLicensingPocHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void ACompanyLicensingPocHUD::DrawHUD()
{
	Super::DrawHUD();

	const FString Message = TEXT("License Valid - Application Running");
	if (GEngine && GEngine->GetSmallFont() && Canvas)
	{
		FCanvasTextItem TextItem(FVector2D(64.0f, 64.0f), FText::FromString(Message), GEngine->GetSmallFont(), FLinearColor::Green);
		TextItem.Scale = FVector2D(2.0f, 2.0f);
		Canvas->DrawItem(TextItem);
	}
}
