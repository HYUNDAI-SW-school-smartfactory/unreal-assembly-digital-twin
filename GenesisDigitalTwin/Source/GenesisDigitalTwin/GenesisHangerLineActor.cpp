#include "GenesisHangerLineActor.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGenesisHangerLineActor::AGenesisHangerLineActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	LineSpline = CreateDefaultSubobject<USplineComponent>(TEXT("HangerSpline"));
	LineSpline->SetupAttachment(Root);
	LineSpline->SetClosedLoop(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamFinder(TEXT("/Game/Meshes/SM_MetalBeam20.SM_MetalBeam20"));
	if (BeamFinder.Succeeded())
	{
		RailMesh = BeamFinder.Object;
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> BlueprintBeamFinder(TEXT("/Game/Blueprints/SM_MetalBeam20.SM_MetalBeam20"));
		if (BlueprintBeamFinder.Succeeded())
		{
			RailMesh = BlueprintBeamFinder.Object;
		}
		else
		{
			static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (CubeFinder.Succeeded())
			{
				RailMesh = CubeFinder.Object;
			}
		}
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/Materials/MI_MetalBeam_Painted01.MI_MetalBeam_Painted01"));
	if (MaterialFinder.Succeeded())
	{
		RailMaterial = MaterialFinder.Object;
	}

	ForwardStationX = {1620.0f, 4800.0f, 8020.0f, 11420.0f, 14640.0f};
	ConfiguredCenterY = CenterY;
}

void AGenesisHangerLineActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (ForwardStationX.Num() == 0)
	{
		ForwardStationX = {1620.0f, 4800.0f, 8020.0f, 11420.0f, 14640.0f};
	}
	ConfigureLine(CenterY, RailHeight, ForwardStationX, UnloadPositionX, EmptyReturnOffset);
}

void AGenesisHangerLineActor::ConfigureLine(
	float InCenterY,
	float InRailHeight,
	const TArray<float>& ForwardX,
	float InUnloadX,
	float InReturnOffset)
{
	CenterY = InCenterY;
	RailHeight = InRailHeight;
	this->ForwardStationX = ForwardX;
	UnloadPositionX = InUnloadX;
	EmptyReturnOffset = InReturnOffset;
	ConfiguredCenterY = InCenterY;
	LineSpline->ClearSplinePoints(false);

	TArray<FVector> Points;
	const float StartX = ForwardX.Num() > 0 ? ForwardX[0] - 1800.0f : 0.0f;
	const float SafeReturnOffset = FMath::Clamp(InReturnOffset, 520.0f, 760.0f);
	const float ReturnDirection = -1.0f;
	const float ReturnY = InCenterY + ReturnDirection * SafeReturnOffset;

	for (float X = StartX; X < InUnloadX; X += 500.0f)
	{
		float Height = InRailHeight;
		if (ForwardX.Num() > 2)
		{
			const float BatteryX = ForwardX[2];
			const float BatteryDistance = FMath::Abs(X - BatteryX);
			if (BatteryDistance < 1400.0f)
			{
				Height += 250.0f * (1.0f - BatteryDistance / 1400.0f);
			}
		}
		Points.Add(FVector(X, InCenterY, Height));
	}
	Points.Add(FVector(InUnloadX, InCenterY, InRailHeight));
	Points.Add(FVector(InUnloadX + 120.0f, InCenterY + ReturnDirection * SafeReturnOffset * 0.25f, InRailHeight + 330.0f));
	Points.Add(FVector(InUnloadX - 420.0f, ReturnY, InRailHeight + 760.0f));

	for (float X = InUnloadX - 820.0f; X > StartX; X -= 500.0f)
	{
		Points.Add(FVector(X, ReturnY, InRailHeight + 760.0f));
	}
	Points.Add(FVector(StartX, ReturnY, InRailHeight + 760.0f));
	Points.Add(FVector(StartX - 520.0f, InCenterY + ReturnDirection * SafeReturnOffset * 0.45f, InRailHeight + 420.0f));

	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		LineSpline->AddSplinePoint(Points[Index], ESplineCoordinateSpace::World, false);
		LineSpline->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}
	LineSpline->SetClosedLoop(true, false);
	LineSpline->UpdateSpline();
	RebuildRailMeshes();
}

float AGenesisHangerLineActor::GetForwardDistanceAtX(float X) const
{
	if (!LineSpline)
	{
		return 0.0f;
	}

	const FVector Location(X, ConfiguredCenterY, GetActorLocation().Z);
	const float InputKey = LineSpline->FindInputKeyClosestToWorldLocation(Location);
	return LineSpline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
}

void AGenesisHangerLineActor::RebuildRailMeshes()
{
	for (USplineMeshComponent* Component : RailMeshes)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}
	RailMeshes.Reset();

	if (!RailMesh || !LineSpline)
	{
		return;
	}

	const int32 SegmentCount = LineSpline->GetNumberOfSplinePoints();
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const int32 NextIndex = (Index + 1) % SegmentCount;
		AddRailSegment(Index, NextIndex, -90.0f);
		AddRailSegment(Index, NextIndex, 87.0f);
	}
}

void AGenesisHangerLineActor::AddRailSegment(int32 StartIndex, int32 EndIndex, float LateralOffset)
{
	FVector StartLocation;
	FVector StartTangent;
	FVector EndLocation;
	FVector EndTangent;
	LineSpline->GetLocationAndTangentAtSplinePoint(StartIndex, StartLocation, StartTangent, ESplineCoordinateSpace::Local);
	LineSpline->GetLocationAndTangentAtSplinePoint(EndIndex, EndLocation, EndTangent, ESplineCoordinateSpace::Local);

	const FVector StartRight = FVector::CrossProduct(StartTangent.GetSafeNormal(), FVector::UpVector).GetSafeNormal();
	const FVector EndRight = FVector::CrossProduct(EndTangent.GetSafeNormal(), FVector::UpVector).GetSafeNormal();
	StartLocation += StartRight * LateralOffset;
	EndLocation += EndRight * LateralOffset;

	USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this);
	Segment->SetupAttachment(Root);
	Segment->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Segment->SetMobility(EComponentMobility::Movable);
	Segment->SetStaticMesh(RailMesh);
	if (RailMaterial)
	{
		Segment->SetMaterial(0, RailMaterial);
	}
	Segment->SetForwardAxis(ESplineMeshAxis::X);
	Segment->SetStartAndEnd(StartLocation, StartTangent, EndLocation, EndTangent);
	Segment->SetStartScale(FVector2D(1.0f, 1.0f));
	Segment->SetEndScale(FVector2D(1.0f, 1.0f));
	Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Segment->RegisterComponent();
	RailMeshes.Add(Segment);
}
