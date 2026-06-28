#include "GenesisHangerFlowManager.h"

#include "Components/SplineComponent.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Paho_Sync_Manager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
	FString GetActorRuntimeLabel(const AActor* Actor)
	{
		if (!Actor)
		{
			return FString();
		}

#if WITH_EDITOR
		return Actor->GetActorLabel();
#else
		return Actor->GetName();
#endif
	}
}

AGenesisHangerFlowManager::AGenesisHangerFlowManager()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FClassFinder<AActor> VehicleFinder(TEXT("/Game/Blueprints/BP_HangerVehicle"));
	if (VehicleFinder.Succeeded())
	{
		VehicleClass = VehicleFinder.Class;
	}

	FillDefaultLabelsAndStations();
}

void AGenesisHangerFlowManager::BeginPlay()
{
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(InitializeTimerHandle, this, &AGenesisHangerFlowManager::InitializeFlow, InitializationDelay, false);
}

void AGenesisHangerFlowManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(MqttManager))
	{
		MqttManager->Delegate_Message_Arrived.RemoveDynamic(this, &AGenesisHangerFlowManager::HandleMqttMessage);
	}

	Super::EndPlay(EndPlayReason);
}

void AGenesisHangerFlowManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bInitialized)
	{
		return;
	}

	for (int32 Index = 0; Index < Hangers.Num(); ++Index)
	{
		UpdateMovingHanger(Index, DeltaSeconds);
		UpdateWorkingHanger(Index, DeltaSeconds);
	}

	AdvanceWaitingHangers();
	TrySpawnAtInput(DeltaSeconds);
}

void AGenesisHangerFlowManager::InitializeFlow()
{
	bInitialized = false;

	if (!ResolveLineAndSpline())
	{
		UE_LOG(LogTemp, Error, TEXT("GenesisFlow: Could not resolve line actor or spline."));
		return;
	}

	ConfigureLineActorForCppControl();
	ResolveFlowPoints();

	if (FlowPoints.Num() < 2)
	{
		UE_LOG(LogTemp, Error, TEXT("GenesisFlow: FlowPoints are not configured."));
		return;
	}

	if (bDestroyExistingVehiclesOnStart)
	{
		DestroyExistingVehicles();
	}

	PointHangerIndex.Init(INDEX_NONE, FlowPoints.Num());
	Hangers.Reset();
	SpawnInitialVehicles();

	if (bEnableMqtt)
	{
		BindOrCreateMqttManager();
	}

	bInitialized = true;
	UE_LOG(LogTemp, Display, TEXT("GenesisFlow: Initialized with %d vehicles, %d flow points."), Hangers.Num(), FlowPoints.Num());
}

void AGenesisHangerFlowManager::ResetFlow()
{
	for (FGenesisHangerRuntime& Hanger : Hangers)
	{
		if (IsValid(Hanger.Vehicle))
		{
			Hanger.Vehicle->Destroy();
		}
	}

	Hangers.Reset();
	PointHangerIndex.Reset();
	bInitialized = false;
	InitializeFlow();
}

