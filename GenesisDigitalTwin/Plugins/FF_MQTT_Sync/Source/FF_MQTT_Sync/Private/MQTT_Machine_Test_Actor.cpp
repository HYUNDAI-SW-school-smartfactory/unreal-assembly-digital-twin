#include "MQTT_Machine_Test_Actor.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Paho_Sync_Manager.h"
#include "UObject/ConstructorHelpers.h"

AMQTT_Machine_Test_Actor::AMQTT_Machine_Test_Actor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MachineBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MachineBody"));
	MachineBody->SetupAttachment(Root);
	MachineBody->SetRelativeScale3D(FVector(1.4f, 0.8f, 0.5f));
	MachineBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MachineBody->SetStaticMesh(CubeMesh.Object);
	}

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(Root);
	StatusLight->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	StatusLight->SetIntensity(3000.0f);
	StatusLight->SetAttenuationRadius(450.0f);

	StatusLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusLabel"));
	StatusLabel->SetupAttachment(Root);
	StatusLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	StatusLabel->SetHorizontalAlignment(EHTA_Center);
	StatusLabel->SetWorldSize(36.0f);
	StatusLabel->SetTextRenderColor(FColor::White);
	StatusLabel->SetText(FText::FromString(TEXT("BODY_INPUT_01\\nWAITING")));
}

void AMQTT_Machine_Test_Actor::BeginPlay()
{
	Super::BeginPlay();
	TryBindToMqttManager();
	RefreshVisual();
}

void AMQTT_Machine_Test_Actor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(MqttManager))
	{
		MqttManager->Delegate_Message_Arrived.RemoveDynamic(this, &AMQTT_Machine_Test_Actor::HandleMqttMessage);
	}

	Super::EndPlay(EndPlayReason);
}

void AMQTT_Machine_Test_Actor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ElapsedTime += DeltaSeconds;

	if (!IsValid(MqttManager))
	{
		BindRetryElapsed += DeltaSeconds;
		if (BindRetryElapsed >= 1.0f)
		{
			BindRetryElapsed = 0.0f;
			TryBindToMqttManager();
		}
	}

	if (CurrentStatus.Equals(TEXT("RUN"), ESearchCase::IgnoreCase))
	{
		MachineBody->AddLocalRotation(FRotator(0.0f, 60.0f * DeltaSeconds, 0.0f));
	}

	if (CurrentStatus.Equals(TEXT("WARNING"), ESearchCase::IgnoreCase) || CurrentStatus.Equals(TEXT("FAULT"), ESearchCase::IgnoreCase))
	{
		StatusLight->SetVisibility(FMath::Sin(ElapsedTime * 7.0f) > 0.0f);
	}
}

void AMQTT_Machine_Test_Actor::ApplyMachineState(const FString& InStatus, float InUtilization, int32 InQueueLength, float InThroughput)
{
	CurrentStatus = InStatus.ToUpper();
	Utilization = InUtilization;
	QueueLength = InQueueLength;
	Throughput = InThroughput;
	RefreshVisual();
}

void AMQTT_Machine_Test_Actor::HandleMqttMessage(FJsonObjectWrapper InMessage)
{
	if (!InMessage.JsonObject.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* Payload = nullptr;
	if (!InMessage.JsonObject->TryGetObjectField(TEXT("Message"), Payload) || !Payload || !Payload->IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Machines = nullptr;
	if (!(*Payload)->TryGetArrayField(TEXT("machines"), Machines) || !Machines)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& MachineValue : *Machines)
	{
		const TSharedPtr<FJsonObject>* Machine = nullptr;
		if (!MachineValue.IsValid() || !MachineValue->TryGetObject(Machine) || !Machine || !Machine->IsValid())
		{
			continue;
		}

		FString ReceivedMachineId;
		if (!(*Machine)->TryGetStringField(TEXT("machine_id"), ReceivedMachineId) || !ReceivedMachineId.Equals(MachineId, ESearchCase::IgnoreCase))
		{
			continue;
		}

		FString Status;
		double ReceivedUtilization = 0.0;
		double ReceivedThroughput = 0.0;
		int32 ReceivedQueueLength = 0;
		(*Machine)->TryGetStringField(TEXT("status"), Status);
		(*Machine)->TryGetNumberField(TEXT("utilization"), ReceivedUtilization);
		(*Machine)->TryGetNumberField(TEXT("throughput"), ReceivedThroughput);
		(*Machine)->TryGetNumberField(TEXT("queue_length"), ReceivedQueueLength);

		ApplyMachineState(Status, static_cast<float>(ReceivedUtilization), ReceivedQueueLength, static_cast<float>(ReceivedThroughput));
		return;
	}
}

void AMQTT_Machine_Test_Actor::TryBindToMqttManager()
{
	if (IsValid(MqttManager) || !GetWorld())
	{
		return;
	}

	for (TActorIterator<APaho_Manager_Sync> It(GetWorld()); It; ++It)
	{
		MqttManager = *It;
		MqttManager->Delegate_Message_Arrived.AddUniqueDynamic(this, &AMQTT_Machine_Test_Actor::HandleMqttMessage);
		return;
	}
}

void AMQTT_Machine_Test_Actor::RefreshVisual()
{
	FLinearColor Color = FLinearColor(0.25f, 0.25f, 0.25f);
	if (CurrentStatus.Equals(TEXT("RUN"), ESearchCase::IgnoreCase))
	{
		Color = FLinearColor(0.0f, 1.0f, 0.15f);
	}
	else if (CurrentStatus.Equals(TEXT("WARNING"), ESearchCase::IgnoreCase))
	{
		Color = FLinearColor(1.0f, 0.65f, 0.0f);
	}
	else if (CurrentStatus.Equals(TEXT("FAULT"), ESearchCase::IgnoreCase))
	{
		Color = FLinearColor::Red;
	}

	StatusLight->SetLightColor(Color);
	StatusLight->SetVisibility(true);
	StatusLabel->SetTextRenderColor(Color.ToFColor(true));
	StatusLabel->SetText(FText::FromString(FString::Printf(TEXT("%s\\n%s | Queue %d | %.0f/h"), *MachineId, *CurrentStatus, QueueLength, Throughput)));
}
