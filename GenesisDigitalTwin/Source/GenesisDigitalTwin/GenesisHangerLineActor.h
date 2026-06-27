#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenesisHangerLineActor.generated.h"

class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;

UCLASS(BlueprintType)
class GENESISDIGITALTWIN_API AGenesisHangerLineActor : public AActor
{
	GENERATED_BODY()

public:
	AGenesisHangerLineActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanger Line|Config")
	int32 LineId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanger Line|Config")
	float CenterY = 1620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanger Line|Config")
	float RailHeight = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanger Line|Config")
	TArray<float> ForwardStationX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanger Line|Config")
	float UnloadPositionX = 15650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanger Line|Config")
	float EmptyReturnOffset = 1680.0f;

	void ConfigureLine(
		float InCenterY,
		float InRailHeight,
		const TArray<float>& ForwardX,
		float InUnloadX,
		float InReturnOffset);

	USplineComponent* GetLineSpline() const { return LineSpline; }
	float GetConfiguredCenterY() const { return ConfiguredCenterY; }
	float GetForwardDistanceAtX(float X) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Hanger Line")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Hanger Line")
	TObjectPtr<USplineComponent> LineSpline;

	UPROPERTY()
	TArray<TObjectPtr<USplineMeshComponent>> RailMeshes;

	UPROPERTY()
	TObjectPtr<UStaticMesh> RailMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> RailMaterial;

	float ConfiguredCenterY = 0.0f;

	void RebuildRailMeshes();
	void AddRailSegment(int32 StartIndex, int32 EndIndex, float LateralOffset);
};
