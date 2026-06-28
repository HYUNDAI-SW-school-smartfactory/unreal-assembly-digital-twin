#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenesisRobotTirePickerToCarActor.generated.h"

class UAnimationAsset;
class USceneComponent;
class USkeletalMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisRobotTirePickerToCarActor : public AActor
{
	GENERATED_BODY()

public:
	AGenesisRobotTirePickerToCarActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot Tire Mount")
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot Tire Mount")
	TObjectPtr<USceneComponent> GripperPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot Tire Mount")
	TObjectPtr<USkeletalMeshComponent> Robot = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount")
	TObjectPtr<AActor> TargetTire = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount")
	bool bIsBusy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount", meta = (ClampMin = "0.0"))
	float MoveDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount")
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount")
	FVector EndLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Animation")
	TObjectPtr<UAnimationAsset> PickPlaceAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Animation", meta = (ClampMin = "0.0"))
	float AttachTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Animation", meta = (ClampMin = "0.0"))
	float DetachTime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Animation", meta = (ClampMin = "0.0"))
	float MountDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Animation", meta = (ClampMin = "0.0"))
	float AnimEndTime = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount")
	TObjectPtr<AActor> TableBufferRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount")
	int32 CurrentMountIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount", meta = (ClampMin = "1"))
	int32 MountCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Animation")
	TObjectPtr<UAnimationAsset> MountAnimation1 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Animation")
	TObjectPtr<UAnimationAsset> MountAnimation2 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Animation", meta = (ClampMin = "0.0"))
	float BetweenMountDelay = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Points")
	TObjectPtr<USceneComponent> SourcePointRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Points")
	TObjectPtr<USceneComponent> DestPointRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Socket")
	FName GripSocketName = TEXT("GripSocket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Mount|Timing", meta = (ClampMin = "0.0"))
	float TakeFromTableDelay = 2.0f;

	UFUNCTION(BlueprintCallable, Category = "Robot Tire Mount")
	bool StartMountTire();

	UFUNCTION(BlueprintCallable, Category = "Robot Tire Mount")
	void StartSingleMount();

	UFUNCTION(BlueprintCallable, Category = "Robot Tire Mount")
	void ResetRobot();

protected:
	void TakeTireAndPlayMount();
	void AttachTireToRobot();
	void FinishSingleMount();
	void FinishAllMounts();

	bool CallTakeTireFromTable(AActor*& TakenTire) const;
	void SetBoolProperty(AActor* Target, FName PropertyName, bool bValue) const;
};
