#include "GenesisFactoryStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Widgets/SWidget.h"

void UGenesisFactoryStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTreeIfNeeded();
	SetStatusText(PendingValue, PendingColor);
}

TSharedRef<SWidget> UGenesisFactoryStatusWidget::RebuildWidget()
{
	BuildWidgetTreeIfNeeded();
	return Super::RebuildWidget();
}

void UGenesisFactoryStatusWidget::BuildWidgetTreeIfNeeded()
{
	if (StatusText || !WidgetTree)
	{
		return;
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* ScreenBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ScreenBackground"));
	ScreenBackground->SetBrushColor(FLinearColor(0.002f, 0.008f, 0.004f, 0.98f));
	UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(ScreenBackground);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	FSlateFontInfo Font = StatusText->GetFont();
	Font.Size = 68;
	StatusText->SetFont(Font);
	StatusText->SetJustification(ETextJustify::Left);
	StatusText->SetColorAndOpacity(FLinearColor(0.05f, 1.0f, 0.15f));
	StatusText->SetShadowColorAndOpacity(FLinearColor::Black);
	StatusText->SetShadowOffset(FVector2D(4.0f, 4.0f));
	StatusText->SetAutoWrapText(true);
	StatusText->SetWrapTextAt(1210.0f);
	StatusText->SetText(FText::FromString(PendingValue));
	StatusText->SetColorAndOpacity(PendingColor);

	UOverlaySlot* TextSlot = Root->AddChildToOverlay(StatusText);
	TextSlot->SetHorizontalAlignment(HAlign_Left);
	TextSlot->SetVerticalAlignment(VAlign_Top);
	TextSlot->SetPadding(FMargin(40.0f, 34.0f, 32.0f, 32.0f));
}

void UGenesisFactoryStatusWidget::SetStatusText(const FString& Value, const FLinearColor& Color)
{
	PendingValue = Value;
	PendingColor = Color;
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Value));
		StatusText->SetColorAndOpacity(Color);
	}
}