void AGenesisHangerFlowManager::FillDefaultLabelsAndStations()
{
	if (FlowPointLabels.Num() == 0)
	{
		FlowPointLabels = {
			TEXT("PT_BODY_INPUT_01"),
			TEXT("PT_BUFFER_A_01"),
			TEXT("PT_BUFFER_A_02"),
			TEXT("PT_LIGHT_ASSEMBLY_01"),
			TEXT("PT_BUFFER_B_01"),
			TEXT("PT_BUFFER_B_02"),
			TEXT("PT_BATTERY_MOUNT_01"),
			TEXT("PT_BUFFER_C_01"),
			TEXT("PT_BUFFER_C_02"),
			TEXT("PT_WHEEL_ASSEMBLY_01"),
			TEXT("PT_BUFFER_D_01"),
			TEXT("PT_BUFFER_D_02"),
			TEXT("PT_DOOR_ASSEMBLY_01")
		};
	}

	if (Stations.Num() == 0)
	{
		auto AddStation = [this](const TCHAR* Id, int32 PointIndex, float CycleTime)
		{
			FGenesisStationRuntime Station;
			Station.ProcessId = Id;
			Station.PointIndex = PointIndex;
			Station.DefaultCycleTime = CycleTime;
			Station.CurrentCycleTime = CycleTime;
			Station.RunStatus = EGenesisStationRunStatus::Run;
			Stations.Add(Station);
		};

		AddStation(TEXT("LIGHT_ASSEMBLY_01"), 3, 5.0f);
		AddStation(TEXT("BATTERY_MOUNT_01"), 6, 6.0f);
		AddStation(TEXT("WHEEL_ASSEMBLY_01"), 9, 5.5f);
		AddStation(TEXT("DOOR_ASSEMBLY_01"), 12, 7.0f);
	}
}

bool AGenesisHangerFlowManager::ResolveLineAndSpline()
{
	if (!IsValid(LineActor) && bAutoFindLineActorByLabel)
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			const FString RuntimeActorLabel = GetActorRuntimeLabel(*It);
			if (RuntimeActorLabel.Equals(AutoFindLineActorLabel, ESearchCase::IgnoreCase) ||
				It->GetName().Contains(AutoFindLineActorLabel))
			{
				LineActor = *It;
				break;
			}
		}
	}

	if (!IsValid(LineActor))
	{
		return false;
	}

	LineSpline = LineActor->FindComponentByClass<USplineComponent>();
	return IsValid(LineSpline);
}

void AGenesisHangerFlowManager::ResolveFlowPoints()
{
	if (!bAutoFindFlowPointsByLabel)
	{
		return;
	}

	FlowPoints.SetNum(FlowPointLabels.Num());

	for (int32 LabelIndex = 0; LabelIndex < FlowPointLabels.Num(); ++LabelIndex)
	{
		if (IsValid(FlowPoints[LabelIndex]))
		{
			continue;
		}

		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			const FString& WantedLabel = FlowPointLabels[LabelIndex];
			const FString RuntimeActorLabel = GetActorRuntimeLabel(*It);
			if (RuntimeActorLabel.Equals(WantedLabel, ESearchCase::IgnoreCase) ||
				It->GetName().StartsWith(WantedLabel))
			{
				FlowPoints[LabelIndex] = *It;
				break;
			}
		}

		if (!IsValid(FlowPoints[LabelIndex]))
		{
			UE_LOG(LogTemp, Warning, TEXT("GenesisFlow: Flow point %d (%s) was not found."), LabelIndex, *FlowPointLabels[LabelIndex]);
		}
	}
}

void AGenesisHangerFlowManager::ConfigureLineActorForCppControl()
{
	if (!IsValid(LineActor))
	{
		return;
	}

	if (bDisableLineActorTick)
	{
		LineActor->SetActorTickEnabled(false);
	}

	if (bSetLineActorFlowEnabledBool)
	{
		SetOwnerBoolProperty(TEXT("bFlowEnabled"), true);
	}
}

void AGenesisHangerFlowManager::SetOwnerBoolProperty(const FName PropertyName, bool bValue) const
{
	if (!IsValid(LineActor))
	{
		return;
	}

	if (FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(LineActor->GetClass(), PropertyName))
	{
		BoolProperty->SetPropertyValue_InContainer(LineActor, bValue);
	}
}

void AGenesisHangerFlowManager::DestroyExistingVehicles()
{
	if (!VehicleClass)
	{
		return;
	}

	TArray<AActor*> ExistingVehicles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), VehicleClass, ExistingVehicles);
	for (AActor* Vehicle : ExistingVehicles)
	{
		if (IsValid(Vehicle))
		{
			Vehicle->Destroy();
		}
	}
}

