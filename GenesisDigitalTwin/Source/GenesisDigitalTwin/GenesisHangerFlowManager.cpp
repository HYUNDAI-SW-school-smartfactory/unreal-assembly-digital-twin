#include "GenesisHangerFlowManager.h"

#include "Async/Async.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Paho_Sync_Manager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr int32 GenesisFlowMqttScreenKey = 26062601;

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

	FString RunStatusToString(EGenesisStationRunStatus Status)
	{
		switch (Status)
		{
		case EGenesisStationRunStatus::Idle:
			return TEXT("IDLE");
		case EGenesisStationRunStatus::Stop:
			return TEXT("STOP");
		case EGenesisStationRunStatus::Run:
		default:
			return TEXT("RUN");
		}
	}

	FString CompactJsonObjectToString(const TSharedPtr<FJsonObject>& JsonObject)
	{
		if (!JsonObject.IsValid())
		{
			return FString();
		}

		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return Output;
	}

	FString MakePreview(const FString& Value, int32 MaxLen = 220)
	{
		if (Value.Len() <= MaxLen)
		{
			return Value;
		}

		return Value.Left(MaxLen) + TEXT("...");
	}

	bool TryParseGenesisJsonObject(const FString& Source, TSharedPtr<FJsonObject>& OutJsonObject)
	{
		auto TryParse = [&OutJsonObject](const FString& Candidate)
		{
			OutJsonObject.Reset();
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Candidate);
			return FJsonSerializer::Deserialize(Reader, OutJsonObject) && OutJsonObject.IsValid();
		};

		FString Candidate = Source;
		Candidate.TrimStartAndEndInline();

		if (TryParse(Candidate))
		{
			return true;
		}

		// Some MQTT paths deliver a JSON object as an escaped string:
		// {\"line_id\":\"...\"} or "{ \"line_id\": \"...\" }".
		FString Unescaped = Candidate;
		if (Unescaped.StartsWith(TEXT("\"")) && Unescaped.EndsWith(TEXT("\"")) && Unescaped.Len() >= 2)
		{
			Unescaped = Unescaped.Mid(1, Unescaped.Len() - 2);
		}

		Unescaped.ReplaceInline(TEXT("\\\""), TEXT("\""));
		Unescaped.ReplaceInline(TEXT("\\/"), TEXT("/"));
		Unescaped.ReplaceInline(TEXT("\\n"), TEXT(""));
		Unescaped.ReplaceInline(TEXT("\\r"), TEXT(""));
		Unescaped.ReplaceInline(TEXT("\\t"), TEXT(""));
		Unescaped.TrimStartAndEndInline();

		if (TryParse(Unescaped))
		{
			return true;
		}

		// If the payload has valid JSON plus trailing bytes, keep the object span only.
		int32 FirstBrace = INDEX_NONE;
		int32 LastBrace = INDEX_NONE;
		if (Unescaped.FindChar(TEXT('{'), FirstBrace) && Unescaped.FindLastChar(TEXT('}'), LastBrace) && LastBrace > FirstBrace)
		{
			const FString ObjectOnly = Unescaped.Mid(FirstBrace, LastBrace - FirstBrace + 1);
			if (TryParse(ObjectOnly))
			{
				return true;
			}
		}

		return false;
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

	LegacyMqttActorNameContains = {
		TEXT("BP_Template_Paho_Sync"),
		TEXT("BP_MQTT_MachineTest"),
		TEXT("MQTT_Machine_Test"),
		TEXT("Factory_Dashboard"),
		TEXT("Dashboard_Board")
	};

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
		MqttManager->Delegate_Connection_Lost.RemoveDynamic(this, &AGenesisHangerFlowManager::HandleMqttConnectionLost);

		if (bOwnsMqttManager)
		{
			MqttManager->MQTT_Sync_Destroy();
			MqttManager->Destroy();
		}
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
	DrawMqttDebugOnScreen();
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
		DestroyLegacyMqttActors();
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
			RetireVehicleActor(Hanger.Vehicle);
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
			RetireVehicleActor(Vehicle);
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

