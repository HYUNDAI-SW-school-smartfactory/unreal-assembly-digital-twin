#include "GenesisTireConveyorActor.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

AGenesisTireConveyorActor::AGenesisTireConveyorActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(Root);

	ConveyorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ConveyorRoot"));
	ConveyorRoot->SetupAttachment(Root);

	ConveyorPreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConveyorPreviewMesh"));
	ConveyorPreviewMesh->SetupAttachment(ConveyorRoot);
	ConveyorPreviewMesh->SetMobility(EComponentMobility::Static);
	ConveyorPreviewMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> AssemblyLineMeshFinder(TEXT("/Game/Meshes/SM_AssemblyLine02.SM_AssemblyLine02"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Box01MeshFinder(TEXT("/Game/Meshes/SM_AssemblyLineBox01.SM_AssemblyLineBox01"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Box02MeshFinder(TEXT("/Game/Meshes/SM_AssemblyLineBox02.SM_AssemblyLineBox02"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));

	if (AssemblyLineMeshFinder.Succeeded())
	{
		ConveyorPreviewMesh->SetStaticMesh(AssemblyLineMeshFinder.Object);
	}
	else if (CubeMeshFinder.Succeeded())
	{
		ConveyorPreviewMesh->SetStaticMesh(CubeMeshFinder.Object);
		ConveyorPreviewMesh->SetRelativeScale3D(FVector(8.0f, 1.2f, 0.15f));
	}

	SideBoxLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SideBoxLeft"));
	SideBoxLeft->SetupAttachment(ConveyorRoot);
	SideBoxLeft->SetMobility(EComponentMobility::Static);
	SideBoxLeft->SetRelativeLocation(FVector(0.0f, -95.0f, 82.0f));
	SideBoxLeft->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	if (Box01MeshFinder.Succeeded())
	{
		SideBoxLeft->SetStaticMesh(Box01MeshFinder.Object);
	}

	SideBoxRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SideBoxRight"));
	SideBoxRight->SetupAttachment(ConveyorRoot);
	SideBoxRight->SetMobility(EComponentMobility::Static);
	SideBoxRight->SetRelativeLocation(FVector(0.0f, 95.0f, 82.0f));
	SideBoxRight->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	if (Box02MeshFinder.Succeeded())
	{
		SideBoxRight->SetStaticMesh(Box02MeshFinder.Object);
	}

	for (int32 Index = 0; Index < 24; ++Index)
	{
		const FName ComponentName(*FString::Printf(TEXT("Roller_%02d"), Index + 1));
		UStaticMeshComponent* Roller = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
		Roller->SetupAttachment(ConveyorRoot);
		Roller->SetMobility(EComponentMobility::Static);
		Roller->SetRelativeLocation(FVector(-360.0f + Index * 31.5f, 0.0f, 115.0f));
		Roller->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
		Roller->SetRelativeScale3D(FVector(0.24f, 1.2f, 0.24f));
		if (Box02MeshFinder.Succeeded())
		{
			Roller->SetStaticMesh(Box02MeshFinder.Object);
		}
		else if (CubeMeshFinder.Succeeded())
		{
			Roller->SetStaticMesh(CubeMeshFinder.Object);
		}
		RollerMeshes.Add(Roller);
	}

	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(ConveyorRoot);
	SpawnPoint->SetRelativeLocation(FVector(-400.0f, 0.0f, 120.0f));

	PickupPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PickupPoint"));
	PickupPoint->SetupAttachment(ConveyorRoot);
	PickupPoint->SetRelativeLocation(FVector(400.0f, 0.0f, 120.0f));

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetupAttachment(ConveyorRoot);
	Spline->ClearSplinePoints(false);
	Spline->AddSplinePoint(FVector(-400.0f, 0.0f, 120.0f), ESplineCoordinateSpace::Local, false);
	Spline->AddSplinePoint(FVector(400.0f, 0.0f, 120.0f), ESplineCoordinateSpace::Local, false);
	Spline->UpdateSpline();

	static ConstructorHelpers::FClassFinder<AActor> TireFinder(TEXT("/Game/Blueprints/BP_Tire"));
	if (TireFinder.Succeeded())
	{
		TireClass = TireFinder.Class;
	}
}

void AGenesisTireConveyorActor::BeginPlay()
{
	Super::BeginPlay();

	InitialLocation = GetActorLocation();
	DistanceAlongSpline = 0.0f;

	if (IsValid(TargetTire))
	{
		UpdateTireLocation();
	}
}

void AGenesisTireConveyorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsMoving || !IsValid(TargetTire) || !Spline)
	{
		return;
	}

	const float SplineLength = Spline->GetSplineLength();
	DistanceAlongSpline = FMath::Clamp(
		DistanceAlongSpline + MoveSpeed * DeltaSeconds,
		0.0f,
		SplineLength);

	UpdateTireLocation();

	if (DistanceAlongSpline >= SplineLength - KINDA_SMALL_NUMBER)
	{
		MarkTireReadyForPickup();
	}
}

bool AGenesisTireConveyorActor::SpawnNextTire()
{
	if (bHasActiveTire || IsValid(TargetTire) || !TireClass || !GetWorld())
	{
		return false;
	}

	const FTransform SpawnTransform = SpawnPoint
		? SpawnPoint->GetComponentTransform()
		: GetActorTransform();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TargetTire = GetWorld()->SpawnActor<AActor>(TireClass, SpawnTransform, SpawnParameters);
	if (!IsValid(TargetTire))
	{
		return false;
	}

	SetTireBoolProperty(TargetTire, TEXT("CanBePicked"), false);
	SetTireBoolProperty(TargetTire, TEXT("CanBeMounted"), false);
	SetTireStateProperty(TargetTire, EGenesisTireConveyorTireState::Moving);

	DistanceAlongSpline = 0.0f;
	bHasActiveTire = true;
	TireState = EGenesisTireConveyorTireState::Moving;
	UpdateTireLocation();

	if (bAutoMoveSpawnedTire)
	{
		StartConveyor();
	}

	return true;
}

void AGenesisTireConveyorActor::StartConveyor()
{
	if (!IsValid(TargetTire))
	{
		return;
	}

	bIsMoving = true;
	bHasActiveTire = true;
	SetActorTickEnabled(true);
	CustomTimeDilation = 1.0f;
}

void AGenesisTireConveyorActor::StopConveyor()
{
	bIsMoving = false;
}

void AGenesisTireConveyorActor::ReleaseCurrentTire()
{
	if (IsValid(TargetTire))
	{
		SetTireBoolProperty(TargetTire, TEXT("CanBePicked"), true);
		SetTireBoolProperty(TargetTire, TEXT("CanBeMounted"), false);
		SetTireStateProperty(TargetTire, EGenesisTireConveyorTireState::ReadyForPickup);
	}

	bIsMoving = false;
	bHasActiveTire = false;
	TireState = EGenesisTireConveyorTireState::ReadyForPickup;
	TargetTire = nullptr;
	DistanceAlongSpline = 0.0f;
}

void AGenesisTireConveyorActor::ResetConveyor()
{
	if (IsValid(TargetTire))
	{
		TargetTire->Destroy();
	}

	TargetTire = nullptr;
	bIsMoving = false;
	bHasActiveTire = false;
	DistanceAlongSpline = 0.0f;
	TireState = EGenesisTireConveyorTireState::ReadyForPickup;
	SetActorLocation(InitialLocation);
}

void AGenesisTireConveyorActor::UpdateTireLocation()
{
	if (!IsValid(TargetTire) || !Spline)
	{
		return;
	}

	const FVector NewLocation = Spline->GetLocationAtDistanceAlongSpline(
		DistanceAlongSpline,
		ESplineCoordinateSpace::World);
	const FRotator NewRotation = Spline->GetRotationAtDistanceAlongSpline(
		DistanceAlongSpline,
		ESplineCoordinateSpace::World);

	TargetTire->SetActorLocation(NewLocation, false, nullptr, ETeleportType::None);
	TargetTire->SetActorRotation(NewRotation, ETeleportType::None);
}

void AGenesisTireConveyorActor::MarkTireReadyForPickup()
{
	bIsMoving = false;
	bHasActiveTire = false;
	TireState = EGenesisTireConveyorTireState::ReadyForPickup;

	if (IsValid(TargetTire))
	{
		SetTireBoolProperty(TargetTire, TEXT("CanBePicked"), true);
		SetTireBoolProperty(TargetTire, TEXT("CanBeMounted"), false);
		SetTireStateProperty(TargetTire, EGenesisTireConveyorTireState::ReadyForPickup);
	}
}

void AGenesisTireConveyorActor::SetTireBoolProperty(AActor* Tire, const FName PropertyName, bool bValue) const
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

void AGenesisTireConveyorActor::SetTireStateProperty(AActor* Tire, EGenesisTireConveyorTireState NewState) const
{
	if (!IsValid(Tire))
	{
		return;
	}

	static const TArray<FName> CandidateNames = {
		TEXT("TireState"),
		TEXT("State")
	};

	for (const FName PropertyName : CandidateNames)
	{
		if (FProperty* Property = FindFProperty<FProperty>(Tire->GetClass(), PropertyName))
		{
			if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
			{
				EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(
					EnumProperty->ContainerPtrToValuePtr<void>(Tire),
					static_cast<int64>(NewState));
				return;
			}
			if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
			{
				ByteProperty->SetPropertyValue_InContainer(Tire, static_cast<uint8>(NewState));
				return;
			}
		}
	}
}
