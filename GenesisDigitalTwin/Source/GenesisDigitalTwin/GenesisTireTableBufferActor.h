#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenesisTireTableBufferActor.generated.h"

class USceneComponent;

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisTireTableBufferActor : public AActor
{
	GENERATED_BODY()

public:
	AGenesisTireTableBufferActor();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Table Buffer")
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Table Buffer|Points")
	TObjectPtr<USceneComponent> Point1 = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Table Buffer|Points")
	TObjectPtr<USceneComponent> Point2 = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Table Buffer|Points")
	TObjectPtr<USceneComponent> Point3 = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Table Buffer|Points")
	TObjectPtr<USceneComponent> Point4 = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tire Table Buffer|Points")
	TObjectPtr<USceneComponent> Point5 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Table Buffer")
	TSubclassOf<AActor> TireClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Table Buffer")
	TArray<TObjectPtr<AActor>> StoredTires;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Table Buffer")
	TArray<TObjectPtr<USceneComponent>> StackPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Table Buffer", meta = (ClampMin = "1"))
	int32 MaxCapacity = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tire Table Buffer")
	TObjectPtr<AActor> ConveyorRef = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Tire Table Buffer")
	bool AddTireToTable(AActor* IncomingTire);

	UFUNCTION(BlueprintCallable, Category = "Tire Table Buffer")
	bool TakeTireFromTable(AActor*& TakenTire);

	UFUNCTION(BlueprintPure, Category = "Tire Table Buffer")
	int32 GetStoredCount() const;

	UFUNCTION(BlueprintCallable, Category = "Tire Table Buffer")
	void RequestNextTire();

	UFUNCTION(BlueprintCallable, Category = "Tire Table Buffer")
	void ClearBuffer(bool bDestroyStoredTires = false);

private:
	void RebuildStackPoints();
	void SetTireBoolProperty(AActor* Tire, FName PropertyName, bool bValue) const;
	void CallNoArgFunction(AActor* Target, FName FunctionName) const;
};