void AGenesisHangerFlowManager::ApplyScenarioVehiclePreset(const FString& Scenario, bool bScenarioChanged)
{
	if (!bApplyScenarioVehiclePresets || Scenario.IsEmpty())
	{
		return;
	}

	if (Scenario.Equals(TEXT("NORMAL"), ESearchCase::IgnoreCase))
	{
		InitialVehicles = NormalInitialVehicles;
		MaxVehicles = NormalMaxVehicles;
		InputSpawnInterval = NormalInputSpawnInterval;
	}
	else if (Scenario.Equals(TEXT("CYCLE_TIME_BOTTLENECK"), ESearchCase::IgnoreCase))
	{
		InitialVehicles = CycleBottleneckInitialVehicles;
		MaxVehicles = CycleBottleneckMaxVehicles;
		InputSpawnInterval = CycleBottleneckInputSpawnInterval;
	}
	else if (Scenario.Equals(TEXT("IDLE_BOTTLENECK"), ESearchCase::IgnoreCase))
	{
		InitialVehicles = IdleBottleneckInitialVehicles;
		MaxVehicles = IdleBottleneckMaxVehicles;
		InputSpawnInterval = IdleBottleneckInputSpawnInterval;
	}
	else
	{
		return;
	}

	InitialVehicles = FMath::Clamp(InitialVehicles, 0, FlowPoints.Num());
	MaxVehicles = FMath::Max(InitialVehicles, MaxVehicles);
	InputSpawnInterval = FMath::Max(0.1f, InputSpawnInterval);

	if (bScenarioChanged && bResetVehiclesOnScenarioChange && bInitialized && FlowPoints.Num() > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("GenesisFlow: scenario changed to %s. Resetting vehicles with Initial=%d Max=%d SpawnInterval=%.1f."),
			*Scenario,
			InitialVehicles,
			MaxVehicles,
			InputSpawnInterval);
		ResetVehiclesOnly();
	}
}

void AGenesisHangerFlowManager::ResetVehiclesOnly()
{
	for (FGenesisHangerRuntime& Hanger : Hangers)
	{
		if (IsValid(Hanger.Vehicle))
		{
			RetireVehicleActor(Hanger.Vehicle);
		}
	}

	Hangers.Reset();
	PointHangerIndex.Init(INDEX_NONE, FlowPoints.Num());
	InputSpawnCooldown = InputSpawnInterval;
	SpawnInitialVehicles();
}

void AGenesisHangerFlowManager::RetireVehicleActor(AActor* Vehicle) const
{
	if (!IsValid(Vehicle))
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Vehicle->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		if (PrimitiveComponent->IsSimulatingPhysics())
		{
			PrimitiveComponent->SetSimulatePhysics(false);
		}
		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetComponentTickEnabled(false);
	}

	Vehicle->SetActorEnableCollision(false);
	Vehicle->SetActorTickEnabled(false);
	Vehicle->SetActorHiddenInGame(true);
	Vehicle->SetActorLocation(FVector(0.0, 0.0, -100000.0), false, nullptr, ETeleportType::TeleportPhysics);
}

void AGenesisHangerFlowManager::DestroyLegacyMqttActors()
{
	if (!bDestroyLegacyMqttActorsOnStart && !bDestroyOtherPahoManagersOnStart)
	{
		return;
	}

	TArray<AActor*> ActorsToDestroy;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || Actor == this || Actor == MqttManager)
		{
			continue;
		}

		bool bShouldDestroy = false;
		const FString Identity = FString::Printf(
			TEXT("%s %s %s"),
			*GetActorRuntimeLabel(Actor),
			*Actor->GetName(),
			*Actor->GetClass()->GetName());

		if (bDestroyOtherPahoManagersOnStart && Actor->IsA<APaho_Manager_Sync>())
		{
			bShouldDestroy = true;
		}

		if (!bShouldDestroy && bDestroyLegacyMqttActorsOnStart)
		{
			for (const FString& Token : LegacyMqttActorNameContains)
			{
				if (!Token.IsEmpty() && Identity.Contains(Token, ESearchCase::IgnoreCase))
				{
					bShouldDestroy = true;
					break;
				}
			}
		}

		if (bShouldDestroy)
		{
			ActorsToDestroy.Add(Actor);
		}
	}

	for (AActor* Actor : ActorsToDestroy)
	{
		if (IsValid(Actor))
		{
			UE_LOG(LogTemp, Display, TEXT("GenesisFlow MQTT: destroying legacy MQTT actor '%s' (%s)."),
				*GetActorRuntimeLabel(Actor),
				*Actor->GetClass()->GetName());
			Actor->Destroy();
		}
	}
}

