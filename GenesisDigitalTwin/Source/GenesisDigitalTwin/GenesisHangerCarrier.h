#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenesisHangerCarrier.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class GENESISDIGITALTWIN_API AGenesisHangerCarrier : public AActor
{
	GENERATED_BODY()

public:
	AGenesisHangerCarrier();

	USceneComponent* GetCarAnchor() const { return CarAnchor; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Hanger")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Hanger")
	TObjectPtr<UStaticMeshComponent> Trolley;

	UPROPERTY(VisibleAnywhere, Category = "Hanger")
	TObjectPtr<UStaticMeshComponent> VerticalSupport;

	UPROPERTY(VisibleAnywhere, Category = "Hanger")
	TObjectPtr<UStaticMeshComponent> ClutchMesh;

	UPROPERTY(VisibleAnywhere, Category = "Hanger")
	TObjectPtr<USceneComponent> CarAnchor;
};
