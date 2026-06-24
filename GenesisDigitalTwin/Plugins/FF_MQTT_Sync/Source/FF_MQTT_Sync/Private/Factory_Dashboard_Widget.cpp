#include "Factory_Dashboard_Widget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UFactory_Dashboard_Widget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TitleText || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DashboardCanvas"));
	WidgetTree->RootWidget = Canvas;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DashboardBackground"));
	Background->SetBrushColor(FLinearColor(0.008f, 0.015f, 0.025f, 0.94f));
	UCanvasPanelSlot* BackgroundSlot = Canvas->AddChildToCanvas(Background);
	BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackgroundSlot->SetOffsets(FMargin(0.0f));

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DashboardLayout"));
	Background->SetContent(Layout);

	TitleText = CreateTextBlock(Layout, 36, FLinearColor(0.85f, 0.95f, 1.0f), FMargin(32.0f, 24.0f, 32.0f, 2.0f));
	ModeText = CreateTextBlock(Layout, 28, FLinearColor(0.0f, 1.0f, 0.25f), FMargin(32.0f, 2.0f, 32.0f, 8.0f));
	EventText = CreateTextBlock(Layout, 18, FLinearColor(0.80f, 0.84f, 0.90f), FMargin(32.0f, 4.0f, 32.0f, 12.0f));
	ProgressText = CreateTextBlock(Layout, 22, FLinearColor(0.40f, 0.80f, 1.0f), FMargin(32.0f, 4.0f, 32.0f, 14.0f));
	MachineRowsText = CreateTextBlock(Layout, 19, FLinearColor(0.88f, 0.92f, 0.96f), FMargin(32.0f, 4.0f, 32.0f, 24.0f));

	TitleText->SetText(FText::FromString(TEXT("GENESIS FINAL ASSEMBLY")));
	ModeText->SetText(FText::FromString(TEXT("WAITING FOR MQTT DATA")));
	EventText->SetText(FText::FromString(TEXT("MQTT receiver is standing by.")));
	ProgressText->SetText(FText::FromString(TEXT("COMPLETED 0 / 16")));
	MachineRowsText->SetText(FText::FromString(TEXT("No machine data received.")));
}

void UFactory_Dashboard_Widget::UpdateDashboard(
	const FString& InLineId,
	const FString& InMode,
	const FString& InEventMessage,
	const FString& InBottleneckMachineId,
	int32 InCompletedVehicles,
	int32 InTotalVehicles,
	const TArray<FString>& InMachineRows)
{
	if (!TitleText)
	{
		return;
	}

	const FLinearColor ModeColor = GetModeColor(InMode);
	TitleText->SetText(FText::FromString(FString::Printf(TEXT("%s  |  DIGITAL TWIN"), *InLineId)));
	ModeText->SetText(FText::FromString(FString::Printf(TEXT("LINE MODE: %s%s"), *InMode, InBottleneckMachineId.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  |  BOTTLENECK: %s"), *InBottleneckMachineId))));
	ModeText->SetColorAndOpacity(ModeColor);
	EventText->SetText(FText::FromString(InEventMessage));
	ProgressText->SetText(FText::FromString(FString::Printf(TEXT("COMPLETED VEHICLES  %d / %d"), InCompletedVehicles, InTotalVehicles)));
	MachineRowsText->SetText(FText::FromString(FString::Join(InMachineRows, TEXT("\n"))));
}

UTextBlock* UFactory_Dashboard_Widget::CreateTextBlock(UVerticalBox* Parent, int32 FontSize, const FLinearColor& Color, const FMargin& InPadding)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(Color);
	Text->SetAutoWrapText(true);
	Text->SetJustification(ETextJustify::Left);
	UVerticalBoxSlot* VerticalSlot = Parent->AddChildToVerticalBox(Text);
	VerticalSlot->SetPadding(InPadding);
	return Text;
}

FLinearColor UFactory_Dashboard_Widget::GetModeColor(const FString& Mode) const
{
	if (Mode.Equals(TEXT("BOTTLENECK"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(1.0f, 0.12f, 0.08f);
	}
	if (Mode.Equals(TEXT("TRAINING"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.15f, 0.75f, 1.0f);
	}
	if (Mode.Equals(TEXT("IMPROVED"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.0f, 0.95f, 0.65f);
	}
	if (Mode.Equals(TEXT("COMPLETED"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.75f, 1.0f, 0.30f);
	}
	return FLinearColor(0.0f, 1.0f, 0.25f);
}