void AGenesisHangerFlowManager::BindOrCreateMqttManager()
{
	bMqttConnected = false;
	bMqttSubscribed = false;
	bOwnsMqttManager = false;
	SetMqttStatus(TEXT("Connecting"));

	if (!IsValid(MqttManager) && bUseExistingMqttManager)
	{
		for (TActorIterator<APaho_Manager_Sync> It(GetWorld()); It; ++It)
		{
			MqttManager = *It;
			break;
		}
	}

	if (!IsValid(MqttManager) && bAutoCreateMqttManager)
	{
		MqttManager = GetWorld()->SpawnActor<APaho_Manager_Sync>();
		bOwnsMqttManager = IsValid(MqttManager);

		if (IsValid(MqttManager))
		{
#if WITH_EDITOR
			MqttManager->SetActorLabel(TEXT("GenesisFlow_MQTT_Manager"));
#endif
			MqttManager->SetActorHiddenInGame(true);
		}
	}

	if (!IsValid(MqttManager))
	{
		SetMqttStatus(TEXT("Manager missing"));
		UE_LOG(LogTemp, Warning, TEXT("GenesisFlow: MQTT manager was not found or created."));
		return;
	}

	MqttManager->Delegate_Message_Arrived.AddUniqueDynamic(this, &AGenesisHangerFlowManager::HandleMqttMessage);
	MqttManager->Delegate_Connection_Lost.AddUniqueDynamic(this, &AGenesisHangerFlowManager::HandleMqttConnectionLost);

	FPahoClientParams Params;
	Params.Address = BrokerAddress;
	Params.ClientId = FString::Printf(TEXT("%s_%d"), *ClientId, FMath::RandRange(1000, 999999));
	Params.UserName = UserName;
	Params.Password = Password;
	Params.KeepAliveInterval = KeepAliveInterval;
	Params.Version = EMQTTVERSION::V3_1_1;

	FDelegate_Paho_Connection ConnectionDelegate;
	ConnectionDelegate.BindDynamic(this, &AGenesisHangerFlowManager::HandleMqttConnected);
	UE_LOG(LogTemp, Display, TEXT("GenesisFlow MQTT: connecting to '%s', topic '%s', client '%s'."),
		*BrokerAddress,
		*SubscribeTopic,
		*Params.ClientId);
	MqttManager->MQTT_Sync_Init(ConnectionDelegate, Params);
}

void AGenesisHangerFlowManager::HandleMqttConnected(bool bIsSuccessful, FJsonObjectWrapper OutCode)
{
	if (!bIsSuccessful)
	{
		bMqttConnected = false;
		bMqttSubscribed = false;
		SetMqttStatus(TEXT("Connection failed"));
		UE_LOG(LogTemp, Error, TEXT("GenesisFlow: MQTT connection failed."));
		return;
	}

	bMqttConnected = true;
	SetMqttStatus(TEXT("Connected"));
	UE_LOG(LogTemp, Display, TEXT("GenesisFlow: MQTT connected."));
	SubscribeToMqttTopic();
}

void AGenesisHangerFlowManager::HandleMqttConnectionLost(FString Cause)
{
	bMqttConnected = false;
	bMqttSubscribed = false;
	SetMqttStatus(FString::Printf(TEXT("Connection lost: %s"), *Cause));
	UE_LOG(LogTemp, Warning, TEXT("GenesisFlow MQTT: connection lost. Cause=%s"), *Cause);
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
	bMqttSubscribed = MqttManager->MQTT_Sync_Subscribe(OutCode, SubscribeTopic, EMQTTQOS::QoS_0);
	SetMqttStatus(bMqttSubscribed ? TEXT("Subscribed") : TEXT("Subscribe failed"));
	UE_LOG(LogTemp, Display, TEXT("GenesisFlow MQTT: subscribe topic='%s' result=%s."),
		*SubscribeTopic,
		bMqttSubscribed ? TEXT("OK") : TEXT("FAILED"));
}

