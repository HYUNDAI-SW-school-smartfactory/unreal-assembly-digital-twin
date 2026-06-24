#include "Factory_Dashboard_Board_Actor.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Factory_Dashboard_Widget.h"
#include "Paho_Sync_Manager.h"
#include "UObject/ConstructorHelpers.h"

AFactory_Dashboard_Board_Actor::AFactory_Dashboard_Board_Actor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DisplayBoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayBoardMesh"));
	DisplayBoardMesh->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoardMesh(TEXT("/Game/Meshes/SM_DisplayBoard01.SM_DisplayBoard01"));
	if (BoardMesh.Succeeded())
	{
		DisplayBoardMesh->SetStaticMesh(BoardMesh.Object);
	}

	DashboardScreen = CreateDefaultSubobject<UWidgetComponent>(TEXT("DashboardScreen"));
	DashboardScreen->SetupAttachment(DisplayBoardMesh);
	DashboardScreen->SetWidgetSpace(EWidgetSpace::World);
	DashboardScreen->SetWidgetClass(UFactory_Dashboard_Widget::StaticClass());
	DashboardScreen->SetDrawSize(FVector2D(1200.0f, 700.0f));
	DashboardScreen->SetPivot(FVector2D(0.5f, 0.5f));
	DashboardScreen->SetTwoSided(true);
	DashboardScreen->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(DisplayBoardMesh);
	StatusLight->SetRelativeLocation(FVector(0.0f, 0.0f, 160.0f));
	StatusLight->SetIntensity(4500.0f);
	StatusLight->SetAttenuationRadius(550.0f);
	StatusLight->SetLightColor(FLinearColor(0.0f, 1.0f, 0.25f));
}

void AFactory_Dashboard_Board_Actor::BeginPlay()
{
	Super::BeginPlay();
	DashboardWidget = Cast<UFactory_Dashboard_Widget>(DashboardScreen->GetUserWidgetObject());
	TryBindToMqttManager();
}

void AFactory_Dashboard_Board_Actor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(MqttManager))
	{
		MqttManager->Delegate_Message_Arrived.RemoveDynamic(this, &AFactory_Dashboard_Board_Actor::HandleMqttMessage);
	}

	Super::EndPlay(EndPlayReason);
}

void AFactory_Dashboard_Board_Actor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsValid(MqttManager))
	{
		BindRetryElapsed += DeltaSeconds;
		if (BindRetryElapsed >= 1.0f)
		{
			BindRetryElapsed = 0.0f;
			TryBindToMqttManager();
		}
	}
}

void AFactory_Dashboard_Board_Actor::HandleMqttMessage(FJsonObjectWrapper InMessage)
{
	if (!InMessage.JsonObject.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* Payload = nullptr;
	if (InMessage.JsonObject->TryGetObjectField(TEXT("Message"), Payload) && Payload && Payload->IsValid())
	{
		UpdateBoard(*Payload);
	}
}

void AFactory_Dashboard_Board_Actor::TryBindToMqttManager()
{
	if (IsValid(MqttManager) || !GetWorld())
	{
		return;
	}

	for (TActorIterator<APaho_Manager_Sync> It(GetWorld()); It; ++It)
	{
		MqttManager = *It;
		MqttManager->Delegate_Message_Arrived.AddUniqueDynamic(this, &AFactory_Dashboard_Board_Actor::HandleMqttMessage);
		return;
	}
}

void AFactory_Dashboard_Board_Actor::UpdateBoard(const TSharedPtr<FJsonObject>& Payload)
{
	if (!Payload.IsValid())
	{
		return;
	}

	if (!DashboardWidget)
	{
		DashboardWidget = Cast<UFactory_Dashboard_Widget>(DashboardScreen->GetUserWidgetObject());
		if (!DashboardWidget)
		{
			return;
		}
	}

	FString LineId = TEXT("GENESIS FINAL ASSEMBLY");
	FString Mode = TEXT("NORMAL");
	FString EventMessage = TEXT("Live factory status received.");
	FString BottleneckMachineId;
	int32 CompletedVehicles = 0;
	int32 TotalVehicles = 16;
	Payload->TryGetStringField(TEXT("line_id"), LineId);
	Payload->TryGetStringField(TEXT("simulation_mode"), Mode);
	Payload->TryGetStringField(TEXT("event_message"), EventMessage);
	Payload->TryGetStringField(TEXT("bottleneck_machine_id"), BottleneckMachineId);
	Payload->TryGetNumberField(TEXT("completed_vehicles"), CompletedVehicles);
	Payload->TryGetNumberField(TEXT("total_vehicles"), TotalVehicles);

	TArray<FString> MachineRows;
	const TArray<TSharedPtr<FJsonValue>>* Machines = nullptr;
	if (Payload->TryGetArrayField(TEXT("machines"), Machines) && Machines)
	{
		for (const TSharedPtr<FJsonValue>& MachineValue : *Machines)
		{
			const TSharedPtr<FJsonObject>* Machine = nullptr;
			if (!MachineValue.IsValid() || !MachineValue->TryGetObject(Machine) || !Machine || !Machine->IsValid())
			{
				continue;
			}

			FString MachineId;
			FString Status;
			int32 QueueLength = 0;
			double Throughput = 0.0;
			(*Machine)->TryGetStringField(TEXT("machine_id"), MachineId);
			(*Machine)->TryGetStringField(TEXT("status"), Status);
			(*Machine)->TryGetNumberField(TEXT("queue_length"), QueueLength);
			(*Machine)->TryGetNumberField(TEXT("throughput"), Throughput);

			const FString Marker = MachineId.Equals(BottleneckMachineId, ESearchCase::IgnoreCase) ? TEXT("  << ALERT") : TEXT("");
			MachineRows.Add(FString::Printf(TEXT("%-22s  %-11s  Q:%2d  THR:%3.0f%s"), *MachineId, *Status, QueueLength, Throughput, *Marker));
		}
	}

	DashboardWidget->UpdateDashboard(LineId, Mode, EventMessage, BottleneckMachineId, CompletedVehicles, TotalVehicles, MachineRows);
	StatusLight->SetLightColor(GetModeColor(Mode));
}

FLinearColor AFactory_Dashboard_Board_Actor::GetModeColor(const FString& Mode) const
{
	if (Mode.Equals(TEXT("BOTTLENECK"), ESearchCase::IgnoreCase))
	{
		return FLinearColor::Red;
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
