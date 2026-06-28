#include "GenesisRobotTirePickerActor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GenesisTireTableBufferActor.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

AGenesisRobotTirePickerActor::AGenesisRobotTirePickerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(Root);

	CR_Component = CreateDefaultSubobject<USceneComponent>(TEXT("CR_Component"));
	CR_Component->SetupAttachment(Root);

	IK_Target = CreateDefaultSubobject<USceneComponent>(TEXT("IK_Target"));
	IK_Target->SetupAttachment(Root);

	MovingArmRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MovingArmRoot"));
	MovingArmRoot->SetupAttachment(Root);

	Pole_Target = CreateDefaultSubobject<USceneComponent>(TEXT("Pole_Target"));
	Pole_Target->SetupAttachment(Root);

	Robot = CreateDefaultSubobject<USceneComponent>(TEXT("Robot"));
	Robot->SetupAttachment(Root);

	RobotMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RobotMesh"));
	RobotMesh->SetupAttachment(Robot);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RobotMeshFinder(TEXT("/Game/Meshes/Robot/SK_RoboArm04.SK_RoboArm04"));
	if (RobotMeshFinder.Succeeded())
	{
		RobotMesh->SetSkeletalMesh(RobotMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> PickAnimFinder(TEXT("/Game/Meshes/Robot/Anim_Robot_ConveyorToTable_Base.Anim_Robot_ConveyorToTable_Base"));
	if (PickAnimFinder.Succeeded())
	{
		PickPlaceAnim = PickAnimFinder.Object;
	}
}

void AGenesisRobotTirePickerActor::BeginPlay()
{
	Super::BeginPlay();

	if (RobotMesh)
	{
		IK_Target->SetWorldTransform(RobotMesh->GetSocketTransform(TireGripSocketName, RTS_World));
	}
}

bool AGenesisRobotTirePickerActor::StartPickAndPlace()
{
	if (bIsBusy)
	{
		return false;
	}

	if (!ResolveTargetTire() || !GetBoolProperty(TargetTire, TEXT("CanBePicked"), false))
	{
		return false;
	}

	bIsBusy = true;
	SetBoolProperty(TargetTire, TEXT("CanBePicked"), false);

	if (RobotMesh && PickPlaceAnim)
	{
		RobotMesh->PlayAnimation(PickPlaceAnim, false);
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

	FTimerHandle DropTimerHandle;
	GetWorldTimerManager().SetTimer(
		DropTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			DropTireToTable();
		}),
		FMath::Max(AttachTime, DetachTime),
		false);

	FTimerHandle EndTimerHandle;
	GetWorldTimerManager().SetTimer(
		EndTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (bIsBusy)
			{
				FinishPickAndPlace(false);
			}
		}),
		FMath::Max(FMath::Max(AttachTime, DetachTime), AnimEndTime),
		false);

	return true;
}

void AGenesisRobotTirePickerActor::ResetRobot()
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	bIsBusy = false;
	TargetTire = nullptr;
}

void AGenesisRobotTirePickerActor::AttachTireToRobot()
{
	if (!IsValid(TargetTire) || !RobotMesh)
	{
		FinishPickAndPlace(false);
		return;
	}

	TargetTire->AttachToComponent(
		RobotMesh,
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true),
		TireGripSocketName);
}

void AGenesisRobotTirePickerActor::DropTireToTable()
{
	if (!IsValid(TargetTire))
	{
		FinishPickAndPlace(false);
		return;
	}

	const bool bAdded = CallAddTireToTable(TargetTire);
	if (bAdded)
	{
		CallNoArgFunction(ConveyorRef, TEXT("ReleaseCurrentTire"));
		CallNoArgFunction(TableBufferRef, TEXT("RequestNextTire"));
		FinishPickAndPlace(true);
		return;
	}

	FinishPickAndPlace(false);
}

void AGenesisRobotTirePickerActor::FinishPickAndPlace(const bool bSuccess)
{
	if (!bSuccess && IsValid(TargetTire))
	{
		SetBoolProperty(TargetTire, TEXT("CanBePicked"), true);
	}

	if (bSuccess)
	{
		TargetTire = nullptr;
	}

	bIsBusy = false;
}

bool AGenesisRobotTirePickerActor::ResolveTargetTire()
{
	if (IsValid(TargetTire))
	{
		return true;
	}

	TargetTire = GetActorProperty(ConveyorRef, TEXT("TargetTire"));
	return IsValid(TargetTire);
}

bool AGenesisRobotTirePickerActor::CallAddTireToTable(AActor* Tire) const
{
	if (!IsValid(TableBufferRef) || !IsValid(Tire))
	{
		return false;
	}

	if (AGenesisTireTableBufferActor* NativeBuffer = Cast<AGenesisTireTableBufferActor>(TableBufferRef))
	{
		return NativeBuffer->AddTireToTable(Tire);
	}

	if (UFunction* Function = TableBufferRef->FindFunction(TEXT("AddTireToTable")))
	{
		struct FAddTireToTableParams
		{
			AActor* IncomingTire = nullptr;
			bool Success = false;
		};

		FAddTireToTableParams Params;
		Params.IncomingTire = Tire;
		TableBufferRef->ProcessEvent(Function, &Params);
		return Params.Success;
	}

	return false;
}

void AGenesisRobotTirePickerActor::CallNoArgFunction(AActor* Target, const FName FunctionName) const
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

AActor* AGenesisRobotTirePickerActor::GetActorProperty(AActor* Target, const FName PropertyName) const
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

bool AGenesisRobotTirePickerActor::GetBoolProperty(AActor* Target, const FName PropertyName, const bool DefaultValue) const
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

void AGenesisRobotTirePickerActor::SetBoolProperty(AActor* Target, const FName PropertyName, const bool bValue) const
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
