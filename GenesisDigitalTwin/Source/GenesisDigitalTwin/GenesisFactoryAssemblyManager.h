#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JsonObjectWrapper.h"
#include "GenesisFactoryAssemblyManager.generated.h"

class AGenesisFactoryStatusBoard;
class AGenesisHangerLineActor;
class ASkeletalMeshActor;
class APaho_Manager_Sync;
class UAnimationAsset;
class UAnimSequence;
class USkeletalMesh;
class USceneComponent;
class UWidgetComponent;

UENUM(BlueprintType)
enum class EGenesisAssemblyStation : uint8
{
	DoorRemoval,
	LightInstall,
	BatteryInstall,
	WheelInstall,
	DoorSeatInstall,
	Inspection
};

UENUM()
enum class EGenesisVehicleFlowState : uint8
{
	Queued,
	MovingToBuffer,
	MovingToStation,
	Processing,
	BlockedAfterProcess,
	MovingToUnload,
	ReturningEmpty
};

USTRUCT(BlueprintType)
struct FGenesisStationTelemetry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	EGenesisAssemblyStation Station = EGenesisAssemblyStation::DoorRemoval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry", meta = (ClampMin = "0.1"))
	float CycleTime = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Utilization = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefectRate = 0.00002f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	bool bRunning = true;
};

USTRUCT(BlueprintType)
struct FGenesisCarVisualMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Visuals")
	TArray<FName> DoorComponentNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Visuals")
	TArray<FName> SeatComponentNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Visuals")
	TArray<FName> LightComponentNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Visuals")
	TArray<FName> BatteryComponentNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Visuals")
	TArray<FName> WheelComponentNames;
};

USTRUCT(BlueprintType)
struct FGenesisLineActorBindings
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Bindings")
	TObjectPtr<AActor> AGV = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Bindings")
	TObjectPtr<AActor> BatteryLift = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Bindings")
	TObjectPtr<AActor> TireCellRight = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Bindings")
	TObjectPtr<AActor> TireCellLeft = nullptr;
};

USTRUCT(BlueprintType)
struct FGenesisAssemblyLineConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	int32 LineId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	float CenterY = 1620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	FGenesisLineActorBindings Actors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	TArray<FGenesisStationTelemetry> Stations;
};

USTRUCT()
struct FGenesisVehicleRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Hanger = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> Car = nullptr;

	EGenesisVehicleFlowState State = EGenesisVehicleFlowState::Queued;
	int32 StationIndex = 0;
	float RemainingProcessTime = 0.0f;
	float MoveAlpha = 0.0f;
	float MoveStartDistance = 0.0f;
	float MoveTargetDistance = 0.0f;
	FVector MoveStart = FVector::ZeroVector;
	FVector MoveTarget = FVector::ZeroVector;
	bool bDefective = false;
};

USTRUCT()
struct FGenesisInspectionVehicleRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Car = nullptr;

	float MoveAlpha = 0.0f;
	float Duration = 8.0f;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
};

