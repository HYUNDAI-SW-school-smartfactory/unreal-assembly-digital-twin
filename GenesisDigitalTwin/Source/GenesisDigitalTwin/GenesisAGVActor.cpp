#include "GenesisAGVActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/ConstructorHelpers.h"

AGenesisAGVActor::AGenesisAGVActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(Root);

	AGV_Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AGV_Mesh"));
	AGV_Mesh->SetupAttachment(Root);
	AGV_Mesh->SetMobility(EComponentMobility::Movable);

	BatteryPack_Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BatteryPack_Mesh"));
	BatteryPack_Mesh->SetupAttachment(AGV_Mesh);
	BatteryPack_Mesh->SetMobility(EComponentMobility::Movable);
	BatteryPack_Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		BatteryPack_Mesh->SetStaticMesh(CubeMeshFinder.Object);
		BatteryPack_Mesh->SetRelativeScale3D(FVector(1.6f, 1.0f, 0.22f));
		BatteryPack_Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 85.0f));
	}
}

void AGenesisAGVActor::BeginPlay()
{
	Super::BeginPlay();

	InitialLocation = GetActorLocation();
	InitialRotation = GetActorRotation();

	if (Waypoints.Num() == 0)
	{
		Waypoints.Add(InitialLocation);
	}

	CurrentWaypointIndex = FMath::Clamp(CurrentWaypointIndex, 0, Waypoints.Num() - 1);
	TargetLocation = Waypoints[CurrentWaypointIndex];
	bIsMoving = false;
	GoingForward = true;
	SetBatteryPackVisible(!bHideBatteryPackOnBeginPlay);
}

void AGenesisAGVActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsMoving || Waypoints.Num() == 0)
	{
		return;
	}

	MoveTowardWaypoint(DeltaSeconds);
}

void AGenesisAGVActor::StartBatteryInstall()
{
	if (bIsMoving)
	{
		return;
	}

	StartAGV();
}

void AGenesisAGVActor::StartAGV()
{
	if (Waypoints.Num() == 0)
	{
		Waypoints.Add(GetActorLocation());
	}

	if (!bIsMoving)
	{
		GoingForward = true;
		if (CurrentWaypointIndex < 0 || CurrentWaypointIndex >= Waypoints.Num())
		{
			CurrentWaypointIndex = 0;
		}
		if (CurrentWaypointIndex == Waypoints.Num() - 1 && Waypoints.Num() > 1)
		{
			CurrentWaypointIndex = 0;
		}
		TargetLocation = Waypoints[CurrentWaypointIndex];
	}

	if (bShowBatteryPackWhenStarted)
	{
		SetBatteryPackVisible(true);
	}

	bIsMoving = true;
	SetActorTickEnabled(true);
	CustomTimeDilation = 1.0f;
}

void AGenesisAGVActor::StopAGV()
{
	bIsMoving = false;
}

void AGenesisAGVActor::ResetAGV()
{
	bIsMoving = false;
	GoingForward = true;
	CurrentWaypointIndex = 0;
	SetActorLocation(InitialLocation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(InitialRotation, ETeleportType::TeleportPhysics);
	TargetLocation = Waypoints.Num() > 0 ? Waypoints[0] : InitialLocation;
	SetBatteryPackVisible(!bHideBatteryPackOnBeginPlay);
}

void AGenesisAGVActor::MoveTowardWaypoint(float DeltaSeconds)
{
	if (!Waypoints.IsValidIndex(CurrentWaypointIndex))
	{
		StopAGV();
		return;
	}

	TargetLocation = Waypoints[CurrentWaypointIndex];
	const FVector CurrentLocation = GetActorLocation();
	const FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, TargetLocation, DeltaSeconds, MoveSpeed);

	if (!CurrentLocation.Equals(TargetLocation, KINDA_SMALL_NUMBER))
	{
		const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(CurrentLocation, TargetLocation);
		SetActorRotation(FRotator(0.0f, LookAtRotation.Yaw, 0.0f));
	}

	SetActorLocation(NewLocation, false, nullptr, ETeleportType::None);

	if (FVector::Dist(NewLocation, TargetLocation) <= AcceptanceRadius)
	{
		SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		HandleWaypointArrival();
	}
}

void AGenesisAGVActor::HandleWaypointArrival()
{
	if (GoingForward)
	{
		AdvanceForward();
	}
	else
	{
		AdvanceBackward();
	}
}

void AGenesisAGVActor::AdvanceForward()
{
	if (CurrentWaypointIndex + 1 < Waypoints.Num())
	{
		++CurrentWaypointIndex;
		TargetLocation = Waypoints[CurrentWaypointIndex];
		return;
	}

	DeliverBatteryToLift();

	if (bReturnAfterDelivery && Waypoints.Num() > 1)
	{
		GoingForward = false;
		CurrentWaypointIndex = Waypoints.Num() - 2;
		TargetLocation = Waypoints[CurrentWaypointIndex];
	}
	else
	{
		StopAGV();
	}
}

void AGenesisAGVActor::AdvanceBackward()
{
	if (CurrentWaypointIndex - 1 >= 0)
	{
		--CurrentWaypointIndex;
		TargetLocation = Waypoints[CurrentWaypointIndex];
		return;
	}

	GoingForward = true;
	CurrentWaypointIndex = 0;
	TargetLocation = Waypoints[0];
	StopAGV();
}

void AGenesisAGVActor::DeliverBatteryToLift()
{
	if (bHideBatteryPackAtFinalWaypoint)
	{
		SetBatteryPackVisible(false);
	}

	CallBatteryLiftStart();
}

void AGenesisAGVActor::SetBatteryPackVisible(bool bVisible)
{
	if (BatteryPack_Mesh)
	{
		BatteryPack_Mesh->SetVisibility(bVisible, true);
	}
}

void AGenesisAGVActor::CallBatteryLiftStart() const
{
	if (!IsValid(BatteryLiftRef))
	{
		return;
	}

	static const TArray<FName> CandidateFunctions = {
		TEXT("StartBatteryInstall"),
		TEXT("StartLift"),
		TEXT("PlayLift")
	};

	for (const FName FunctionName : CandidateFunctions)
	{
		if (UFunction* Function = BatteryLiftRef->FindFunction(FunctionName))
		{
			TArray<uint8> Params;
			Params.SetNumZeroed(Function->ParmsSize);
			BatteryLiftRef->ProcessEvent(Function, Params.GetData());
			return;
		}
	}
}
