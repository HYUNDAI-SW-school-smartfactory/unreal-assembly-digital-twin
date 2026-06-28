#include "GenesisTireAssemblyCellActor.h"

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GenesisRobotTirePickerActor.h"
#include "GenesisRobotTirePickerToCarActor.h"
#include "GenesisTireConveyorActor.h"
#include "GenesisTireTableBufferActor.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

AGenesisTireAssemblyCellActor::AGenesisTireAssemblyCellActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(Root);

	Robot_TableToCar = CreateDefaultSubobject<UChildActorComponent>(TEXT("Robot_TableToCar"));
	Robot_TableToCar->SetupAttachment(Root);
	Robot_TableToCar->SetChildActorClass(AGenesisRobotTirePickerToCarActor::StaticClass());

	Robot_ConveyorToTable = CreateDefaultSubobject<UChildActorComponent>(TEXT("Robot_ConveyorToTable"));
	Robot_ConveyorToTable->SetupAttachment(Root);
	Robot_ConveyorToTable->SetChildActorClass(AGenesisRobotTirePickerActor::StaticClass());

	ConveyorActor = CreateDefaultSubobject<UChildActorComponent>(TEXT("ConveyorActor"));
	ConveyorActor->SetupAttachment(Root);
	ConveyorActor->SetChildActorClass(AGenesisTireConveyorActor::StaticClass());

	TireActor = CreateDefaultSubobject<UChildActorComponent>(TEXT("TireActor"));
	TireActor->SetupAttachment(Root);

	TireTableBuffer = CreateDefaultSubobject<UChildActorComponent>(TEXT("TireTableBuffer"));
	TireTableBuffer->SetupAttachment(Root);
	TireTableBuffer->SetChildActorClass(AGenesisTireTableBufferActor::StaticClass());

	P_CarMount = CreateDefaultSubobject<USceneComponent>(TEXT("P_CarMount"));
	P_CarMount->SetupAttachment(Root);
	P_CarMount->SetRelativeLocation(FVector(300.0f, 0.0f, 120.0f));

	P_ConveyorPickup = CreateDefaultSubobject<USceneComponent>(TEXT("P_ConveyorPickup"));
	P_ConveyorPickup->SetupAttachment(Root);
	P_ConveyorPickup->SetRelativeLocation(FVector(-300.0f, -160.0f, 120.0f));

	P_TableDrop = CreateDefaultSubobject<USceneComponent>(TEXT("P_TableDrop"));
	P_TableDrop->SetupAttachment(Root);
	P_TableDrop->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));

	P_TablePickup = CreateDefaultSubobject<USceneComponent>(TEXT("P_TablePickup"));
	P_TablePickup->SetupAttachment(Root);
	P_TablePickup->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));

	TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
	TableMesh->SetupAttachment(Root);
	TableMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 38.0f));
	TableMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DeskMeshFinder(TEXT("/Game/Meshes/SM_AssemblyLineDesk01.SM_AssemblyLineDesk01"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DeskMeshFinder.Succeeded())
	{
		TableMesh->SetStaticMesh(DeskMeshFinder.Object);
	}
	else if (CubeMeshFinder.Succeeded())
	{
		TableMesh->SetStaticMesh(CubeMeshFinder.Object);
		TableMesh->SetRelativeScale3D(FVector(2.0f, 1.5f, 0.12f));
	}

	ApplyDefaultSideLayout();
}

void AGenesisTireAssemblyCellActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bApplyDefaultSideLayout)
	{
		ApplyDefaultSideLayout();
	}
}

void AGenesisTireAssemblyCellActor::BeginPlay()
{
	Super::BeginPlay();

	BindChildActors();
	ConfigureChildReferences();

	if (bAutoSpawnFirstTire)
	{
		SpawnNextTire();
	}
}

void AGenesisTireAssemblyCellActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickRobotDispatch();
}

