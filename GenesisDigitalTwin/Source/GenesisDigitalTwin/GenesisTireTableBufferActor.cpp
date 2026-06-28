#include "GenesisTireTableBufferActor.h"

#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

AGenesisTireTableBufferActor::AGenesisTireTableBufferActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(Root);

	Point1 = CreateDefaultSubobject<USceneComponent>(TEXT("Point1"));
	Point1->SetupAttachment(Root);
	Point1->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));

	Point2 = CreateDefaultSubobject<USceneComponent>(TEXT("Point2"));
	Point2->SetupAttachment(Root);
	Point2->SetRelativeLocation(FVector(0.0f, 0.0f, 125.0f));

	Point3 = CreateDefaultSubobject<USceneComponent>(TEXT("Point3"));
	Point3->SetupAttachment(Root);
	Point3->SetRelativeLocation(FVector(0.0f, 0.0f, 170.0f));

	Point4 = CreateDefaultSubobject<USceneComponent>(TEXT("Point4"));
	Point4->SetupAttachment(Root);
	Point4->SetRelativeLocation(FVector(0.0f, 0.0f, 215.0f));

	Point5 = CreateDefaultSubobject<USceneComponent>(TEXT("Point5"));
	Point5->SetupAttachment(Root);
	Point5->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));

	static ConstructorHelpers::FClassFinder<AActor> TireFinder(TEXT("/Game/Blueprints/BP_Tire"));
	if (TireFinder.Succeeded())
	{
		TireClass = TireFinder.Class;
	}

	RebuildStackPoints();
}

void AGenesisTireTableBufferActor::BeginPlay()
{
	Super::BeginPlay();

	StoredTires.Reset();
	RebuildStackPoints();
}

bool AGenesisTireTableBufferActor::AddTireToTable(AActor* IncomingTire)
{
	if (!IsValid(IncomingTire))
	{
		return false;
	}

	RebuildStackPoints();

	if (StoredTires.Num() >= MaxCapacity)
	{
		return false;
	}

	const int32 StackIndex = StoredTires.Num();
	if (!StackPoints.IsValidIndex(StackIndex) || !IsValid(StackPoints[StackIndex]))
	{
		return false;
	}

	FTransform StackTransform = StackPoints[StackIndex]->GetComponentTransform();
	StackTransform.SetScale3D(FVector::OneVector);

	IncomingTire->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	IncomingTire->SetActorTransform(StackTransform, false, nullptr, ETeleportType::TeleportPhysics);
	IncomingTire->SetActorHiddenInGame(false);

	SetTireBoolProperty(IncomingTire, TEXT("CanBePicked"), false);
	SetTireBoolProperty(IncomingTire, TEXT("CanBeMounted"), true);

	StoredTires.AddUnique(IncomingTire);

	if (StoredTires.Num() >= MaxCapacity)
	{
		CallNoArgFunction(ConveyorRef, TEXT("StopConveyor"));
	}

	return true;
}

bool AGenesisTireTableBufferActor::TakeTireFromTable(AActor*& TakenTire)
{
	TakenTire = nullptr;

	if (StoredTires.Num() <= 0)
	{
		return false;
	}

	const int32 LastIndex = StoredTires.Num() - 1;
	TakenTire = StoredTires[LastIndex];
	StoredTires.RemoveAt(LastIndex);

	if (IsValid(TakenTire))
	{
		SetTireBoolProperty(TakenTire, TEXT("CanBePicked"), false);
		SetTireBoolProperty(TakenTire, TEXT("CanBeMounted"), true);
	}

	RequestNextTire();
	return IsValid(TakenTire);
}

int32 AGenesisTireTableBufferActor::GetStoredCount() const
{
	return StoredTires.Num();
}

void AGenesisTireTableBufferActor::RequestNextTire()
{
	if (StoredTires.Num() < MaxCapacity)
	{
		CallNoArgFunction(ConveyorRef, TEXT("SpawnNextTire"));
		return;
	}

	CallNoArgFunction(ConveyorRef, TEXT("StopConveyor"));
}

void AGenesisTireTableBufferActor::ClearBuffer(bool bDestroyStoredTires)
{
	if (bDestroyStoredTires)
	{
		for (AActor* Tire : StoredTires)
		{
			if (IsValid(Tire))
			{
				Tire->Destroy();
			}
		}
	}

	StoredTires.Reset();
	RequestNextTire();
}

void AGenesisTireTableBufferActor::RebuildStackPoints()
{
	StackPoints.Reset();
	StackPoints.Add(Point1);
	StackPoints.Add(Point2);
	StackPoints.Add(Point3);
	StackPoints.Add(Point4);
	StackPoints.Add(Point5);

	MaxCapacity = FMath::Max(1, FMath::Min(MaxCapacity, StackPoints.Num()));
}

void AGenesisTireTableBufferActor::SetTireBoolProperty(AActor* Tire, const FName PropertyName, const bool bValue) const
{
	if (!IsValid(Tire))
	{
		return;
	}

	if (FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(Tire->GetClass(), PropertyName))
	{
		BoolProperty->SetPropertyValue_InContainer(Tire, bValue);
	}
}

void AGenesisTireTableBufferActor::CallNoArgFunction(AActor* Target, const FName FunctionName) const
{
	if (!IsValid(Target))
	{
		return;
	}

	if (UFunction* Function = Target->FindFunction(FunctionName))
	{
		TArray<uint8> Params;
		Params.SetNumZeroed(Function->ParmsSize);
		Target->ProcessEvent(Function, Params.GetData());
	}
}
