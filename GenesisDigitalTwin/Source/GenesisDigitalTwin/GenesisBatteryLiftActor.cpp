#include "GenesisBatteryLiftActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AGenesisBatteryLiftActor::AGenesisBatteryLiftActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(Root);

	LiftRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LiftRoot"));
	LiftRoot->SetupAttachment(Root);

	BatteryPack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BatteryPack"));
	BatteryPack->SetupAttachment(LiftRoot);
	BatteryPack->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Cube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cube"));
	Cube->SetupAttachment(LiftRoot);
	Cube->SetRelativeLocation(FVector(0.0f, 0.0f, -140.0f));
	Cube->SetRelativeScale3D(FVector(1.7f, 1.7f, 4.2f));
	Cube->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		Cube->SetStaticMesh(CubeMeshFinder.Object);
	}
}

void AGenesisBatteryLiftActor::BeginPlay()
{
	Super::BeginPlay();

	LiftStartLocation = LiftRoot ? LiftRoot->GetRelativeLocation() : LiftStartLocation;
	LiftEndLocation = LiftStartLocation + FVector(0.0f, 0.0f, LiftHeight);

	if (BatteryPack)
	{
		BatteryPack->SetVisibility(!bHideBatteryPackOnBeginPlay, true);
	}

	if (LiftRoot)
	{
		LiftRoot->SetRelativeLocation(LiftStartLocation);
	}
}

void AGenesisBatteryLiftActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bLiftPlaying || !LiftRoot)
	{
		return;
	}

	LiftElapsed = FMath::Min(LiftDuration, LiftElapsed + DeltaSeconds);
	const float RawAlpha = LiftDuration > 0.0f ? LiftElapsed / LiftDuration : 1.0f;
	const float Alpha = bUseSmoothStepAlpha
		? FMath::SmoothStep(0.0f, 1.0f, RawAlpha)
		: RawAlpha;

	const FVector Start = bReturningToStart ? LiftEndLocation : LiftStartLocation;
	const FVector End = bReturningToStart ? LiftStartLocation : LiftEndLocation;
	LiftRoot->SetRelativeLocation(FMath::Lerp(Start, End, Alpha));

	if (LiftElapsed >= LiftDuration)
	{
		if (bReturningToStart)
		{
			FinishReturnLift();
		}
		else
		{
			FinishLift();
		}
	}
}

void AGenesisBatteryLiftActor::StartBatteryInstall()
{
	if (bIsInstalling)
	{
		return;
	}

	bIsInstalling = true;
	if (BatteryPack)
	{
		BatteryPack->SetVisibility(true, true);
	}

	PlayLift();
}

void AGenesisBatteryLiftActor::StartLift()
{
	StartBatteryInstall();
}

void AGenesisBatteryLiftActor::PlayLift()
{
	if (!LiftRoot)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(HideBatteryTimerHandle);
	bReturningToStart = false;
	LiftRoot->SetRelativeLocation(LiftStartLocation);
	LiftElapsed = 0.0f;
	bLiftPlaying = true;
	bIsInstalling = true;
	SetActorTickEnabled(true);
	CustomTimeDilation = 1.0f;
}

void AGenesisBatteryLiftActor::ResetLift()
{
	GetWorldTimerManager().ClearTimer(HideBatteryTimerHandle);
	bLiftPlaying = false;
	bReturningToStart = false;
	LiftElapsed = 0.0f;
	bIsInstalling = false;

	if (LiftRoot)
	{
		LiftRoot->SetRelativeLocation(LiftStartLocation);
	}
	if (BatteryPack)
	{
		BatteryPack->SetVisibility(!bHideBatteryPackOnBeginPlay, true);
	}
}

void AGenesisBatteryLiftActor::FinishLift()
{
	bLiftPlaying = false;
	if (LiftRoot)
	{
		LiftRoot->SetRelativeLocation(LiftEndLocation);
	}

	GetWorldTimerManager().SetTimer(
		HideBatteryTimerHandle,
		this,
		&AGenesisBatteryLiftActor::StartReturnLift,
		PostInstallHideDelay,
		false);
}

void AGenesisBatteryLiftActor::StartReturnLift()
{
	HideBatteryPackAfterInstall();

	if (!LiftRoot)
	{
		FinishReturnLift();
		return;
	}

	LiftRoot->SetRelativeLocation(LiftEndLocation);
	LiftElapsed = 0.0f;
	bReturningToStart = true;
	bLiftPlaying = true;
	SetActorTickEnabled(true);
	CustomTimeDilation = 1.0f;
}

void AGenesisBatteryLiftActor::FinishReturnLift()
{
	bLiftPlaying = false;
	bReturningToStart = false;
	LiftElapsed = 0.0f;
	if (LiftRoot)
	{
		LiftRoot->SetRelativeLocation(LiftStartLocation);
	}
	bIsInstalling = false;
}

void AGenesisBatteryLiftActor::HideBatteryPackAfterInstall()
{
	if (BatteryPack)
	{
		BatteryPack->SetVisibility(false, true);
	}
}
