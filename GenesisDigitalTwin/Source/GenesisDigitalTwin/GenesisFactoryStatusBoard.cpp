#include "GenesisFactoryStatusBoard.h"

#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "GenesisFactoryStatusWidget.h"
#include "UObject/ConstructorHelpers.h"

AGenesisFactoryStatusBoard::AGenesisFactoryStatusBoard()
{
	PrimaryActorTick.bCanEverTick = false;

	// Keep the original default subobject names so already-placed MainMaps actors
	// keep their serialized component instances after C++ rebuilds.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MonitorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
	MonitorMesh->SetupAttachment(Root);
	MonitorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoardFinder(TEXT("/Game/Meshes/SM_DisplayBoard03.SM_DisplayBoard03"));
	if (BoardFinder.Succeeded())
	{
		MonitorMesh->SetStaticMesh(BoardFinder.Object);
	}

	MonitorWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("MonitorWidget"));
	MonitorWidget->SetupAttachment(Root);
	MonitorWidget->SetWidgetSpace(EWidgetSpace::World);
	MonitorWidget->SetWidgetClass(UGenesisFactoryStatusWidget::StaticClass());
	MonitorWidget->SetDrawSize(FVector2D(1280.0f, 720.0f));
	MonitorWidget->SetTwoSided(true);
	MonitorWidget->SetPivot(FVector2D(0.5f, 0.5f));
	MonitorWidget->SetRelativeLocation(FVector(-51.0f, 0.0f, -174.0f));
	MonitorWidget->SetRelativeRotation(FRotator(-15.0f, 180.0f, 0.0f));
	MonitorWidget->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.16f));
}

void AGenesisFactoryStatusBoard::BeginPlay()
{
	Super::BeginPlay();
	EnsureMonitorWidgetReady();
	UpdateScreen(TEXT("WAITING FOR FACTORY DATA"), FLinearColor(0.05f, 1.0f, 0.15f));
}

void AGenesisFactoryStatusBoard::SetStationStatus(
	int32 LineId,
	const FString& StationName,
	float Utilization,
	float DefectRate,
	float CycleTime,
	int32 BufferCount,
	bool bIsBottleneck)
{
	(void)LineId;
	(void)BufferCount;
	const FString Bottleneck = bIsBottleneck ? TEXT("\nBOTTLENECK") : TEXT("");
	UpdateScreen(
		FString::Printf(
			TEXT("%s\nUTILIZATION %.1f%%\nDEFECT RATE %.4f%%\nCYCLE TIME %.2fs%s"),
			*StationName,
			Utilization * 100.0f,
			DefectRate * 100.0f,
			CycleTime,
			*Bottleneck),
		bIsBottleneck ? FLinearColor::Red : FLinearColor(0.05f, 1.0f, 0.15f));
}

void AGenesisFactoryStatusBoard::SetLineStatus(int32 LineId, int32 TotalProduced, const FString& BottleneckName)
{
	SetLineStatusDetailed(LineId, TotalProduced, BottleneckName, BottleneckName.IsEmpty() ? 0 : 2);
}

void AGenesisFactoryStatusBoard::SetLineStatusDetailed(
	int32 LineId,
	int32 TotalProduced,
	const FString& BottleneckName,
	int32 BottleneckBufferCount)
{
	const bool bHasBottleneck = !BottleneckName.IsEmpty();
	UpdateScreen(
		FString::Printf(
			TEXT("LINE %d\nPRODUCTION: %d\nBOTTLENECK: %s\nREASON: %s"),
			LineId,
			TotalProduced,
			bHasBottleneck ? *BottleneckName : TEXT("NONE"),
			bHasBottleneck
				? *FString::Printf(TEXT("BUFFER FULL %d/2"), BottleneckBufferCount)
				: TEXT("NORMAL FLOW")),
		bHasBottleneck
			? FLinearColor(1.0f, 0.45f, 0.02f)
			: FLinearColor(0.05f, 1.0f, 0.15f));
}

void AGenesisFactoryStatusBoard::ConfigureAsStationBoard()
{
	EnsureMonitorWidgetReady();
	if (MonitorWidget)
	{
		MonitorWidget->SetDrawSize(FVector2D(1280.0f, 720.0f));
		MonitorWidget->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.16f));
	}
}

void AGenesisFactoryStatusBoard::ConfigureAsLineBoard()
{
	EnsureMonitorWidgetReady();
	if (MonitorWidget)
	{
		MonitorWidget->SetDrawSize(FVector2D(1280.0f, 720.0f));
		MonitorWidget->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.16f));
	}
}

void AGenesisFactoryStatusBoard::EnsureMonitorWidgetReady()
{
	if (!MonitorWidget)
	{
		return;
	}

	MonitorWidget->SetHiddenInGame(false);
	MonitorWidget->SetVisibility(true, true);
	MonitorWidget->SetWidgetSpace(EWidgetSpace::World);
	MonitorWidget->SetDrawSize(FVector2D(1280.0f, 720.0f));
	MonitorWidget->SetTwoSided(true);
	MonitorWidget->SetPivot(FVector2D(0.5f, 0.5f));

	const TSubclassOf<UUserWidget> CurrentWidgetClass = MonitorWidget->GetWidgetClass();
	if (!CurrentWidgetClass || !CurrentWidgetClass->IsChildOf(UGenesisFactoryStatusWidget::StaticClass()))
	{
		MonitorWidget->SetWidgetClass(UGenesisFactoryStatusWidget::StaticClass());
		StatusWidget = nullptr;
	}
}

void AGenesisFactoryStatusBoard::UpdateScreen(const FString& Value, const FLinearColor& Color)
{
	EnsureMonitorWidgetReady();
	if (!StatusWidget && MonitorWidget)
	{
		MonitorWidget->InitWidget();
		StatusWidget = Cast<UGenesisFactoryStatusWidget>(MonitorWidget->GetUserWidgetObject());
	}
	if (StatusWidget)
	{
		StatusWidget->SetStatusText(Value, Color);
	}
}
