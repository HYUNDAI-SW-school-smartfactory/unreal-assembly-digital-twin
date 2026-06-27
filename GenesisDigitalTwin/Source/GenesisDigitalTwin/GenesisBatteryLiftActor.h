#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenesisBatteryLiftActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisBatteryLiftActor : public AActor
{
	GENERATED_BODY()

public:
	AGenesisBatteryLiftActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battery Lift")
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battery Lift")
	TObjectPtr<USceneComponent> LiftRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battery Lift")
	TObjectPtr<UStaticMeshComponent> BatteryPack = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battery Lift")
	TObjectPtr<UStaticMeshComponent> Cube = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery Lift")
	FVector LiftStartLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery Lift")
	FVector LiftEndLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery Lift", meta = (ClampMin = "0.0"))
	float LiftHeight = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery Lift", meta = (ClampMin = "0.01"))
	float LiftDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery Lift", meta = (ClampMin = "0.0"))
	float PostInstallHideDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery Lift")
	bool bHideBatteryPackOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery Lift")
	bool bUseSmoothStepAlpha = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battery Lift")
	bool bIsInstalling = false;

	UFUNCTION(BlueprintCallable, Category = "Battery Lift")
	void StartBatteryInstall();

	UFUNCTION(BlueprintCallable, Category = "Battery Lift")
	void StartLift();

	UFUNCTION(BlueprintCallable, Category = "Battery Lift")
	void PlayLift();

	UFUNCTION(BlueprintCallable, Category = "Battery Lift")
	void ResetLift();

	UFUNCTION(BlueprintPure, Category = "Battery Lift")
	bool IsInstalling() const { return bIsInstalling; }

private:
	bool bLiftPlaying = false;
	bool bReturningToStart = false;
	float LiftElapsed = 0.0f;
	FTimerHandle HideBatteryTimerHandle;

	void FinishLift();
	void StartReturnLift();
	void FinishReturnLift();
	void HideBatteryPackAfterInstall();
};
