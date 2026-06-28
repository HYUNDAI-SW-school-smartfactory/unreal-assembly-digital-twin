#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenesisTireAssemblyCellActor.generated.h"

class UChildActorComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EGenesisTireAssemblyCellSide : uint8
{
	Right,
	Left
};

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisTireAssemblyCellActor : public AActor
{
	GENERATED_BODY()

public:
	AGenesisTireAssemblyCellActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell")
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell|Children")
	TObjectPtr<UChildActorComponent> Robot_TableToCar = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell|Children")
	TObjectPtr<UChildActorComponent> Robot_ConveyorToTable = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell|Children")
	TObjectPtr<UChildActorComponent> ConveyorActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell|Children")
	TObjectPtr<UChildActorComponent> TireActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell|Children")
	TObjectPtr<UChildActorComponent> TireTableBuffer = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell|Points")
	TObjectPtr<USceneComponent> P_CarMount = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell|Points")
	TObjectPtr<USceneComponent> P_ConveyorPickup = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell|Points")
	TObjectPtr<USceneComponent> P_TableDrop = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell|Points")
	TObjectPtr<USceneComponent> P_TablePickup = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Assembly Cell|Visual")
	TObjectPtr<UStaticMeshComponent> TableMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Layout")
	EGenesisTireAssemblyCellSide CellSide = EGenesisTireAssemblyCellSide::Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Layout")
	bool bApplyDefaultSideLayout = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Refs")
	TObjectPtr<AActor> TireRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Refs")
	TObjectPtr<AActor> ConveyorRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Refs")
	TObjectPtr<AActor> RobotToTableRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Refs")
	TObjectPtr<AActor> RobotToCarRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Refs")
	TObjectPtr<AActor> TireTableBufferRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Flow")
	bool bHasStartedRobotToTable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Flow")
	bool bHasStartedRobotToCar = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Flow")
	bool bAutoSpawnFirstTire = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Flow", meta = (ClampMin = "0"))
	int32 BufferCountToStartMount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Flow")
	bool bVehicleWaitingForTires = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Assembly Cell|Flow", meta = (ClampMin = "0.0"))
	float VehicleMountStartDelay = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Tire Assembly Cell")
	bool SpawnNextTire();

	UFUNCTION(BlueprintCallable, Category = "Tire Assembly Cell")
	void StartPickAndPlace();

	UFUNCTION(BlueprintCallable, Category = "Tire Assembly Cell")
	void StartVehicleTireMount();

	UFUNCTION(BlueprintCallable, Category = "Tire Assembly Cell")
	void StartMountTire();

	UFUNCTION(BlueprintCallable, Category = "Tire Assembly Cell")
	void ResetCell();

private:
	void ApplyDefaultSideLayout();
	void BindChildActors();
	void ConfigureChildReferences();
	void TickRobotDispatch();

	AActor* GetChildActor(const UChildActorComponent* ChildActorComponent) const;
	bool CallBoolFunction(AActor* Target, FName FunctionName) const;
	void CallNoArgFunction(AActor* Target, FName FunctionName) const;
	int32 CallIntFunction(AActor* Target, FName FunctionName, int32 DefaultValue = 0) const;
	bool GetBoolProperty(AActor* Target, FName PropertyName, bool DefaultValue = false) const;
	AActor* GetActorProperty(AActor* Target, FName PropertyName) const;
	void SetActorProperty(AActor* Target, FName PropertyName, AActor* Value) const;
	void SetSceneComponentProperty(AActor* Target, FName PropertyName, USceneComponent* Value) const;

	double VehicleMountRequestTime = -1.0;
};
