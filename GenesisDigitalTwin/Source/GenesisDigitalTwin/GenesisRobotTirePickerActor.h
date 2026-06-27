#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenesisRobotTirePickerActor.generated.h"

class UAnimationAsset;
class USceneComponent;
class USkeletalMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisRobotTirePickerActor : public AActor
{
	GENERATED_BODY()

public:
	AGenesisRobotTirePickerActor();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot Tire Picker")
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot Tire Picker|IK")
	TObjectPtr<USceneComponent> CR_Component = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot Tire Picker|IK")
	TObjectPtr<USceneComponent> IK_Target = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot Tire Picker|IK")
	TObjectPtr<USceneComponent> MovingArmRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot Tire Picker|IK")
	TObjectPtr<USceneComponent> Pole_Target = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot Tire Picker|Mesh")
	TObjectPtr<USceneComponent> Robot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot Tire Picker|Mesh")
	TObjectPtr<USkeletalMeshComponent> RobotMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker")
	TObjectPtr<AActor> TargetTire = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker")
	bool bIsBusy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker", meta = (ClampMin = "0.0"))
	float MoveDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker")
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker")
	FVector EndLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker")
	float LiftHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker")
	FVector LiftedSourceLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker")
	FVector LiftedDestLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker|Animation")
	TObjectPtr<UAnimationAsset> PickPlaceAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker|Animation", meta = (ClampMin = "0.0"))
	float AttachTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker|Animation", meta = (ClampMin = "0.0"))
	float DetachTime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker|Animation", meta = (ClampMin = "0.0"))
	float AnimEndTime = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker")
	TObjectPtr<AActor> TableBufferRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker")
	TObjectPtr<AActor> ConveyorRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker|Points")
	TObjectPtr<USceneComponent> SourcePointRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker|Points")
	TObjectPtr<USceneComponent> DestPointRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot Tire Picker|Socket")
	FName TireGripSocketName = TEXT("TireGripSocket");

	UFUNCTION(BlueprintCallable, Category = "Robot Tire Picker")
	bool StartPickAndPlace();

	UFUNCTION(BlueprintCallable, Category = "Robot Tire Picker")
	void ResetRobot();

protected:
	void AttachTireToRobot();
	void DropTireToTable();
	void FinishPickAndPlace(bool bSuccess);

	bool ResolveTargetTire();
	bool CallAddTireToTable(AActor* Tire) const;
	void CallNoArgFunction(AActor* Target, FName FunctionName) const;
	AActor* GetActorProperty(AActor* Target, FName PropertyName) const;
	bool GetBoolProperty(AActor* Target, FName PropertyName, bool DefaultValue = false) const;
	void SetBoolProperty(AActor* Target, FName PropertyName, bool bValue) const;
};
