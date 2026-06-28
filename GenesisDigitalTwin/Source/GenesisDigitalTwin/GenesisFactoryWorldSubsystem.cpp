#include "GenesisFactoryWorldSubsystem.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GenesisFactoryAssemblyManager.h"

namespace
{
	bool IsGenesisAutoEquipment(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}

		const FString ClassPath = Actor->GetClass()->GetPathName();
		return ClassPath.Contains(TEXT("BP_AGV_C")) ||
			ClassPath.Contains(TEXT("BP_BatteryLift_C")) ||
			ClassPath.Contains(TEXT("BP_TireAssemblyCell_C")) ||
			ClassPath.Contains(TEXT("BP_TireAssemblyCell_L_C")) ||
			ClassPath.Contains(TEXT("BP_AGV_Controlled_C")) ||
			ClassPath.Contains(TEXT("BP_BatteryLift_Controlled_C")) ||
			ClassPath.Contains(TEXT("BP_TireAssemblyCell_Controlled_C")) ||
			ClassPath.Contains(TEXT("BP_TireAssemblyCell_L_Controlled_C"));
	}

	void PauseTopLevelEquipmentActor(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		Actor->SetActorHiddenInGame(false);
		Actor->SetActorEnableCollision(true);
		Actor->SetActorTickEnabled(false);
		Actor->CustomTimeDilation = 0.0f;
	}
}

void UGenesisFactoryWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const FString RuntimeMapName = InWorld.GetMapName();
	if (!RuntimeMapName.EndsWith(TEXT("MainMaps"), ESearchCase::CaseSensitive))
	{
		return;
	}

	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
		GEngine->ClearOnScreenDebugMessages();
	}

	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		if (IsGenesisAutoEquipment(*It))
		{
			PauseTopLevelEquipmentActor(*It);
		}
	}

	for (TActorIterator<AGenesisFactoryAssemblyManager> It(&InWorld); It; ++It)
	{
		return;
	}

	UClass* ManagerClass = LoadClass<AGenesisFactoryAssemblyManager>(
		nullptr,
		TEXT("/Game/Blueprints/BP_GenesisFactoryAssemblyManager.BP_GenesisFactoryAssemblyManager_C"));
	if (!ManagerClass)
	{
		ManagerClass = AGenesisFactoryAssemblyManager::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("GenesisFactoryAssemblyManager");
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGenesisFactoryAssemblyManager* Manager = InWorld.SpawnActor<AGenesisFactoryAssemblyManager>(
		ManagerClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("GenesisFactory: Auto-spawned manager %s in %s."),
		Manager ? *Manager->GetName() : TEXT("<failed>"),
		*InWorld.GetMapName());
}
