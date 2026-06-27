#include "GenesisHangerCarrier.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGenesisHangerCarrier::AGenesisHangerCarrier()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("SplineFollower"));
	SetRootComponent(Root);

	Trolley = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HangerTrolley"));
	Trolley->SetupAttachment(Root);
	Trolley->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trolley->SetRelativeScale3D(FVector(1.8f, 0.65f, 0.3f));

	VerticalSupport = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HangerVerticalSupport"));
	VerticalSupport->SetupAttachment(Root);
	VerticalSupport->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VerticalSupport->SetRelativeLocation(FVector(0.0f, 0.0f, -360.0f));
	VerticalSupport->SetRelativeScale3D(FVector(0.12f, 0.12f, 7.2f));
	VerticalSupport->SetVisibility(false, true);
	VerticalSupport->SetHiddenInGame(true, true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Trolley->SetStaticMesh(CubeFinder.Object);
		VerticalSupport->SetStaticMesh(CubeFinder.Object);
	}

	ClutchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClutchMesh"));
	ClutchMesh->SetupAttachment(Root);
	ClutchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClutchMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -710.0f));
	ClutchMesh->SetRelativeRotation(FRotator::ZeroRotator);
	ClutchMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ClutchFinder(TEXT("/Game/Meshes/SM_Clutch02.SM_Clutch02"));
	if (ClutchFinder.Succeeded())
	{
		ClutchMesh->SetStaticMesh(ClutchFinder.Object);
	}

	CarAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("Car"));
	CarAnchor->SetupAttachment(Root);
	CarAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, -640.0f));
}
