#include "GenesisFactoryStatusBoard.h"

#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "GenesisFactoryStatusWidget.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FVector2D StatusBoardDrawSize(1280.0f, 720.0f);
	const FRotator MonitorFaceRotation(-15.0f, 180.0f, 0.0f);
	const FVector MonitorWidgetScale(0.15f, 0.15f, 0.16f);

	void ApplyStationMonitorWidgetLayout(UWidgetComponent* Widget)
	{
		if (!Widget)
		{
			return;
		}

		// Match the final line-board / BP_MQTT_DisplayBoard widget transform.
		// MainMaps station boards are scaled to the same display-board proportions
		// as the line boards, so a single widget layout stays seated in the mesh.
		Widget->SetRelativeLocation(FVector(-51.0f, 0.0f, -174.0f));
		Widget->SetRelativeRotation(MonitorFaceRotation);
		Widget->SetRelativeScale3D(MonitorWidgetScale);
		Widget->SetDrawSize(StatusBoardDrawSize);
	}

	void ApplyLineMonitorWidgetLayout(UWidgetComponent* Widget)
	{
		if (!Widget)
		{
			return;
		}

		// Keep the existing final line-board layout; those boards are already
		// correctly seated on the black entrance panels.
		Widget->SetRelativeLocation(FVector(-51.0f, 0.0f, -174.0f));
		Widget->SetRelativeRotation(MonitorFaceRotation);
		Widget->SetRelativeScale3D(MonitorWidgetScale);
		Widget->SetDrawSize(StatusBoardDrawSize);
	}
}

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
	MonitorWidget->SetDrawSize(StatusBoardDrawSize);
	MonitorWidget->SetTwoSided(true);
	MonitorWidget->SetPivot(FVector2D(0.5f, 0.5f));
	ApplyStationMonitorWidgetLayout(MonitorWidget);
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
	bool bIsBottleneck,
	bool bIsIdle)
{
	(void)LineId;
	(void)BufferCount;
	const FString AlertLine = bIsIdle
		? TEXT("\nIDLE")
		: (bIsBottleneck ? TEXT("\nBOTTLENECK") : TEXT(""));
	UpdateScreen(
		FString::Printf(
			TEXT("%s\nUTILIZATION %.1f%%\nDEFECT RATE %.4f%%\nCYCLE TIME %.2fs%s"),
			*StationName,
			Utilization * 100.0f,
			DefectRate * 100.0f,
			CycleTime,
			*AlertLine),
		(bIsBottleneck || bIsIdle) ? FLinearColor::Red : FLinearColor(0.05f, 1.0f, 0.15f));
}

void AGenesisFactoryStatusBoard::SetLineStatus(int32 LineId, int32 TotalProduced, const FString& BottleneckName)
{
	SetLineStatusDetailed(LineId, TotalProduced, BottleneckName, BottleneckName.IsEmpty() ? 0 : 2, FString(), false);
}

void AGenesisFactoryStatusBoard::SetLineStatusDetailed(
	int32 LineId,
	int32 TotalProduced,
	const FString& BottleneckName,
	int32 BottleneckBufferCount,
	const FString& ReasonOverride,
	bool bForceAlert)
{
	const bool bHasBottleneck = !BottleneckName.IsEmpty();
	const bool bHasReasonOverride = !ReasonOverride.IsEmpty();
	UpdateScreen(
		FString::Printf(
			TEXT("LINE %d\nPRODUCTION: %d\nBOTTLENECK: %s\nREASON: %s"),
			LineId,
			TotalProduced,
			bHasBottleneck ? *BottleneckName : TEXT("NONE"),
			bHasReasonOverride
				? *ReasonOverride
				: (bHasBottleneck
				? *FString::Printf(TEXT("BUFFER FULL %d/2"), BottleneckBufferCount)
				: TEXT("NORMAL FLOW"))),
		(bHasBottleneck || bForceAlert)
			? FLinearColor(1.0f, 0.45f, 0.02f)
			: FLinearColor(0.05f, 1.0f, 0.15f));
}

void AGenesisFactoryStatusBoard::ConfigureAsStationBoard()
{
	EnsureMonitorWidgetReady();
	ApplyStationMonitorWidgetLayout(MonitorWidget);
}

void AGenesisFactoryStatusBoard::ConfigureAsLineBoard()
{
	EnsureMonitorWidgetReady();
	ApplyLineMonitorWidgetLayout(MonitorWidget);
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
	MonitorWidget->SetDrawSize(StatusBoardDrawSize);
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