void AGenesisHangerFlowManager::HandleMqttMessage(FJsonObjectWrapper InMessage)
{
	if (!InMessage.JsonObject.IsValid())
	{
		TWeakObjectPtr<AGenesisHangerFlowManager> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			++WeakThis->MqttParseErrorCount;
			WeakThis->LastMqttParseError = TEXT("Wrapper JsonObject invalid");
			WeakThis->SetMqttStatus(TEXT("Invalid wrapper"));
		});
		return;
	}

	FString TopicName;
	InMessage.JsonObject->TryGetStringField(TEXT("TopicName"), TopicName);

	FString PayloadText;
	bool bHasPayload = false;
	const TSharedPtr<FJsonObject>* PayloadObject = nullptr;
	if (InMessage.JsonObject->TryGetObjectField(TEXT("Message"), PayloadObject) && PayloadObject && PayloadObject->IsValid())
	{
		PayloadText = CompactJsonObjectToString(*PayloadObject);
		bHasPayload = true;
	}
	else
	{
		FString PayloadString;
		if (InMessage.JsonObject->TryGetStringField(TEXT("Message"), PayloadString))
		{
			PayloadText = PayloadString;
			bHasPayload = true;
		}
	}

	TWeakObjectPtr<AGenesisHangerFlowManager> WeakThis(this);
	AsyncTask(ENamedThreads::GameThread, [WeakThis, TopicName, PayloadText, bHasPayload]()
	{
		if (!WeakThis.IsValid())
		{
			return;
		}

		WeakThis->ProcessMqttPayloadOnGameThread(TopicName, PayloadText, bHasPayload);
	});
}

void AGenesisHangerFlowManager::ProcessMqttPayloadOnGameThread(const FString& TopicName, const FString& PayloadText, bool bHasPayload)
{
	if (!SubscribeTopic.IsEmpty() && !TopicName.Equals(SubscribeTopic, ESearchCase::IgnoreCase))
	{
		return;
	}

	++MqttMessageCount;
	LastMqttTopic = TopicName;
	LastMqttReceiveWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f;
	LastMqttParseError.Reset();
	LastParsedStationCount = 0;

	if (!bHasPayload)
	{
		++MqttParseErrorCount;
		LastMqttParseError = TEXT("Message field missing or unsupported");
		SetMqttStatus(TEXT("Unsupported message"));
		return;
	}

	LastMqttPayloadPreview = MakePreview(PayloadText);
	const int32 ParseErrorsBefore = MqttParseErrorCount;
	ApplyLineStatusJsonString(PayloadText);
	if (MqttParseErrorCount == ParseErrorsBefore)
	{
		SetMqttStatus(FString::Printf(TEXT("Received / stations=%d"), LastParsedStationCount));
	}

	if (bLogMqttPayloads)
	{
		UE_LOG(LogTemp, Display, TEXT("GenesisFlow MQTT: message #%d topic='%s' parsedStations=%d scenario='%s' payload=%s"),
			MqttMessageCount,
			*LastMqttTopic,
			LastParsedStationCount,
			*CurrentScenario,
			*LastMqttPayloadPreview);
	}
}

void AGenesisHangerFlowManager::ApplyLineStatusJsonString(const FString& Payload)
{
	TSharedPtr<FJsonObject> JsonObject;
	if (TryParseGenesisJsonObject(Payload, JsonObject))
	{
		ApplyPayloadObject(JsonObject);
		return;
	}

	++MqttParseErrorCount;
	LastMqttParseError = TEXT("Payload JSON parse failed");
	SetMqttStatus(TEXT("Payload parse failed"));
	UE_LOG(LogTemp, Warning, TEXT("GenesisFlow MQTT: JSON parse failed. Payload=%s"), *MakePreview(Payload));
}

