#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenesisFactoryStatusBoard.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UGenesisFactoryStatusWidget;

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisFactoryStatusBoard : public AActor
{
	GENERATED_BODY()

public:
	AGenesisFactoryStatusBoard();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Factory Board")
	void SetStationStatus(
		int32 LineId,
		const FString& StationName,
		float Utilization,
		float DefectRate,
		float CycleTime,
		int32 BufferCount,
		bool bIsBottleneck);

	UFUNCTION(BlueprintCallable, Category = "Factory Board")
	void SetLineStatus(int32 LineId, int32 TotalProduced, const FString& BottleneckName);

	UFUNCTION(BlueprintCallable, Category = "Factory Board")
	void SetLineStatusDetailed(int32 LineId, int32 TotalProduced, const FString& BottleneckName, int32 BottleneckBufferCount);

	UFUNCTION(BlueprintCallable, Category = "Factory Board")
	void ConfigureAsStationBoard();

	UFUNCTION(BlueprintCallable, Category = "Factory Board")
	void ConfigureAsLineBoard();

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MonitorMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> MonitorWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGenesisFactoryStatusWidget> StatusWidget;

	void EnsureMonitorWidgetReady();
	void UpdateScreen(const FString& Value, const FLinearColor& Color);
};
