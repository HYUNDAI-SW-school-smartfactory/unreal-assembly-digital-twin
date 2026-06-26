#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JsonObjectWrapper.h"
#include "GenesisHangerFlowManager.generated.h"

class APaho_Manager_Sync;
class USplineComponent;

UENUM(BlueprintType)
enum class EGenesisStationRunStatus : uint8
{
	Run UMETA(DisplayName = "Run"),
	Idle UMETA(DisplayName = "Idle"),
	Stop UMETA(DisplayName = "Stop")
};

USTRUCT(BlueprintType)
struct FGenesisStationRuntime
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	FString ProcessId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	int32 PointIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	float DefaultCycleTime = 5.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	float CurrentCycleTime = 5.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	EGenesisStationRunStatus RunStatus = EGenesisStationRunStatus::Run;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	float Utilization = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	float DefectRate = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	int32 QueueLength = 0;
};

USTRUCT()
struct FGenesisHangerRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Vehicle = nullptr;

	int32 CurrentPointIndex = INDEX_NONE;
	int32 TargetPointIndex = INDEX_NONE;
	float CurrentDistance = 0.0f;
	float TargetDistance = 0.0f;
	bool bMoving = false;
	bool bWorking = false;
	float WorkRemaining = 0.0f;
	int32 WorkingStationIndex = INDEX_NONE;
};

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisHangerFlowManager : public AActor
{
	GENERATED_BODY()

public:
	AGenesisHangerFlowManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Line")
	TObjectPtr<AActor> LineActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Line")
	FString AutoFindLineActorLabel = TEXT("BP_HangerMovingAlongSpline3");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Line")
	bool bAutoFindLineActorByLabel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Line")
	bool bDisableLineActorTick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Line")
	bool bSetLineActorFlowEnabledBool = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Vehicles")
	TSubclassOf<AActor> VehicleClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Vehicles")
	int32 InitialVehicles = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Vehicles")
	int32 MaxVehicles = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Vehicles")
	float MoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Vehicles")
	bool bDestroyExistingVehiclesOnStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Vehicles")
	bool bEnableAutoInputSpawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Vehicles")
	float InputSpawnInterval = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Setup")
	float InitializationDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Points")
	TArray<TObjectPtr<AActor>> FlowPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Points")
	bool bAutoFindFlowPointsByLabel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Points")
	TArray<FString> FlowPointLabels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Stations")
	TArray<FGenesisStationRuntime> Stations;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|Runtime")
	int32 TotalProduced = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|Runtime")
	int32 TotalDefects = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|Runtime")
	FString CurrentScenario;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	bool bApplyScenarioVehiclePresets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	bool bResetVehiclesOnScenarioChange = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	int32 NormalInitialVehicles = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	int32 NormalMaxVehicles = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	float NormalInputSpawnInterval = 9999.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	int32 CycleBottleneckInitialVehicles = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	int32 CycleBottleneckMaxVehicles = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	float CycleBottleneckInputSpawnInterval = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	int32 IdleBottleneckInitialVehicles = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	int32 IdleBottleneckMaxVehicles = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|Scenario Demo")
	float IdleBottleneckInputSpawnInterval = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	bool bEnableMqtt = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	bool bAutoCreateMqttManager = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	bool bUseExistingMqttManager = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	bool bDestroyLegacyMqttActorsOnStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	bool bDestroyOtherPahoManagersOnStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	TArray<FString> LegacyMqttActorNameContains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	FString BrokerAddress = TEXT("ssl://007b4a400f514602846249f6f57ee55e.s1.eu.hivemq.cloud:8883");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	FString SubscribeTopic = TEXT("factory/genesis/line/status");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	FString ClientId = TEXT("ue_genesis_flow_manager");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	FString UserName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	FString Password;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT")
	int32 KeepAliveInterval = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT Debug")
	bool bShowMqttDebugOnScreen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT Debug")
	bool bLogMqttPayloads = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genesis Flow|MQTT Debug")
	float MqttDebugScreenDuration = 0.15f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|MQTT Debug")
	bool bMqttConnected = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|MQTT Debug")
	bool bMqttSubscribed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|MQTT Debug")
	int32 MqttMessageCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|MQTT Debug")
	int32 MqttParseErrorCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|MQTT Debug")
	int32 LastParsedStationCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|MQTT Debug")
	FString LastMqttTopic;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|MQTT Debug")
	FString LastMqttPayloadPreview;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|MQTT Debug")
	FString LastMqttStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|MQTT Debug")
	FString LastMqttParseError;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Genesis Flow|MQTT Debug")
	float LastMqttReceiveWorldTime = -1.0f;

	UFUNCTION(BlueprintCallable, Category = "Genesis Flow")
	void InitializeFlow();

	UFUNCTION(BlueprintCallable, Category = "Genesis Flow")
	void ResetFlow();

	UFUNCTION(BlueprintCallable, Category = "Genesis Flow|MQTT")
	void ApplyLineStatusJsonString(const FString& Payload);

	UFUNCTION()
	void HandleMqttConnected(bool bIsSuccessful, FJsonObjectWrapper OutCode);

	UFUNCTION()
	void HandleMqttMessage(FJsonObjectWrapper InMessage);

	UFUNCTION()
	void HandleMqttConnectionLost(FString Cause);

private:
	UPROPERTY()
	TObjectPtr<USplineComponent> LineSpline = nullptr;

	UPROPERTY()
	TArray<FGenesisHangerRuntime> Hangers;

	TArray<int32> PointHangerIndex;
	FTimerHandle InitializeTimerHandle;
	float InputSpawnCooldown = 0.0f;
	bool bInitialized = false;

	UPROPERTY()
	TObjectPtr<APaho_Manager_Sync> MqttManager = nullptr;

	bool bOwnsMqttManager = false;

	void FillDefaultLabelsAndStations();
	bool ResolveLineAndSpline();
	void ResolveFlowPoints();
	void ConfigureLineActorForCppControl();
	void DestroyExistingVehicles();
	void SpawnInitialVehicles();
	bool SpawnVehicleAtPoint(int32 PointIndex, int32& OutHangerIndex);
	float GetDistanceForPoint(int32 PointIndex) const;
	FTransform GetTransformAtDistance(float Distance) const;
	int32 GetNextPointIndex(int32 PointIndex) const;
	int32 FindStationByPointIndex(int32 PointIndex) const;
	int32 FindStationByProcessId(const FString& ProcessId) const;
	bool IsStationRunnable(int32 StationIndex) const;
	void UpdateMovingHanger(int32 HangerIndex, float DeltaSeconds);
	void UpdateWorkingHanger(int32 HangerIndex, float DeltaSeconds);
	void AdvanceWaitingHangers();
	bool TryMoveHangerToNextPoint(int32 HangerIndex);
	void BeginStationWorkIfNeeded(int32 HangerIndex);
	void TrySpawnAtInput(float DeltaSeconds);
	void ApplyScenarioVehiclePreset(const FString& Scenario, bool bScenarioChanged);
	void ResetVehiclesOnly();
	void RetireVehicleActor(AActor* Vehicle) const;
	void DestroyLegacyMqttActors();
	void BindOrCreateMqttManager();
	void SubscribeToMqttTopic();
	void ProcessMqttPayloadOnGameThread(const FString& TopicName, const FString& PayloadText, bool bHasPayload);
	void ApplyPayloadObject(const TSharedPtr<FJsonObject>& PayloadObject);
	void ApplyStationObject(const TSharedPtr<FJsonObject>& StationObject);
	void SetOwnerBoolProperty(const FName PropertyName, bool bValue) const;
	void DrawMqttDebugOnScreen() const;
	FString BuildMqttDebugText() const;
	void SetMqttStatus(const FString& Status);
};