void AGenesisHangerFlowManager::ApplyPayloadObject(const TSharedPtr<FJsonObject>& PayloadObject)
{
	if (!PayloadObject.IsValid())
	{
		++MqttParseErrorCount;
		LastMqttParseError = TEXT("PayloadObject invalid");
		return;
	}

	FString IncomingScenario;
	PayloadObject->TryGetStringField(TEXT("scenario"), IncomingScenario);
	if (!IncomingScenario.IsEmpty())
	{
		const bool bScenarioChanged = !IncomingScenario.Equals(CurrentScenario, ESearchCase::IgnoreCase);
		CurrentScenario = IncomingScenario;
		ApplyScenarioVehiclePreset(CurrentScenario, bScenarioChanged);
	}

	const TSharedPtr<FJsonObject>* LineObject = nullptr;
	if (PayloadObject->TryGetObjectField(TEXT("line"), LineObject) && LineObject && LineObject->IsValid())
	{
		double Produced = TotalProduced;
		double Defects = TotalDefects;
		(*LineObject)->TryGetNumberField(TEXT("total_produced"), Produced);
		(*LineObject)->TryGetNumberField(TEXT("total_defects"), Defects);
		TotalProduced = FMath::Max(TotalProduced, static_cast<int32>(Produced));
		TotalDefects = FMath::Max(TotalDefects, static_cast<int32>(Defects));

		double NumberValue = 0.0;
		if ((*LineObject)->TryGetNumberField(TEXT("initial_vehicles"), NumberValue))
		{
			InitialVehicles = FMath::Clamp(static_cast<int32>(NumberValue), 0, FlowPoints.Num());
		}
		if ((*LineObject)->TryGetNumberField(TEXT("max_vehicles"), NumberValue))
		{
			MaxVehicles = FMath::Max(InitialVehicles, static_cast<int32>(NumberValue));
		}
		if ((*LineObject)->TryGetNumberField(TEXT("input_spawn_interval"), NumberValue))
		{
			InputSpawnInterval = FMath::Max(0.1f, static_cast<float>(NumberValue));
		}

		bool bResetFlow = false;
		if ((*LineObject)->TryGetBoolField(TEXT("reset_flow"), bResetFlow) && bResetFlow && bInitialized)
		{
			ResetVehiclesOnly();
		}
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
	else
	{
		LastMqttParseError = TEXT("stations array missing");
	}
}

void AGenesisHangerFlowManager::ApplyStationObject(const TSharedPtr<FJsonObject>& StationObject)
{
	FString ProcessId;
	if (!StationObject.IsValid() || !StationObject->TryGetStringField(TEXT("process_id"), ProcessId))
	{
		LastMqttParseError = TEXT("station.process_id missing");
		return;
	}

	const int32 StationIndex = FindStationByProcessId(ProcessId);
	if (!Stations.IsValidIndex(StationIndex))
	{
		LastMqttParseError = FString::Printf(TEXT("Unknown process_id: %s"), *ProcessId);
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

	++LastParsedStationCount;
	UE_LOG(LogTemp, Verbose, TEXT("GenesisFlow MQTT: applied station %s status=%s cycle=%.2f util=%.2f defect=%.3f queue=%d"),
		*Station.ProcessId,
		*RunStatusToString(Station.RunStatus),
		Station.CurrentCycleTime,
		Station.Utilization,
		Station.DefectRate,
		Station.QueueLength);
}

void AGenesisHangerFlowManager::DrawMqttDebugOnScreen() const
{
	if (!bShowMqttDebugOnScreen || !GEngine)
	{
		return;
	}

	const FColor Color = bMqttConnected && bMqttSubscribed ? FColor::Green : FColor::Yellow;
	GEngine->AddOnScreenDebugMessage(GenesisFlowMqttScreenKey, MqttDebugScreenDuration, Color, BuildMqttDebugText());
}

FString AGenesisHangerFlowManager::BuildMqttDebugText() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const FString LastAgeText = LastMqttReceiveWorldTime >= 0.0f
		? FString::Printf(TEXT("%.1fs ago"), FMath::Max(0.0f, Now - LastMqttReceiveWorldTime))
		: TEXT("never");

	FString Text = FString::Printf(
		TEXT("[GenesisFlow MQTT]\nConn:%s  Sub:%s  Msg:%d  ParsedStations:%d  ParseFails:%d\nStatus:%s  Last:%s\nTopic:%s\nScenario:%s  Vehicles:%d/%d  Produced:%d"),
		bMqttConnected ? TEXT("OK") : TEXT("NO"),
		bMqttSubscribed ? TEXT("OK") : TEXT("NO"),
		MqttMessageCount,
		LastParsedStationCount,
		MqttParseErrorCount,
		*LastMqttStatus,
		*LastAgeText,
		LastMqttTopic.IsEmpty() ? TEXT("-") : *LastMqttTopic,
		CurrentScenario.IsEmpty() ? TEXT("-") : *CurrentScenario,
		Hangers.Num(),
		MaxVehicles,
		TotalProduced);

	for (const FGenesisStationRuntime& Station : Stations)
	{
		Text += FString::Printf(
			TEXT("\n%s P%d %s CT:%.1fs Q:%d Util:%.0f%% Def:%.2f%%"),
			*Station.ProcessId,
			Station.PointIndex,
			*RunStatusToString(Station.RunStatus),
			Station.CurrentCycleTime,
			Station.QueueLength,
			Station.Utilization * 100.0f,
			Station.DefectRate * 100.0f);
	}

	if (!LastMqttParseError.IsEmpty())
	{
		Text += FString::Printf(TEXT("\nParseNote: %s"), *LastMqttParseError);
		if (!LastMqttPayloadPreview.IsEmpty())
		{
			Text += FString::Printf(TEXT("\nLastPayload: %s"), *LastMqttPayloadPreview);
		}
	}

	return Text;
}

void AGenesisHangerFlowManager::SetMqttStatus(const FString& Status)
{
	LastMqttStatus = Status;
}
