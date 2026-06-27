#include "GenesisRobotTirePickerToCarActor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GenesisTireTableBufferActor.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

AGenesisRobotTirePickerToCarActor::AGenesisRobotTirePickerToCarActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(Root);

	GripperPoint = CreateDefaultSubobject<USceneComponent>(TEXT("GripperPoint"));
	GripperPoint->SetupAttachment(Root);

	Robot = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Robot"));
	Robot->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RobotMeshFinder(TEXT("/Game/Meshes/RobotLarge/SK_IndustrialRobot.SK_IndustrialRobot"));
	if (RobotMeshFinder.Succeeded())
	{
		Robot->SetSkeletalMesh(RobotMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> MountAnim1Finder(TEXT("/Game/Meshes/RobotLarge/SK_IndustrialRobot_Left_Anim.SK_IndustrialRobot_Left_Anim"));
	if (MountAnim1Finder.Succeeded())
	{
		MountAnimation1 = MountAnim1Finder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> MountAnim2Finder(TEXT("/Game/Meshes/RobotLarge/SK_IndustrialRobot_Right_Anim.SK_IndustrialRobot_Right_Anim"));
	if (MountAnim2Finder.Succeeded())
	{
		MountAnimation2 = MountAnim2Finder.Object;
	}
}

bool AGenesisRobotTirePickerToCarActor::StartMountTire()
{
	if (bIsBusy)
	{
		return false;
	}

	bIsBusy = true;
	CurrentMountIndex = 0;
	StartSingleMount();
	return true;
}

void AGenesisRobotTirePickerToCarActor::StartSingleMount()
{
	if (!bIsBusy)
	{
		return;
	}

	FTimerHandle TakeTimerHandle;
	GetWorldTimerManager().SetTimer(
		TakeTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			TakeTireAndPlayMount();
		}),
		FMath::Max(0.0f, TakeFromTableDelay),
		false);
}

void AGenesisRobotTirePickerToCarActor::ResetRobot()
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	bIsBusy = false;
	TargetTire = nullptr;
	CurrentMountIndex = 0;
}

void AGenesisRobotTirePickerToCarActor::TakeTireAndPlayMount()
{
	if (!bIsBusy)
	{
		return;
	}

	AActor* TakenTire = nullptr;
	if (!CallTakeTireFromTable(TakenTire) || !IsValid(TakenTire))
	{
		FinishAllMounts();
		return;
	}

	TargetTire = TakenTire;
	SetBoolProperty(TargetTire, TEXT("CanBeMounted"), false);

	UAnimationAsset* AnimationToPlay = nullptr;
	if (CurrentMountIndex == 0)
	{
		AnimationToPlay = MountAnimation1 ? MountAnimation1.Get() : PickPlaceAnim.Get();
	}
	else
	{
		AnimationToPlay = MountAnimation2 ? MountAnimation2.Get() : MountAnimation1.Get();
	}

	if (Robot && AnimationToPlay)
	{
		Robot->PlayAnimation(AnimationToPlay, false);
	}

	FTimerHandle AttachTimerHandle;
	GetWorldTimerManager().SetTimer(
		AttachTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			AttachTireToRobot();
		}),
		FMath::Max(0.0f, AttachTime),
		false);

	FTimerHandle FinishTimerHandle;
	GetWorldTimerManager().SetTimer(
		FinishTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			FinishSingleMount();
		}),
		FMath::Max(FMath::Max(AttachTime, DetachTime), MountDuration),
		false);
}

void AGenesisRobotTirePickerToCarActor::AttachTireToRobot()
{
	if (!IsValid(TargetTire) || !Robot)
	{
		return;
	}

	TargetTire->AttachToComponent(
		Robot,
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true),
		GripSocketName);
}

void AGenesisRobotTirePickerToCarActor::FinishSingleMount()
{
	if (IsValid(TargetTire))
	{
		TargetTire->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		TargetTire->Destroy();
		TargetTire = nullptr;
	}

	++CurrentMountIndex;
	if (CurrentMountIndex < MountCount)
	{
		FTimerHandle NextMountTimerHandle;
		GetWorldTimerManager().SetTimer(
			NextMountTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				StartSingleMount();
			}),
			FMath::Max(0.0f, BetweenMountDelay),
			false);
		return;
	}

	FinishAllMounts();
}

void AGenesisRobotTirePickerToCarActor::FinishAllMounts()
{
	TargetTire = nullptr;
	bIsBusy = false;
}

bool AGenesisRobotTirePickerToCarActor::CallTakeTireFromTable(AActor*& TakenTire) const
{
	TakenTire = nullptr;

	if (!IsValid(TableBufferRef))
	{
		return false;
	}

	if (AGenesisTireTableBufferActor* NativeBuffer = Cast<AGenesisTireTableBufferActor>(TableBufferRef))
	{
		return NativeBuffer->TakeTireFromTable(TakenTire);
	}

	if (UFunction* Function = TableBufferRef->FindFunction(TEXT("TakeTireFromTable")))
	{
		struct FTakeTireFromTableParams
		{
			AActor* TakenTire = nullptr;
			bool Success = false;
		};

		FTakeTireFromTableParams Params;
		TableBufferRef->ProcessEvent(Function, &Params);
		TakenTire = Params.TakenTire;
		return Params.Success;
	}

	return false;
}

void AGenesisRobotTirePickerToCarActor::SetBoolProperty(AActor* Target, const FName PropertyName, const bool bValue) const
{
	if (!IsValid(Target))
	{
		return;
	}

	if (FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(Target->GetClass(), PropertyName))
	{
		BoolProperty->SetPropertyValue_InContainer(Target, bValue);
	}
}