bool AGenesisTireAssemblyCellActor::SpawnNextTire()
{
	if (!IsValid(ConveyorRef))
	{
		BindChildActors();
	}

	if (!IsValid(ConveyorRef))
	{
		return false;
	}

	if (UFunction* Function = ConveyorRef->FindFunction(TEXT("SpawnNextTire")))
	{
		struct FSpawnNextTireParams
		{
			bool Success = false;
		};

		FSpawnNextTireParams Params;
		ConveyorRef->ProcessEvent(Function, &Params);
		TireRef = GetActorProperty(ConveyorRef, TEXT("TargetTire"));
		return Params.Success;
	}

	return false;
}

void AGenesisTireAssemblyCellActor::StartPickAndPlace()
{
	if (!IsValid(RobotToTableRef))
	{
		return;
	}

	if (!IsValid(TireRef) && IsValid(ConveyorRef))
	{
		TireRef = GetActorProperty(ConveyorRef, TEXT("TargetTire"));
	}

	SetActorProperty(RobotToTableRef, TEXT("TargetTire"), TireRef);
	SetActorProperty(RobotToTableRef, TEXT("ConveyorRef"), ConveyorRef);
	SetActorProperty(RobotToTableRef, TEXT("TableBufferRef"), TireTableBufferRef);
	SetSceneComponentProperty(RobotToTableRef, TEXT("SourcePointRef"), P_ConveyorPickup);
	SetSceneComponentProperty(RobotToTableRef, TEXT("DestPointRef"), P_TableDrop);

	CallNoArgFunction(RobotToTableRef, TEXT("StartPickAndPlace"));
	bHasStartedRobotToTable = true;
}

