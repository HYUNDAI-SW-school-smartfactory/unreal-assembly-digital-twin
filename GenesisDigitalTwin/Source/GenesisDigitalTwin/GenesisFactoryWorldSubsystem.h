#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GenesisFactoryWorldSubsystem.generated.h"

UCLASS()
class GENESISDIGITALTWIN_API UGenesisFactoryWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
