#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JsonObjectWrapper.h"
#include "MQTT_Machine_Test_Actor.generated.h"

class APaho_Manager_Sync;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * A small, reusable visual receiver for testing MQTT equipment states in a level.
 * It listens to the first APaho_Manager_Sync actor in the world and looks for its
 * MachineId in Message.machines[].
 */
UCLASS(BlueprintType, Blueprintable)
class FF_MQTT_SYNC_API AMQTT_Machine_Test_Actor : public AActor
{
	GENERATED_BODY()

public:
	AMQTT_Machine_Test_Actor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The MQTT machines[].machine_id this visual represents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MQTT Test")
	FString MachineId = TEXT("BODY_INPUT_01");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MQTT Test")
	FString CurrentStatus = TEXT("WAITING");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MQTT Test")
	float Utilization = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MQTT Test")
	int32 QueueLength = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MQTT Test")
	float Throughput = 0.0f;

	/** Manually drive the visual; useful for editor and Blueprint testing. */
	UFUNCTION(BlueprintCallable, Category = "MQTT Test")
	void ApplyMachineState(const FString& InStatus, float InUtilization, int32 InQueueLength, float InThroughput);

	/** Called automatically by the MQTT manager's message-arrived delegate. */
	UFUNCTION()
	void HandleMqttMessage(FJsonObjectWrapper InMessage);

private:
	void TryBindToMqttManager();
	void RefreshVisual();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MachineBody;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> StatusLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTextRenderComponent> StatusLabel;

	UPROPERTY(Transient)
	TObjectPtr<APaho_Manager_Sync> MqttManager;

	float BindRetryElapsed = 0.0f;
	float ElapsedTime = 0.0f;
};
