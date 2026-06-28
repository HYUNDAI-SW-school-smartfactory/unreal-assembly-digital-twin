#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JsonObjectWrapper.h"
#include "Factory_Dashboard_Board_Actor.generated.h"

class APaho_Manager_Sync;
class UFactory_Dashboard_Widget;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UWidgetComponent;

/** In-world display board that presents the current MQTT factory-line snapshot. */
UCLASS(BlueprintType, Blueprintable)
class FF_MQTT_SYNC_API AFactory_Dashboard_Board_Actor : public AActor
{
	GENERATED_BODY()

public:
	AFactory_Dashboard_Board_Actor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void HandleMqttMessage(FJsonObjectWrapper InMessage);

private:
	void TryBindToMqttManager();
	void UpdateBoard(const TSharedPtr<FJsonObject>& Payload);
	FLinearColor GetModeColor(const FString& Mode) const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DisplayBoardMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UWidgetComponent> DashboardScreen;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> StatusLight;

	UPROPERTY(Transient)
	TObjectPtr<APaho_Manager_Sync> MqttManager;

	UPROPERTY(Transient)
	TObjectPtr<UFactory_Dashboard_Widget> DashboardWidget;

	float BindRetryElapsed = 0.0f;
};
