#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenesisTireConveyorActor.generated.h"

class USceneComponent;
class USplineComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EGenesisTireConveyorTireState : uint8
{
	Moving,
	ReadyForPickup,
	Picked,
	Mounted
};

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisTireConveyorActor : public AActor
{
	GENERATED_BODY()

public:
	AGenesisTireConveyorActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Conveyor")
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Conveyor")
	TObjectPtr<USceneComponent> ConveyorRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Conveyor")
	TObjectPtr<USceneComponent> SpawnPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Conveyor")
	TObjectPtr<USceneComponent> PickupPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Conveyor")
	TObjectPtr<USplineComponent> Spline = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Visual")
	TObjectPtr<UStaticMeshComponent> ConveyorPreviewMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Visual")
	TObjectPtr<UStaticMeshComponent> SideBoxLeft = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Visual")
	TObjectPtr<UStaticMeshComponent> SideBoxRight = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Visual")
	TArray<TObjectPtr<UStaticMeshComponent>> RollerMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Tire")
	TSubclassOf<AActor> TireClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Tire")
	TObjectPtr<AActor> TargetTire = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Movement", meta = (ClampMin = "0.0"))
	float DistanceAlongSpline = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Movement")
	bool bIsMoving = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Movement")
	bool bHasActiveTire = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Movement")
	bool bAutoMoveSpawnedTire = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Conveyor|Tire")
	EGenesisTireConveyorTireState TireState = EGenesisTireConveyorTireState::ReadyForPickup;

	UFUNCTION(BlueprintCallable, Category = "Tire Conveyor")
	bool SpawnNextTire();

	UFUNCTION(BlueprintCallable, Category = "Tire Conveyor")
	void StartConveyor();

	UFUNCTION(BlueprintCallable, Category = "Tire Conveyor")
	void StopConveyor();

	UFUNCTION(BlueprintCallable, Category = "Tire Conveyor")
	void ReleaseCurrentTire();

	UFUNCTION(BlueprintCallable, Category = "Tire Conveyor")
	void ResetConveyor();

private:
	FVector InitialLocation = FVector::ZeroVector;

	void UpdateTireLocation();
	void MarkTireReadyForPickup();
	void SetTireBoolProperty(AActor* Tire, const FName PropertyName, bool bValue) const;
	void SetTireStateProperty(AActor* Tire, EGenesisTireConveyorTireState NewState) const;
};