void AGenesisHangerFlowManager::SpawnInitialVehicles()
{
	const int32 Count = FMath::Clamp(InitialVehicles, 0, FMath::Min(MaxVehicles, FlowPoints.Num()));
	for (int32 PointIndex = 0; PointIndex < Count; ++PointIndex)
	{
		int32 HangerIndex = INDEX_NONE;
		SpawnVehicleAtPoint(PointIndex, HangerIndex);
	}
}

bool AGenesisHangerFlowManager::SpawnVehicleAtPoint(int32 PointIndex, int32& OutHangerIndex)
{
	OutHangerIndex = INDEX_NONE;

	if (!VehicleClass || !LineSpline || !FlowPoints.IsValidIndex(PointIndex) || !IsValid(FlowPoints[PointIndex]))
	{
		return false;
	}

	if (PointHangerIndex.IsValidIndex(PointIndex) && PointHangerIndex[PointIndex] != INDEX_NONE)
	{
		return false;
	}

	const float Distance = GetDistanceForPoint(PointIndex);
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Vehicle = GetWorld()->SpawnActor<AActor>(VehicleClass, GetTransformAtDistance(Distance), SpawnParams);
	if (!IsValid(Vehicle))
	{
		return false;
	}

	FGenesisHangerRuntime Runtime;
	Runtime.Vehicle = Vehicle;
	Runtime.CurrentPointIndex = PointIndex;
	Runtime.TargetPointIndex = PointIndex;
	Runtime.CurrentDistance = Distance;
	Runtime.TargetDistance = Distance;

	OutHangerIndex = Hangers.Add(Runtime);
	PointHangerIndex[PointIndex] = OutHangerIndex;
	BeginStationWorkIfNeeded(OutHangerIndex);
	return true;
}

float AGenesisHangerFlowManager::GetDistanceForPoint(int32 PointIndex) const
{
	if (!LineSpline || !FlowPoints.IsValidIndex(PointIndex) || !IsValid(FlowPoints[PointIndex]))
	{
		return 0.0f;
	}

	const FVector PointLocation = FlowPoints[PointIndex]->GetActorLocation();
	const float InputKey = LineSpline->FindInputKeyClosestToWorldLocation(PointLocation);
	return LineSpline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
}

FTransform AGenesisHangerFlowManager::GetTransformAtDistance(float Distance) const
{
	if (!LineSpline)
	{
		return FTransform::Identity;
	}

	return LineSpline->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World, true);
}

int32 AGenesisHangerFlowManager::GetNextPointIndex(int32 PointIndex) const
{
	if (FlowPoints.Num() == 0)
	{
		return INDEX_NONE;
	}

	return (PointIndex + 1) % FlowPoints.Num();
}

