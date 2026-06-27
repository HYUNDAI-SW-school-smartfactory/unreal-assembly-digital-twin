#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenesisAGVActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisAGVActor : public AActor
{
	GENERATED_BODY()

public:
	AGenesisAGVActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AGV")
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AGV")
	TObjectPtr<UStaticMeshComponent> AGV_Mesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AGV")
	TObjectPtr<UStaticMeshComponent> BatteryPack_Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Movement")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Movement", meta = (ClampMin = "1.0"))
	float MoveSpeed = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Movement", meta = (ClampMin = "1.0"))
	float AcceptanceRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Movement")
	TArray<FVector> Waypoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Movement")
	int32 CurrentWaypointIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Movement")
	bool bIsMoving = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Movement")
	bool GoingForward = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Movement")
	bool bReturnAfterDelivery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Battery")
	bool bHideBatteryPackOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Battery")
	bool bShowBatteryPackWhenStarted = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Battery")
	bool bHideBatteryPackAtFinalWaypoint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGV|Battery")
	TObjectPtr<AActor> BatteryLiftRef = nullptr;

	UFUNCTION(BlueprintCallable, Category = "AGV")
	void StartBatteryInstall();

	UFUNCTION(BlueprintCallable, Category = "AGV")
	void StartAGV();

	UFUNCTION(BlueprintCallable, Category = "AGV")
	void StopAGV();

	UFUNCTION(BlueprintCallable, Category = "AGV")
	void ResetAGV();

private:
	FVector InitialLocation = FVector::ZeroVector;
	FRotator InitialRotation = FRotator::ZeroRotator;

	void MoveTowardWaypoint(float DeltaSeconds);
	void HandleWaypointArrival();
	void AdvanceForward();
	void AdvanceBackward();
	void DeliverBatteryToLift();
	void SetBatteryPackVisible(bool bVisible);
	void CallBatteryLiftStart() const;
};