void AGenesisTireAssemblyCellActor::StartVehicleTireMount()
{
	bVehicleWaitingForTires = true;
	bHasStartedRobotToCar = false;
	VehicleMountRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void AGenesisTireAssemblyCellActor::StartMountTire()
{
	if (!IsValid(RobotToCarRef))
	{
		return;
	}

	SetActorProperty(RobotToCarRef, TEXT("TableBufferRef"), TireTableBufferRef);
	SetActorProperty(RobotToCarRef, TEXT("RobotToCarRef"), RobotToCarRef);
	SetSceneComponentProperty(RobotToCarRef, TEXT("SourcePointRef"), P_TablePickup);
	SetSceneComponentProperty(RobotToCarRef, TEXT("DestPointRef"), P_CarMount);

	CallNoArgFunction(RobotToCarRef, TEXT("StartMountTire"));
	bHasStartedRobotToCar = true;
}

void AGenesisTireAssemblyCellActor::ResetCell()
{
	bHasStartedRobotToTable = false;
	bHasStartedRobotToCar = false;
	bVehicleWaitingForTires = false;
	VehicleMountRequestTime = -1.0;
	TireRef = nullptr;

	CallNoArgFunction(ConveyorRef, TEXT("ResetConveyor"));
	CallNoArgFunction(RobotToTableRef, TEXT("ResetRobot"));
	CallNoArgFunction(RobotToCarRef, TEXT("ResetRobot"));
}

void AGenesisTireAssemblyCellActor::ApplyDefaultSideLayout()
{
	const bool bLeft = CellSide == EGenesisTireAssemblyCellSide::Left;
	const float SideSign = bLeft ? -1.0f : 1.0f;

	if (ConveyorActor)
	{
		ConveyorActor->SetRelativeLocation(FVector(-520.0f, SideSign * 250.0f, 0.0f));
		ConveyorActor->SetRelativeRotation(FRotator(0.0f, bLeft ? -18.0f : 18.0f, 0.0f));
	}

	if (TireTableBuffer)
	{
		TireTableBuffer->SetRelativeLocation(FVector(-40.0f, SideSign * 120.0f, 0.0f));
		TireTableBuffer->SetRelativeRotation(FRotator(0.0f, bLeft ? -90.0f : 90.0f, 0.0f));
	}

	if (Robot_ConveyorToTable)
	{
		Robot_ConveyorToTable->SetRelativeLocation(FVector(-260.0f, SideSign * 470.0f, 0.0f));
		Robot_ConveyorToTable->SetRelativeRotation(FRotator(0.0f, bLeft ? -145.0f : 145.0f, 0.0f));
	}

	if (Robot_TableToCar)
	{
		Robot_TableToCar->SetRelativeLocation(FVector(355.0f, SideSign * 260.0f, 0.0f));
		Robot_TableToCar->SetRelativeRotation(FRotator(0.0f, bLeft ? -118.0f : 118.0f, 0.0f));
	}

	if (P_ConveyorPickup)
	{
		P_ConveyorPickup->SetRelativeLocation(FVector(-160.0f, SideSign * 250.0f, 125.0f));
	}
	if (P_TableDrop)
	{
		P_TableDrop->SetRelativeLocation(FVector(-40.0f, SideSign * 92.0f, 128.0f));
	}
	if (P_TablePickup)
	{
		P_TablePickup->SetRelativeLocation(FVector(52.0f, SideSign * 88.0f, 128.0f));
	}
	if (P_CarMount)
	{
		P_CarMount->SetRelativeLocation(FVector(430.0f, SideSign * 72.0f, 115.0f));
	}
	if (TableMesh)
	{
		TableMesh->SetRelativeLocation(FVector(0.0f, SideSign * 95.0f, 38.0f));
		TableMesh->SetRelativeRotation(FRotator(0.0f, bLeft ? 180.0f : 0.0f, 0.0f));
	}
}

void AGenesisTireAssemblyCellActor::BindChildActors()
{
	if (!IsValid(RobotToTableRef))
	{
		RobotToTableRef = GetChildActor(Robot_ConveyorToTable);
	}
	if (!IsValid(RobotToCarRef))
	{
		RobotToCarRef = GetChildActor(Robot_TableToCar);
	}
	if (!IsValid(ConveyorRef))
	{
		ConveyorRef = GetChildActor(ConveyorActor);
	}
	if (!IsValid(TireRef))
	{
		TireRef = GetChildActor(TireActor);
	}
	if (!IsValid(TireTableBufferRef))
	{
		TireTableBufferRef = GetChildActor(TireTableBuffer);
	}
}

void AGenesisTireAssemblyCellActor::ConfigureChildReferences()
{
	SetActorProperty(RobotToTableRef, TEXT("ConveyorRef"), ConveyorRef);
	SetActorProperty(RobotToTableRef, TEXT("TableBufferRef"), TireTableBufferRef);
	SetSceneComponentProperty(RobotToTableRef, TEXT("SourcePointRef"), P_ConveyorPickup);
	SetSceneComponentProperty(RobotToTableRef, TEXT("DestPointRef"), P_TableDrop);

	SetActorProperty(RobotToCarRef, TEXT("TableBufferRef"), TireTableBufferRef);
	SetSceneComponentProperty(RobotToCarRef, TEXT("SourcePointRef"), P_TablePickup);
	SetSceneComponentProperty(RobotToCarRef, TEXT("DestPointRef"), P_CarMount);

	SetActorProperty(ConveyorRef, TEXT("TableBufferRef"), TireTableBufferRef);
}

void AGenesisTireAssemblyCellActor::TickRobotDispatch()
{
	if (!IsValid(TireTableBufferRef) || !IsValid(ConveyorRef))
	{
		return;
	}

	AActor* ConveyorTire = GetActorProperty(ConveyorRef, TEXT("TargetTire"));
	if (IsValid(ConveyorTire))
	{
		TireRef = ConveyorTire;
	}

	const bool bTireCanBePicked = GetBoolProperty(TireRef, TEXT("CanBePicked"), false);
	const bool bRobotToTableBusy = GetBoolProperty(RobotToTableRef, TEXT("IsBusy"), false);
	if (IsValid(TireRef) && bTireCanBePicked && !bRobotToTableBusy)
	{
		StartPickAndPlace();
		CallNoArgFunction(ConveyorRef, TEXT("ReleaseCurrentTire"));
		SpawnNextTire();
	}

	const int32 StoredCount = CallIntFunction(TireTableBufferRef, TEXT("GetStoredCount"), 0);
	const bool bRobotToCarBusy = GetBoolProperty(RobotToCarRef, TEXT("IsBusy"), false);
	const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bMountRequestDelayElapsed = VehicleMountRequestTime < 0.0 ||
		(CurrentTime - VehicleMountRequestTime) >= VehicleMountStartDelay;
	if (bVehicleWaitingForTires &&
		!bHasStartedRobotToCar &&
		bMountRequestDelayElapsed &&
		StoredCount >= BufferCountToStartMount &&
		!bRobotToCarBusy)
	{
		StartMountTire();
		bVehicleWaitingForTires = false;
		VehicleMountRequestTime = -1.0;
	}
}

AActor* AGenesisTireAssemblyCellActor::GetChildActor(const UChildActorComponent* ChildActorComponent) const
{
	return ChildActorComponent ? ChildActorComponent->GetChildActor() : nullptr;
}

bool AGenesisTireAssemblyCellActor::CallBoolFunction(AActor* Target, FName FunctionName) const
{
	if (!IsValid(Target))
	{
		return false;
	}

	if (UFunction* Function = Target->FindFunction(FunctionName))
	{
		struct FBoolReturnParams
		{
			bool ReturnValue = false;
		};

		FBoolReturnParams Params;
		Target->ProcessEvent(Function, &Params);
		return Params.ReturnValue;
	}

	return false;
}

void AGenesisTireAssemblyCellActor::CallNoArgFunction(AActor* Target, FName FunctionName) const
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

int32 AGenesisTireAssemblyCellActor::CallIntFunction(AActor* Target, FName FunctionName, int32 DefaultValue) const
{
	if (!IsValid(Target))
	{
		return DefaultValue;
	}

	if (UFunction* Function = Target->FindFunction(FunctionName))
	{
		struct FIntReturnParams
		{
			int32 ReturnValue = 0;
		};

		FIntReturnParams Params;
		Target->ProcessEvent(Function, &Params);
		return Params.ReturnValue;
	}

	return DefaultValue;
}

bool AGenesisTireAssemblyCellActor::GetBoolProperty(AActor* Target, FName PropertyName, bool DefaultValue) const
{
	if (!IsValid(Target))
	{
		return DefaultValue;
	}

	if (FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(Target->GetClass(), PropertyName))
	{
		return BoolProperty->GetPropertyValue_InContainer(Target);
	}

	return DefaultValue;
}

AActor* AGenesisTireAssemblyCellActor::GetActorProperty(AActor* Target, FName PropertyName) const
{
	if (!IsValid(Target))
	{
		return nullptr;
	}

	if (FObjectProperty* ObjectProperty = FindFProperty<FObjectProperty>(Target->GetClass(), PropertyName))
	{
		return Cast<AActor>(ObjectProperty->GetObjectPropertyValue_InContainer(Target));
	}

	return nullptr;
}

void AGenesisTireAssemblyCellActor::SetActorProperty(AActor* Target, FName PropertyName, AActor* Value) const
{
	if (!IsValid(Target))
	{
		return;
	}

	if (FObjectProperty* ObjectProperty = FindFProperty<FObjectProperty>(Target->GetClass(), PropertyName))
	{
		ObjectProperty->SetObjectPropertyValue_InContainer(Target, Value);
	}
}

void AGenesisTireAssemblyCellActor::SetSceneComponentProperty(AActor* Target, FName PropertyName, USceneComponent* Value) const
{
	if (!IsValid(Target))
	{
		return;
	}

	if (FObjectProperty* ObjectProperty = FindFProperty<FObjectProperty>(Target->GetClass(), PropertyName))
	{
		ObjectProperty->SetObjectPropertyValue_InContainer(Target, Value);
	}
}