int32 AGenesisHangerFlowManager::FindStationByPointIndex(int32 PointIndex) const
{
	for (int32 Index = 0; Index < Stations.Num(); ++Index)
	{
		if (Stations[Index].PointIndex == PointIndex)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

int32 AGenesisHangerFlowManager::FindStationByProcessId(const FString& ProcessId) const
{
	for (int32 Index = 0; Index < Stations.Num(); ++Index)
	{
		if (Stations[Index].ProcessId.Equals(ProcessId, ESearchCase::IgnoreCase))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool AGenesisHangerFlowManager::IsStationRunnable(int32 StationIndex) const
{
	if (!Stations.IsValidIndex(StationIndex))
	{
		return true;
	}

	return Stations[StationIndex].RunStatus == EGenesisStationRunStatus::Run;
}

void AGenesisHangerFlowManager::UpdateMovingHanger(int32 HangerIndex, float DeltaSeconds)
{
	if (!Hangers.IsValidIndex(HangerIndex) || !LineSpline)
	{
		return;
	}

	FGenesisHangerRuntime& Hanger = Hangers[HangerIndex];
	if (!Hanger.bMoving || !IsValid(Hanger.Vehicle))
	{
		return;
	}

	const float SplineLength = LineSpline->GetSplineLength();
	float Remaining = Hanger.TargetDistance - Hanger.CurrentDistance;
	if (Remaining < 0.0f)
	{
		Remaining += SplineLength;
	}

	const float Step = MoveSpeed * DeltaSeconds;
	if (Step >= Remaining)
	{
		Hanger.CurrentDistance = Hanger.TargetDistance;
		Hanger.CurrentPointIndex = Hanger.TargetPointIndex;
		Hanger.bMoving = false;
		Hanger.Vehicle->SetActorTransform(GetTransformAtDistance(Hanger.CurrentDistance), false, nullptr, ETeleportType::TeleportPhysics);
		BeginStationWorkIfNeeded(HangerIndex);
		return;
	}

	Hanger.CurrentDistance += Step;
	if (Hanger.CurrentDistance > SplineLength)
	{
		Hanger.CurrentDistance -= SplineLength;
	}

	Hanger.Vehicle->SetActorTransform(GetTransformAtDistance(Hanger.CurrentDistance), false, nullptr, ETeleportType::TeleportPhysics);
}

void AGenesisHangerFlowManager::UpdateWorkingHanger(int32 HangerIndex, float DeltaSeconds)
{
	if (!Hangers.IsValidIndex(HangerIndex))
	{
		return;
	}

	FGenesisHangerRuntime& Hanger = Hangers[HangerIndex];
	if (!Hanger.bWorking)
	{
		return;
	}

	if (!IsStationRunnable(Hanger.WorkingStationIndex))
	{
		return;
	}

	Hanger.WorkRemaining -= DeltaSeconds;
	if (Hanger.WorkRemaining > 0.0f)
	{
		return;
	}

	if (Stations.IsValidIndex(Hanger.WorkingStationIndex) &&
		Stations[Hanger.WorkingStationIndex].ProcessId.Equals(TEXT("DOOR_ASSEMBLY_01"), ESearchCase::IgnoreCase))
	{
		++TotalProduced;
	}

	Hanger.bWorking = false;
	Hanger.WorkRemaining = 0.0f;
	Hanger.WorkingStationIndex = INDEX_NONE;
}

void AGenesisHangerFlowManager::AdvanceWaitingHangers()
{
	for (int32 PointIndex = PointHangerIndex.Num() - 1; PointIndex >= 0; --PointIndex)
	{
		const int32 HangerIndex = PointHangerIndex[PointIndex];
		if (!Hangers.IsValidIndex(HangerIndex))
		{
			continue;
		}

		TryMoveHangerToNextPoint(HangerIndex);
	}
}

bool AGenesisHangerFlowManager::TryMoveHangerToNextPoint(int32 HangerIndex)
{
	if (!Hangers.IsValidIndex(HangerIndex))
	{
		return false;
	}

	FGenesisHangerRuntime& Hanger = Hangers[HangerIndex];
	if (Hanger.bMoving || Hanger.bWorking || Hanger.CurrentPointIndex == INDEX_NONE)
	{
		return false;
	}

	const int32 CurrentPoint = Hanger.CurrentPointIndex;
	const int32 NextPoint = GetNextPointIndex(CurrentPoint);
	if (!PointHangerIndex.IsValidIndex(CurrentPoint) || !PointHangerIndex.IsValidIndex(NextPoint))
	{
		return false;
	}

	if (PointHangerIndex[NextPoint] != INDEX_NONE)
	{
		return false;
	}

	PointHangerIndex[CurrentPoint] = INDEX_NONE;
	PointHangerIndex[NextPoint] = HangerIndex;

	Hanger.TargetPointIndex = NextPoint;
	Hanger.TargetDistance = GetDistanceForPoint(NextPoint);
	Hanger.bMoving = true;
	return true;
}

void AGenesisHangerFlowManager::BeginStationWorkIfNeeded(int32 HangerIndex)
{
	if (!Hangers.IsValidIndex(HangerIndex))
	{
		return;
	}

	FGenesisHangerRuntime& Hanger = Hangers[HangerIndex];
	const int32 StationIndex = FindStationByPointIndex(Hanger.CurrentPointIndex);
	if (!Stations.IsValidIndex(StationIndex))
	{
		return;
	}

	Hanger.bWorking = true;
	Hanger.WorkingStationIndex = StationIndex;
	Hanger.WorkRemaining = FMath::Max(0.1f, Stations[StationIndex].CurrentCycleTime);
}

void AGenesisHangerFlowManager::TrySpawnAtInput(float DeltaSeconds)
{
	if (!bEnableAutoInputSpawn || Hangers.Num() >= MaxVehicles || PointHangerIndex.Num() == 0)
	{
		return;
	}

	InputSpawnCooldown -= DeltaSeconds;
	if (InputSpawnCooldown > 0.0f)
	{
		return;
	}

	InputSpawnCooldown = InputSpawnInterval;
	if (PointHangerIndex[0] == INDEX_NONE)
	{
		int32 NewIndex = INDEX_NONE;
		SpawnVehicleAtPoint(0, NewIndex);
	}
}

void AGenesisHangerFlowManager::BindOrCreateMqttManager()
{
	for (TActorIterator<APaho_Manager_Sync> It(GetWorld()); It; ++It)
	{
		MqttManager = *It;
		break;
	}

	if (!IsValid(MqttManager) && bAutoCreateMqttManager)
	{
		MqttManager = GetWorld()->SpawnActor<APaho_Manager_Sync>();
	}

	if (!IsValid(MqttManager))
	{
		UE_LOG(LogTemp, Warning, TEXT("GenesisFlow: MQTT manager was not found or created."));
		return;
	}

	MqttManager->Delegate_Message_Arrived.AddUniqueDynamic(this, &AGenesisHangerFlowManager::HandleMqttMessage);

	FPahoClientParams Params;
	Params.Address = BrokerAddress;
	Params.ClientId = FString::Printf(TEXT("%s_%d"), *ClientId, FMath::RandRange(1000, 999999));
	Params.UserName = UserName;
	Params.Password = Password;
	Params.KeepAliveInterval = KeepAliveInterval;
	Params.Version = EMQTTVERSION::V3_1_1;

	FDelegate_Paho_Connection ConnectionDelegate;
	ConnectionDelegate.BindDynamic(this, &AGenesisHangerFlowManager::HandleMqttConnected);
	MqttManager->MQTT_Sync_Init(ConnectionDelegate, Params);
}

void AGenesisHangerFlowManager::HandleMqttConnected(bool bIsSuccessful, FJsonObjectWrapper OutCode)
{
	if (!bIsSuccessful)
	{
		UE_LOG(LogTemp, Error, TEXT("GenesisFlow: MQTT connection failed."));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("GenesisFlow: MQTT connected."));
	SubscribeToMqttTopic();
}

void AGenesisHangerFlowManager::SubscribeToMqttTopic()
{
	if (!IsValid(MqttManager))
	{
		return;
	}

	FJsonObjectWrapper OutCode;
	if (!OutCode.JsonObject.IsValid())
	{
		OutCode.JsonObject = MakeShared<FJsonObject>();
	}
	MqttManager->MQTT_Sync_Subscribe(OutCode, SubscribeTopic, EMQTTQOS::QoS_0);
}

void AGenesisHangerFlowManager::HandleMqttMessage(FJsonObjectWrapper InMessage)
{
	if (!InMessage.JsonObject.IsValid())
	{
		return;
	}

	FString TopicName;
	InMessage.JsonObject->TryGetStringField(TEXT("TopicName"), TopicName);
	if (!SubscribeTopic.IsEmpty() && !TopicName.Equals(SubscribeTopic, ESearchCase::IgnoreCase))
	{
		return;
	}

	const TSharedPtr<FJsonObject>* PayloadObject = nullptr;
	if (InMessage.JsonObject->TryGetObjectField(TEXT("Message"), PayloadObject) && PayloadObject && PayloadObject->IsValid())
	{
		ApplyPayloadObject(*PayloadObject);
		return;
	}

	FString PayloadString;
	if (InMessage.JsonObject->TryGetStringField(TEXT("Message"), PayloadString))
	{
		ApplyLineStatusJsonString(PayloadString);
	}
}

void AGenesisHangerFlowManager::ApplyLineStatusJsonString(const FString& Payload)
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		ApplyPayloadObject(JsonObject);
	}
}

void AGenesisHangerFlowManager::ApplyPayloadObject(const TSharedPtr<FJsonObject>& PayloadObject)
{
	if (!PayloadObject.IsValid())
	{
		return;
	}

	PayloadObject->TryGetStringField(TEXT("scenario"), CurrentScenario);

	const TSharedPtr<FJsonObject>* LineObject = nullptr;
	if (PayloadObject->TryGetObjectField(TEXT("line"), LineObject) && LineObject && LineObject->IsValid())
	{
		double Produced = TotalProduced;
		double Defects = TotalDefects;
		(*LineObject)->TryGetNumberField(TEXT("total_produced"), Produced);
		(*LineObject)->TryGetNumberField(TEXT("total_defects"), Defects);
		TotalProduced = FMath::Max(TotalProduced, static_cast<int32>(Produced));
		TotalDefects = FMath::Max(TotalDefects, static_cast<int32>(Defects));
	}

	const TArray<TSharedPtr<FJsonValue>>* StationValues = nullptr;
	if (PayloadObject->TryGetArrayField(TEXT("stations"), StationValues) && StationValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *StationValues)
		{
			const TSharedPtr<FJsonObject>* StationObject = nullptr;
			if (Value.IsValid() && Value->TryGetObject(StationObject) && StationObject && StationObject->IsValid())
			{
				ApplyStationObject(*StationObject);
			}
		}
	}
}

void AGenesisHangerFlowManager::ApplyStationObject(const TSharedPtr<FJsonObject>& StationObject)
{
	FString ProcessId;
	if (!StationObject.IsValid() || !StationObject->TryGetStringField(TEXT("process_id"), ProcessId))
	{
		return;
	}

	const int32 StationIndex = FindStationByProcessId(ProcessId);
	if (!Stations.IsValidIndex(StationIndex))
	{
		return;
	}

	FGenesisStationRuntime& Station = Stations[StationIndex];

	FString RunStatus;
	if (!StationObject->TryGetStringField(TEXT("run_status"), RunStatus))
	{
		StationObject->TryGetStringField(TEXT("state"), RunStatus);
	}
	RunStatus = RunStatus.ToUpper();

	if (RunStatus == TEXT("IDLE"))
	{
		Station.RunStatus = EGenesisStationRunStatus::Idle;
	}
	else if (RunStatus == TEXT("STOP") || RunStatus == TEXT("FAULT"))
	{
		Station.RunStatus = EGenesisStationRunStatus::Stop;
	}
	else
	{
		Station.RunStatus = EGenesisStationRunStatus::Run;
	}

	double NumberValue = 0.0;
	if (StationObject->TryGetNumberField(TEXT("cycle_time"), NumberValue))
	{
		Station.CurrentCycleTime = FMath::Max(0.1f, static_cast<float>(NumberValue));
	}
	if (StationObject->TryGetNumberField(TEXT("utilization"), NumberValue))
	{
		Station.Utilization = static_cast<float>(NumberValue);
	}
	if (StationObject->TryGetNumberField(TEXT("defect_rate"), NumberValue))
	{
		Station.DefectRate = static_cast<float>(NumberValue);
	}
	if (StationObject->TryGetNumberField(TEXT("queue_length"), NumberValue))
	{
		Station.QueueLength = static_cast<int32>(NumberValue);
	}
}
