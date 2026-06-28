#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GenesisFactoryStatusWidget.generated.h"

class UTextBlock;

UCLASS()
class GENESISDIGITALTWIN_API UGenesisFactoryStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void SetStatusText(const FString& Value, const FLinearColor& Color);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	FString PendingValue = TEXT("WAITING FOR FACTORY DATA");
	FLinearColor PendingColor = FLinearColor(0.05f, 1.0f, 0.15f);

	void BuildWidgetTreeIfNeeded();
};
