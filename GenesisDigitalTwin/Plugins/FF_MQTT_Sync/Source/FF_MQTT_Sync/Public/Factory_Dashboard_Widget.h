#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Factory_Dashboard_Widget.generated.h"

class UTextBlock;
class UVerticalBox;

/** Runtime UMG view used by the in-world factory display board. */
UCLASS()
class FF_MQTT_SYNC_API UFactory_Dashboard_Widget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void UpdateDashboard(
		const FString& InLineId,
		const FString& InMode,
		const FString& InEventMessage,
		const FString& InBottleneckMachineId,
		int32 InCompletedVehicles,
		int32 InTotalVehicles,
		const TArray<FString>& InMachineRows);

private:
	UTextBlock* CreateTextBlock(UVerticalBox* Parent, int32 FontSize, const FLinearColor& Color, const FMargin& InPadding = FMargin(0.0f));
	FLinearColor GetModeColor(const FString& Mode) const;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ModeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EventText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProgressText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MachineRowsText;
};