USTRUCT()
struct FGenesisAssemblyLineRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGenesisVehicleRuntime> Vehicles;

	UPROPERTY()
	TArray<FGenesisInspectionVehicleRuntime> InspectionVehicles;

	TArray<TArray<int32>> StationQueues;
	TArray<int32> StationOccupants;
	TArray<double> BufferFullSince;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> StationBoards;

	UPROPERTY()
	TObjectPtr<AActor> LineBoard = nullptr;

	UPROPERTY()
	TObjectPtr<AGenesisHangerLineActor> HangerLine = nullptr;

	int32 TotalProduced = 0;
	int32 BottleneckStationIndex = INDEX_NONE;
	bool bHasEmptyHangerReturned = false;
};

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisFactoryAssemblyManager : public AActor
{
	GENERATED_BODY()

public:
	AGenesisFactoryAssemblyManager();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory")
	TArray<FGenesisAssemblyLineConfig> Lines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Classes")
	TSubclassOf<AActor> HangerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Classes")
	TSubclassOf<AActor> CarClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Classes")
	TSubclassOf<AGenesisFactoryStatusBoard> StatusBoardClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Classes")
	TSubclassOf<AActor> AGVClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Classes")
	TSubclassOf<AActor> BatteryLiftClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Classes")
	TSubclassOf<AActor> TireCellRightClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Classes")
	TSubclassOf<AActor> TireCellLeftClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Visuals")
	FGenesisCarVisualMapping CarVisuals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Visuals")
	FVector CarSpawnScale = FVector(1.8f, 1.8f, 1.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Visuals")
	TObjectPtr<USkeletalMesh> LeftRobotMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Visuals")
	TObjectPtr<UAnimationAsset> LeftRobotAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Visuals")
	TObjectPtr<UAnimationAsset> RightRobotAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow", meta = (ClampMin = "1", ClampMax = "2"))
	int32 BufferCapacity = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow", meta = (ClampMin = "1"))
	int32 InitialHangersPerLine = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow", meta = (ClampMin = "1"))
	int32 MaxRampUpHangersPerLine = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	bool bRampUpHangersUntilFirstReturn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	bool bUsePreplacedStationRobotsOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	bool bRemovePreviewCarsOnStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	bool bPausePreplacedEquipmentOnStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	bool bKeepTireCellsRunningBetweenVehicles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	bool bSpawnMissingEquipmentAtRuntime = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	bool bUsePreplacedHangerLineGeometry = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	float HangerHeight = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	float HangerRailHeight = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	float TravelTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	float EmptyReturnTime = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	float EmptyReturnLoopOffset = 1680.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	float InspectionTravelTime = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	float InspectionVehicleFloorZ = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	bool bUseLegacyAgvRoutes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow", meta = (ClampMin = "1.0"))
	float AgvRuntimeMoveSpeed = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow", meta = (ClampMin = "0.0"))
	float BatteryLiftRuntimeHeight = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow")
	TArray<float> MinimumStationProcessTimes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow", meta = (ClampMin = "0.0"))
	float BatteryAgvLeadTime = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow", meta = (ClampMin = "0.0"))
	float BatteryLiftMinimumTime = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Flow", meta = (ClampMin = "0.0"))
	float TireCellMinimumTime = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Coordinates")
	TArray<float> StationX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Coordinates")
	float VehicleStopXOffset = -150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Coordinates")
	float UnloadX = 15650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Coordinates")
	float InspectionEndX = 23990.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|Coordinates")
	float RobotSideOffset = 580.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|MQTT")
	bool bEnableMqtt = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|MQTT")
	FString BrokerAddress = TEXT("ssl://007b4a400f514602846249f6f57ee55e.s1.eu.hivemq.cloud:8883");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|MQTT")
	FString SubscribeTopic = TEXT("factory/genesis/line/status");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|MQTT")
	FString UserName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory|MQTT")
	FString Password;

	UFUNCTION(BlueprintCallable, Category = "Factory")
	void InitializeFactory();

	UFUNCTION(BlueprintCallable, Category = "Factory|MQTT")
	void ApplyMqttPayloadJson(const FString& Payload);

	UFUNCTION(BlueprintPure, Category = "Factory")
	int32 GetLineProductionCount(int32 LineId) const;

	UFUNCTION(BlueprintPure, Category = "Factory")
	EGenesisAssemblyStation GetLineBottleneck(int32 LineId, bool& bHasBottleneck) const;

	UFUNCTION()
	void HandleMqttConnected(bool bSuccess, FJsonObjectWrapper Result);

	UFUNCTION()
	void HandleMqttMessage(FJsonObjectWrapper Message);

private:
	UPROPERTY(VisibleAnywhere, Category = "Factory")
	TObjectPtr<USceneComponent> FactoryRoot;

	UPROPERTY()
	TArray<FGenesisAssemblyLineRuntime> RuntimeLines;

	UPROPERTY()
	TObjectPtr<APaho_Manager_Sync> MqttManager = nullptr;

	TMap<TWeakObjectPtr<AActor>, FTransform> EquipmentInitialTransforms;

	void FillDefaults();
	void RemovePreviewCars();
	void RemoveLegacyStatusBoards();
	void AutoBindMapActors();
	void InitializeLine(int32 LineIndex);
	void TickLine(int32 LineIndex, float DeltaSeconds);
	void TickVehicle(int32 LineIndex, int32 VehicleIndex, float DeltaSeconds);
	void TickInspection(int32 LineIndex, float DeltaSeconds);
	void DispatchQueues(int32 LineIndex);
	void UpdateBottleneck(int32 LineIndex);
	void UpdateBoards(int32 LineIndex);
	void StartMovingToStation(int32 LineIndex, int32 VehicleIndex, int32 StationIndex);
	void StartStationProcess(int32 LineIndex, int32 VehicleIndex, int32 StationIndex);
	void CompleteStationProcess(int32 LineIndex, int32 VehicleIndex);
	void MoveVehicleToUnload(int32 LineIndex, int32 VehicleIndex);
	void UnloadVehicle(int32 LineIndex, int32 VehicleIndex);
	void FinishEmptyReturn(int32 LineIndex, int32 VehicleIndex);
	bool SpawnEntryHanger(int32 LineIndex, bool bWithCar);
	void TryRampUpEntryHanger(int32 LineIndex);
	void SpawnCarForHanger(int32 LineIndex, int32 VehicleIndex);
	void TriggerStationVisuals(int32 LineIndex, int32 StationIndex, AActor* Car);
	void ApplyCompletedStationVisuals(int32 StationIndex, AActor* Car);
	void SetMappedComponentsVisible(AActor* Car, const TArray<FName>& Names, bool bVisible) const;
	void SetDefectHighlight(AActor* Car, bool bDefective) const;
	void RotateMappedWheels(AActor* Car, float DeltaDegrees) const;
	void TriggerNoArgFunction(AActor* Target, const FName FunctionName) const;
	void CacheEquipmentInitialTransform(AActor* Target);
	void SetEquipmentPaused(AActor* Target, bool bPaused) const;
	void SetLineEquipmentPaused(int32 LineIndex, bool bPaused) const;
	void SetEquipmentWaiting(AActor* Target, bool bWaiting);
	void SetLineEquipmentWaiting(int32 LineIndex, bool bWaiting);
	void SetStationEquipmentWaiting(int32 LineIndex, int32 StationIndex, bool bWaiting);
	void EnsureStationEquipmentSpawned(int32 LineIndex, int32 StationIndex);
	void DestroyStationEquipment(int32 LineIndex, int32 StationIndex);
	FTransform GetEquipmentSpawnTransform(int32 LineIndex, const FString& EquipmentType) const;
	void PlayStationRobots(int32 LineIndex, int32 StationIndex);
	void CreateOrFindStationRobots(int32 LineIndex, int32 StationIndex);
	void StopPreplacedStationRobotAnimations(int32 LineIndex);
	ASkeletalMeshActor* FindNamedStationRobot(int32 LineIndex, int32 StationIndex, const FString& Side) const;
	void CreateBoards(int32 LineIndex);
	void FixKnownPreplacedBoardTransforms(FGenesisAssemblyLineRuntime& Runtime, int32 LineId);
	void FixKnownPreplacedBoardTransformsByLabels() const;
	AActor* FindPreplacedStatusBoard(int32 LineId, int32 StationIndex) const;
	AActor* FindPreplacedLineBoard(int32 LineId) const;
	AActor* FindPreplacedBoardByCandidates(const TArray<FString>& Candidates) const;
	void ConfigureStatusBoardActor(AActor* Board, bool bLineBoard) const;
	void SetStatusBoardText(AActor* Board, const FString& Value, const FLinearColor& Color) const;
	bool TrySetWidgetComponentText(UWidgetComponent* WidgetComponent, const FString& Value, const FLinearColor& Color) const;
	float GetMinimumProcessTime(int32 StationIndex) const;
	FVector GetStationLocation(int32 LineIndex, int32 StationIndex) const;
	FVector GetBufferLocation(int32 LineIndex, int32 StationIndex, int32 QueuePosition) const;
	float GetVehicleStopX(int32 StationIndex) const;
	FVector GetUnloadLocation(int32 LineIndex) const;
	FVector GetInspectionEndLocation(int32 LineIndex) const;
	FTransform GetLineTransformAtDistance(int32 LineIndex, float Distance) const;
	float GetLineDistanceForX(int32 LineIndex, float X) const;
	FString GetStationDisplayName(int32 StationIndex) const;
	void BindMqtt();
	void ApplyPayloadObject(const TSharedPtr<FJsonObject>& Root);
};
