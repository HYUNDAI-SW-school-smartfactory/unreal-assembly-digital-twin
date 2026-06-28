#include "GenesisFactoryAssemblyManager.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Animation/AnimationAsset.h"
#include "Animation/SkeletalMeshActor.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GenesisAGVActor.h"
#include "GenesisBatteryLiftActor.h"
#include "GenesisFactoryStatusBoard.h"
#include "GenesisFactoryStatusWidget.h"
#include "GenesisHangerCarrier.h"
#include "GenesisHangerLineActor.h"
#include "Paho_Sync_Manager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr int32 ProcessStationCount = 5;

	int32 StationIdToIndex(const FString& StationId)
	{
		const FString Id = StationId.ToUpper();
		if (Id.Contains(TEXT("DOOR_REMOVAL")) || Id.Contains(TEXT("BODY_INPUT"))) return 0;
		if (Id.Contains(TEXT("LIGHT")) || Id.Contains(TEXT("LAMP"))) return 1;
		if (Id.Contains(TEXT("BATTERY")) || Id.Contains(TEXT("BATT"))) return 2;
		if (Id.Contains(TEXT("WHEEL")) || Id.Contains(TEXT("TIRE")) || Id.Contains(TEXT("TYRE"))) return 3;
		if (Id.Contains(TEXT("DOOR_SEAT")) || Id.Contains(TEXT("DOOR&SEAT")) || Id.Contains(TEXT("DOOR_ASSEMBLY")) || Id.Contains(TEXT("SEAT"))) return 4;
		if (Id.Contains(TEXT("INSPECTION"))) return 5;

		FString Digits;
		for (int32 Index = Id.Len() - 1; Index >= 0; --Index)
		{
			const TCHAR Character = Id[Index];
			if (FChar::IsDigit(Character))
			{
				Digits.InsertAt(0, Character);
			}
			else if (!Digits.IsEmpty())
			{
				break;
			}
		}
		if (!Digits.IsEmpty())
		{
			const int32 StationNumber = FCString::Atoi(*Digits);
			if (StationNumber >= 1 && StationNumber <= 6)
			{
				return StationNumber - 1;
			}
			if (StationNumber >= 0 && StationNumber <= 5 && (Id.Contains(TEXT("INDEX")) || Id.Contains(TEXT("IDX"))))
			{
				return StationNumber;
			}
		}
		return INDEX_NONE;
	}

	bool VisualComponentMatchesName(const USceneComponent* Component, const FName TargetName)
	{
		if (!IsValid(Component) || TargetName.IsNone())
		{
			return false;
		}

		const FString Target = TargetName.ToString();
		return Component->GetFName() == TargetName || Component->GetName().Equals(Target, ESearchCase::IgnoreCase);
	}

	FString GetRuntimeActorLabel(const AActor* Actor)
	{
		if (!Actor)
		{
			return FString();
		}
#if WITH_EDITOR
		return Actor->GetActorLabel();
#else
		return Actor->GetName();
#endif
	}

	int32 ExtractLineNumberFromString(const FString& RawLineId)
	{
		const FString Trimmed = RawLineId.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return 0;
		}

		bool bAllDigits = true;
		for (const TCHAR Character : Trimmed)
		{
			if (!FChar::IsDigit(Character))
			{
				bAllDigits = false;
				break;
			}
		}
		if (bAllDigits)
		{
			return FCString::Atoi(*Trimmed);
		}

		const FString Upper = Trimmed.ToUpper();
		FString TrailingDigits;
		for (int32 Index = Upper.Len() - 1; Index >= 0; --Index)
		{
			const TCHAR Character = Upper[Index];
			if (FChar::IsDigit(Character))
			{
				TrailingDigits.InsertAt(0, Character);
			}
			else if (!TrailingDigits.IsEmpty())
			{
				break;
			}
		}

		if (!TrailingDigits.IsEmpty())
		{
			return FCString::Atoi(*TrailingDigits);
		}

		return 0;
	}

	bool TryGetJsonFieldAsString(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, FString& OutValue)
	{
		if (!Object.IsValid())
		{
			return false;
		}

		FString StringValue;
		if (Object->TryGetStringField(FieldName, StringValue))
		{
			OutValue = StringValue;
			return true;
		}

		double NumberValue = 0.0;
		if (Object->TryGetNumberField(FieldName, NumberValue))
		{
			OutValue = FString::FromInt(FMath::RoundToInt(NumberValue));
			return true;
		}

		return false;
	}

	int32 LineIdToInt(const TSharedPtr<FJsonObject>& LineObject)
	{
		if (!LineObject.IsValid())
		{
			return 0;
		}

		static const TCHAR* CandidateFields[] =
		{
			TEXT("line_id"),
			TEXT("lineId"),
			TEXT("line"),
			TEXT("line_no"),
			TEXT("lineNo"),
			TEXT("line_number"),
			TEXT("lineNumber"),
			TEXT("id")
		};

		for (const TCHAR* FieldName : CandidateFields)
		{
			FString RawValue;
			if (TryGetJsonFieldAsString(LineObject, FieldName, RawValue))
			{
				const int32 ParsedLineId = ExtractLineNumberFromString(RawValue);
				if (ParsedLineId > 0)
				{
					return ParsedLineId;
				}
			}
		}

		const TSharedPtr<FJsonObject>* NestedLineObject = nullptr;
		if (LineObject->TryGetObjectField(TEXT("line"), NestedLineObject) && NestedLineObject && NestedLineObject->IsValid())
		{
			for (const TCHAR* FieldName : CandidateFields)
			{
				FString RawValue;
				if (TryGetJsonFieldAsString(*NestedLineObject, FieldName, RawValue))
				{
					const int32 ParsedLineId = ExtractLineNumberFromString(RawValue);
					if (ParsedLineId > 0)
					{
						return ParsedLineId;
					}
				}
			}
		}

		return 0;
	}

	FString CompactRuntimeName(const FString& Value)
	{
		FString Result = Value.ToUpper();
		Result.ReplaceInline(TEXT("_"), TEXT(""));
		Result.ReplaceInline(TEXT("-"), TEXT(""));
		Result.ReplaceInline(TEXT(" "), TEXT(""));
		return Result;
	}

	bool RuntimeNameMatchesAny(const AActor* Actor, const TArray<FString>& Candidates)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		const FString CompactLabel = CompactRuntimeName(GetRuntimeActorLabel(Actor));
		const FString CompactName = CompactRuntimeName(Actor->GetName());
		for (const FString& Candidate : Candidates)
		{
			const FString CompactCandidate = CompactRuntimeName(Candidate);
			if (CompactLabel.Contains(CompactCandidate) || CompactName.Contains(CompactCandidate))
			{
				return true;
			}
		}
		return false;
	}

	bool RuntimeNameEqualsAny(const AActor* Actor, const TArray<FString>& Candidates)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		const FString CompactLabel = CompactRuntimeName(GetRuntimeActorLabel(Actor));
		const FString CompactName = CompactRuntimeName(Actor->GetName());
		for (const FString& Candidate : Candidates)
		{
			const FString CompactCandidate = CompactRuntimeName(Candidate);
			if (CompactLabel.Equals(CompactCandidate, ESearchCase::IgnoreCase) ||
				CompactName.Equals(CompactCandidate, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	bool IsTelemetryIdle(const FGenesisStationTelemetry& Telemetry)
	{
		return !Telemetry.bRunning || Telemetry.CycleTime >= 998.5f;
	}

	bool RuntimeNameContainsToken(const AActor* Actor, const FString& Token)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		const FString CompactToken = CompactRuntimeName(Token);
		return CompactRuntimeName(GetRuntimeActorLabel(Actor)).Contains(CompactToken) ||
			CompactRuntimeName(Actor->GetName()).Contains(CompactToken);
	}

	bool IsLikelyTireAssemblyCell(const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		const FString ClassPath = Actor->GetClass()->GetPathName();
		return ClassPath.Contains(TEXT("BP_TireAssemblyCell")) ||
			ClassPath.Contains(TEXT("GenesisTireAssemblyCellActor")) ||
			RuntimeNameContainsToken(Actor, TEXT("TireAssemblyCell"));
	}

	bool IsLikelyLeftTireAssemblyCell(const AActor* Actor)
	{
		if (!IsLikelyTireAssemblyCell(Actor))
		{
			return false;
		}

		const FString ClassPath = Actor->GetClass()->GetPathName();
		return ClassPath.Contains(TEXT("BP_TireAssemblyCell_L")) ||
			RuntimeNameContainsToken(Actor, TEXT("TireAssemblyCell_L")) ||
			RuntimeNameContainsToken(Actor, TEXT("TireCell_L")) ||
			RuntimeNameContainsToken(Actor, TEXT("Left"));
	}

	bool IsLikelyRightTireAssemblyCell(const AActor* Actor)
	{
		return IsLikelyTireAssemblyCell(Actor) && !IsLikelyLeftTireAssemblyCell(Actor);
	}

	TArray<FString> MakeAgvNameCandidates(int32 LineId)
	{
		if (LineId == 1)
		{
			return {
				TEXT("GenesisAGVActor1"),
				TEXT("GenesisAGVActor"),
				TEXT("AGV1"),
				TEXT("AGV_1"),
				TEXT("Line1_AGV"),
				TEXT("Line_1_AGV"),
			};
		}

		return {
			FString::Printf(TEXT("GenesisAGVActor%d"), LineId),
			FString::Printf(TEXT("AGV%d"), LineId),
			FString::Printf(TEXT("AGV_%d"), LineId),
			FString::Printf(TEXT("Line%d_AGV"), LineId),
			FString::Printf(TEXT("Line_%d_AGV"), LineId),
			FString::Printf(TEXT("Line%02d_AGV"), LineId),
		};
	}

	TArray<FString> MakeBatteryLiftNameCandidates(int32 LineId)
	{
		if (LineId == 1)
		{
			return {
				TEXT("GenesisBatteryLiftActor1"),
				TEXT("GenesisBatteryLiftActor"),
				TEXT("BatteryLift1"),
				TEXT("BatteryLift_1"),
				TEXT("Lift1"),
				TEXT("Lift_1"),
				TEXT("Line1_BatteryLift"),
				TEXT("Line_1_BatteryLift"),
			};
		}

		return {
			FString::Printf(TEXT("GenesisBatteryLiftActor%d"), LineId),
			FString::Printf(TEXT("BatteryLift%d"), LineId),
			FString::Printf(TEXT("BatteryLift_%d"), LineId),
			FString::Printf(TEXT("Lift%d"), LineId),
			FString::Printf(TEXT("Lift_%d"), LineId),
			FString::Printf(TEXT("Line%d_BatteryLift"), LineId),
			FString::Printf(TEXT("Line_%d_BatteryLift"), LineId),
		};
	}

	TArray<FVector> MakeLegacyAgvRoute(float ReferenceY, float Z)
	{
		if (ReferenceY < 4000.0f)
		{
			return {
				FVector(-8821.0f, 115.0f, Z),
				FVector(-590.0f, 6390.0f, Z),
				FVector(200.0f, 3180.0f, Z),
				FVector(7700.0f, 3180.0f, Z),
				FVector(7700.0f, 2130.0f, Z),
			};
		}

		if (ReferenceY < 8000.0f)
		{
			return {
				FVector(-8821.0f, 6393.0f, Z),
				FVector(-590.0f, 6390.0f, Z),
				FVector(678.0f, 4868.0f, Z),
				FVector(7700.0f, 4868.0f, Z),
				FVector(7700.0f, 5865.0f, Z),
			};
		}

		return {
			FVector(-8821.0f, 12468.0f, Z),
			FVector(-590.0f, 6390.0f, Z),
			FVector(200.0f, 9600.0f, Z),
			FVector(7700.0f, 9600.0f, Z),
			FVector(7700.0f, 10590.0f, Z),
		};
	}

	TArray<FString> MakeLineActorNameCandidates(const FString& Prefix, int32 LineId)
	{
		TArray<FString> Candidates = {
			FString::Printf(TEXT("%s%d"), *Prefix, LineId),
			FString::Printf(TEXT("%s_%d"), *Prefix, LineId),
			FString::Printf(TEXT("%s%02d"), *Prefix, LineId),
			FString::Printf(TEXT("%s_%02d"), *Prefix, LineId),
			FString::Printf(TEXT("Line%d_%s"), LineId, *Prefix),
			FString::Printf(TEXT("Line_%d_%s"), LineId, *Prefix),
			FString::Printf(TEXT("Line%02d_%s"), LineId, *Prefix),
		};
		if (Prefix.Equals(TEXT("BatteryLift"), ESearchCase::IgnoreCase))
		{
			Candidates.Add(LineId == 1 ? TEXT("GenesisBatteryLiftActor") : FString::Printf(TEXT("GenesisBatteryLiftActor%d"), LineId));
		}
		return Candidates;
	}
}

AGenesisFactoryAssemblyManager::AGenesisFactoryAssemblyManager()
{
	PrimaryActorTick.bCanEverTick = true;

	FactoryRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(FactoryRoot);

	HangerClass = AGenesisHangerCarrier::StaticClass();
	AGVClass = AGenesisAGVActor::StaticClass();
	BatteryLiftClass = AGenesisBatteryLiftActor::StaticClass();

	static ConstructorHelpers::FClassFinder<AActor> CarFinder(TEXT("/Game/Blueprints/BP_Car"));
	if (CarFinder.Succeeded())
	{
		CarClass = CarFinder.Class;
	}

	StatusBoardClass = AGenesisFactoryStatusBoard::StaticClass();

	static ConstructorHelpers::FClassFinder<AActor> TireCellRightFinder(TEXT("/Game/Blueprints/BP_TireAssemblyCell"));
	if (TireCellRightFinder.Succeeded())
	{
		TireCellRightClass = TireCellRightFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<AActor> TireCellLeftFinder(TEXT("/Game/Blueprints/BP_TireAssemblyCell_L"));
	if (TireCellLeftFinder.Succeeded())
	{
		TireCellLeftClass = TireCellLeftFinder.Class;
	}

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> LeftRobotFinder(TEXT("/Game/Meshes/Robot/SK_RoboArm04.SK_RoboArm04"));
	if (LeftRobotFinder.Succeeded())
	{
		LeftRobotMesh = LeftRobotFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> LeftAnimationFinder(TEXT("/Game/Meshes/Robot/RobotArm_02.RobotArm_02"));
	if (LeftAnimationFinder.Succeeded())
	{
		LeftRobotAnimation = LeftAnimationFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> RightAnimationFinder(TEXT("/Game/Meshes/RobotLarge/SK_TruckWelding_Anim.SK_TruckWelding_Anim"));
	if (RightAnimationFinder.Succeeded())
	{
		RightRobotAnimation = RightAnimationFinder.Object;
	}

	FillDefaults();
}

void AGenesisFactoryAssemblyManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	FixKnownPreplacedBoardTransformsByLabels();
}

void AGenesisFactoryAssemblyManager::BeginPlay()
{
	Super::BeginPlay();
	bFactoryEndingPlay = false;
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
		GEngine->ClearOnScreenDebugMessages();
	}
	FixKnownPreplacedBoardTransformsByLabels();
	InitializeFactory();
}

void AGenesisFactoryAssemblyManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bFactoryEndingPlay = true;
	SetActorTickEnabled(false);

	if (IsValid(MqttManager))
	{
		MqttManager->Delegate_Message_Arrived.RemoveDynamic(this, &AGenesisFactoryAssemblyManager::HandleMqttMessage);
		FJsonObjectWrapper OutCode;
		OutCode.JsonObject = MakeShared<FJsonObject>();
		if (!SubscribeTopic.IsEmpty())
		{
			MqttManager->MQTT_Sync_Unsubscribe(OutCode, SubscribeTopic);
		}
		MqttManager->MQTT_Sync_Destroy();
	}
	MqttManager = nullptr;
	RuntimeLines.Empty();
	EquipmentInitialTransforms.Empty();
	Super::EndPlay(EndPlayReason);
}

void AGenesisFactoryAssemblyManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFactoryEndingPlay)
	{
		return;
	}

	for (int32 LineIndex = 0; LineIndex < RuntimeLines.Num(); ++LineIndex)
	{
		TickLine(LineIndex, DeltaSeconds);
	}
}

void AGenesisFactoryAssemblyManager::FillDefaults()
{
	InitialHangersPerLine = FMath::Max(InitialHangersPerLine, 1);
	MaxRampUpHangersPerLine = FMath::Max(MaxRampUpHangersPerLine, InitialHangersPerLine);
	UnloadX = FMath::Min(UnloadX, 15650.0f);
	EmptyReturnLoopOffset = FMath::Clamp(EmptyReturnLoopOffset, 520.0f, 760.0f);
	AgvRuntimeMoveSpeed = FMath::Max(AgvRuntimeMoveSpeed, 2400.0f);
	BatteryLiftRuntimeHeight = FMath::Max(BatteryLiftRuntimeHeight, 360.0f);
	BatteryAgvLeadTime = FMath::Max(BatteryAgvLeadTime, 15.0f);
	BatteryLiftMinimumTime = FMath::Max(BatteryLiftMinimumTime, 30.0f);
	TireCellMinimumTime = FMath::Max(TireCellMinimumTime, 60.0f);
	InspectionTravelTime = FMath::Max(InspectionTravelTime, 8.0f);
	if (SubscribeTopic.IsEmpty() || SubscribeTopic.Equals(TEXT("factory/genesis/stations"), ESearchCase::IgnoreCase))
	{
		SubscribeTopic = TEXT("factory/genesis/line/status");
	}

	if (StationX.Num() != ProcessStationCount)
	{
		StationX = {1620.0f, 4800.0f, 8020.0f, 11420.0f, 14640.0f};
	}

	if (CarVisuals.DoorComponentNames.Num() == 0)
	{
		CarVisuals.DoorComponentNames = {
			TEXT("Door_RF"),
			TEXT("Door_RB"),
			TEXT("Door_LF"),
			TEXT("Door_LB")
		};
	}
	if (CarVisuals.LightComponentNames.Num() == 0)
	{
		CarVisuals.LightComponentNames = {
			TEXT("Body_2"),
			TEXT("Body_8"),
			TEXT("Body_5"),
			TEXT("Body_20"),
			TEXT("Body_13"),
			TEXT("Body_21")
		};
	}
	if (CarVisuals.WheelComponentNames.Num() == 0)
	{
		CarVisuals.WheelComponentNames = {
			TEXT("Wheel_RB"),
			TEXT("Tire_RB"),
			TEXT("Wheel_RF"),
			TEXT("Tire_RF"),
			TEXT("Wheel_LB"),
			TEXT("Tire_LB"),
			TEXT("Wheel_LF"),
			TEXT("Tire_LF"),
			TEXT("Body_WheelPart_2_RB"),
			TEXT("Body_WheelPart_1_RB"),
			TEXT("StaticMesh52"),
			TEXT("StaticMesh50"),
			TEXT("Body_WheelPart_1_LB"),
			TEXT("Body_WheelPart_2_LB"),
			TEXT("StaticMesh49"),
			TEXT("Body_WheelPart_1_LF"),
			TEXT("Body_WheelPart_2_LF"),
			TEXT("StaticMesh51"),
			TEXT("Body_WheelPart_1_RF"),
			TEXT("Body_WheelPart_2_RF")
		};
	}
	if (CarVisuals.RotatingWheelComponentNames.Num() == 0)
	{
		CarVisuals.RotatingWheelComponentNames = {
			TEXT("Wheel_RB"),
			TEXT("Tire_RB"),
			TEXT("Wheel_RF"),
			TEXT("Tire_RF"),
			TEXT("Wheel_LB"),
			TEXT("Tire_LB"),
			TEXT("Wheel_LF"),
			TEXT("Tire_LF")
		};
	}

	if (Lines.Num() == 0)
	{
		const TArray<float> CenterLines = {1620.0f, 6400.0f, 11200.0f};
		for (int32 Index = 0; Index < CenterLines.Num(); ++Index)
		{
			FGenesisAssemblyLineConfig Line;
			Line.LineId = Index + 1;
			Line.CenterY = CenterLines[Index];
			Lines.Add(Line);
		}
	}

	const TArray<float> DefaultCycles = {6.0f, 5.5f, 7.0f, 8.0f, 7.0f, 5.0f};
	if (MinimumStationProcessTimes.Num() != 6)
	{
		MinimumStationProcessTimes = {6.0f, 6.0f, BatteryAgvLeadTime + BatteryLiftMinimumTime, TireCellMinimumTime, 6.0f, InspectionTravelTime};
	}
	else
	{
		MinimumStationProcessTimes[0] = FMath::Max(MinimumStationProcessTimes[0], 6.0f);
		MinimumStationProcessTimes[1] = FMath::Max(MinimumStationProcessTimes[1], 6.0f);
		MinimumStationProcessTimes[2] = FMath::Max(MinimumStationProcessTimes[2], BatteryAgvLeadTime + BatteryLiftMinimumTime);
		MinimumStationProcessTimes[3] = FMath::Max(MinimumStationProcessTimes[3], TireCellMinimumTime);
		MinimumStationProcessTimes[4] = FMath::Max(MinimumStationProcessTimes[4], 6.0f);
		MinimumStationProcessTimes[5] = FMath::Max(MinimumStationProcessTimes[5], InspectionTravelTime);
	}
	for (FGenesisAssemblyLineConfig& Line : Lines)
	{
		if (Line.Stations.Num() != 6)
		{
			Line.Stations.Reset();
			for (int32 StationIndex = 0; StationIndex < 6; ++StationIndex)
			{
				FGenesisStationTelemetry Telemetry;
				Telemetry.Station = static_cast<EGenesisAssemblyStation>(StationIndex);
				Telemetry.CycleTime = DefaultCycles[StationIndex];
				Telemetry.Utilization = FMath::FRandRange(0.72f, 0.92f);
				Telemetry.DefectRate = FMath::FRandRange(0.000005f, 0.00002f);
				Line.Stations.Add(Telemetry);
			}
		}
	}
}

void AGenesisFactoryAssemblyManager::InitializeFactory()
{
	FillDefaults();
	if (!HangerClass || HangerClass->GetPathName().Contains(TEXT("BP_HangerVehicle")))
	{
		HangerClass = AGenesisHangerCarrier::StaticClass();
	}
	if (bRemovePreviewCarsOnStart)
	{
		RemovePreviewCars();
	}
	// Do not delete pre-placed dashboard actors. The MainMaps layout is now authored
	// by hand and the status boards are intentionally placed in the level.
	AutoBindMapActors();
	RuntimeLines.SetNum(Lines.Num());

	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		InitializeLine(LineIndex);
	}

	if (bEnableMqtt)
	{
		BindMqtt();
	}

	UE_LOG(LogTemp, Display, TEXT("GenesisFactory: Initialized %d assembly lines."), RuntimeLines.Num());
}

void AGenesisFactoryAssemblyManager::RemovePreviewCars()
{
	if (!CarClass)
	{
		return;
	}

	TArray<AActor*> PreviewCars;
	for (TActorIterator<AActor> It(GetWorld(), CarClass); It; ++It)
	{
		if (IsValid(*It))
		{
			PreviewCars.Add(*It);
		}
	}

	for (AActor* PreviewCar : PreviewCars)
	{
		PreviewCar->Destroy();
	}

	UE_LOG(LogTemp, Display, TEXT("GenesisFactory: Removed %d preview cars before starting hanger flow."), PreviewCars.Num());
}

void AGenesisFactoryAssemblyManager::RemoveLegacyStatusBoards()
{
	TArray<AActor*> LegacyBoards;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const FString ClassPath = It->GetClass()->GetPathName();
		if (ClassPath.Contains(TEXT("BP_MQTT_DisplayBoard_C")) ||
			ClassPath.Contains(TEXT("BP_FactoryDisplayBoard_C")) ||
			ClassPath.Contains(TEXT("Factory_Dashboard_Board_Actor")))
		{
			LegacyBoards.Add(*It);
		}
	}
	for (AActor* Board : LegacyBoards)
	{
		if (IsValid(Board))
		{
			Board->Destroy();
		}
	}
}

void AGenesisFactoryAssemblyManager::AutoBindMapActors()
{
	TArray<AActor*> Agvs;
	TArray<AActor*> BatteryLifts;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		const FString ClassPath = Actor->GetClass()->GetPathName();
		if (Actor->IsA<AGenesisAGVActor>() ||
			ClassPath.Contains(TEXT("BP_AGV_C")) ||
			ClassPath.Contains(TEXT("BP_AGV_Controlled_C")))
		{
			Agvs.Add(Actor);
		}
		if (Actor->IsA<AGenesisBatteryLiftActor>() ||
			ClassPath.Contains(TEXT("BP_BatteryLift_C")) ||
			ClassPath.Contains(TEXT("BP_BatteryLift_Controlled_C")))
		{
			BatteryLifts.Add(Actor);
		}
	}

	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		FGenesisLineActorBindings& Bindings = Lines[LineIndex].Actors;
		const int32 LineId = Lines[LineIndex].LineId;

		Bindings.AGV = nullptr;
		Bindings.BatteryLift = nullptr;

		for (AActor* Agv : Agvs)
		{
			if (RuntimeNameEqualsAny(Agv, MakeAgvNameCandidates(LineId)))
			{
				Bindings.AGV = Agv;
				break;
			}
		}

		for (AActor* Lift : BatteryLifts)
		{
			if (RuntimeNameEqualsAny(Lift, MakeBatteryLiftNameCandidates(LineId)))
			{
				Bindings.BatteryLift = Lift;
				break;
			}
		}

		float BestRightCellDistance = TNumericLimits<float>::Max();
		float BestLeftCellDistance = TNumericLimits<float>::Max();

		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* Actor = *It;
			const float YDistance = FMath::Abs(Actor->GetActorLocation().Y - Lines[LineIndex].CenterY);

			if (IsLikelyRightTireAssemblyCell(Actor) && YDistance < BestRightCellDistance)
			{
				BestRightCellDistance = YDistance;
				Bindings.TireCellRight = Actor;
			}
			if (IsLikelyLeftTireAssemblyCell(Actor) && YDistance < BestLeftCellDistance)
			{
				BestLeftCellDistance = YDistance;
				Bindings.TireCellLeft = Actor;
			}
		}
	}
}

void AGenesisFactoryAssemblyManager::InitializeLine(int32 LineIndex)
{
	if (!Lines.IsValidIndex(LineIndex) || !RuntimeLines.IsValidIndex(LineIndex))
	{
		return;
	}

	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	bool bFoundPreplacedHangerLine = false;
	for (TActorIterator<AGenesisHangerLineActor> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && FMath::IsNearlyEqual(It->GetConfiguredCenterY(), Lines[LineIndex].CenterY, 20.0f))
		{
			Runtime.HangerLine = *It;
			bFoundPreplacedHangerLine = true;
			break;
		}
	}
	if (!IsValid(Runtime.HangerLine))
	{
		Runtime.HangerLine = GetWorld()->SpawnActor<AGenesisHangerLineActor>(
			AGenesisHangerLineActor::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator);
	}
	if (IsValid(Runtime.HangerLine))
	{
		Runtime.HangerLine->LineId = Lines[LineIndex].LineId;
		if (bFoundPreplacedHangerLine && bUsePreplacedHangerLineGeometry)
		{
			Lines[LineIndex].CenterY = Runtime.HangerLine->CenterY;
			if (Runtime.HangerLine->ForwardStationX.Num() == ProcessStationCount)
			{
				StationX = Runtime.HangerLine->ForwardStationX;
			}
			UnloadX = Runtime.HangerLine->UnloadPositionX;
			HangerRailHeight = Runtime.HangerLine->RailHeight;
			EmptyReturnLoopOffset = Runtime.HangerLine->EmptyReturnOffset;
		}
		else
		{
			Runtime.HangerLine->ConfigureLine(
				Lines[LineIndex].CenterY,
				HangerRailHeight,
				StationX,
				UnloadX,
				EmptyReturnLoopOffset);
		}
	}
	Runtime.StationQueues.SetNum(ProcessStationCount);
	Runtime.StationOccupants.Init(INDEX_NONE, ProcessStationCount);
	Runtime.BufferFullSince.Init(-1.0, 6);
	CreateBoards(LineIndex);
	if (bPausePreplacedEquipmentOnStart)
	{
		SetLineEquipmentWaiting(LineIndex, true);
	}

	const int32 HangerCount = FMath::Clamp(InitialHangersPerLine, 1, MaxRampUpHangersPerLine);
	for (int32 VehicleIndex = 0; VehicleIndex < HangerCount; ++VehicleIndex)
	{
		SpawnEntryHanger(LineIndex, true);
	}

	StopPreplacedStationRobotAnimations(LineIndex);
}

void AGenesisFactoryAssemblyManager::TickLine(int32 LineIndex, float DeltaSeconds)
{
	if (!RuntimeLines.IsValidIndex(LineIndex))
	{
		return;
	}

	for (int32 VehicleIndex = 0; VehicleIndex < RuntimeLines[LineIndex].Vehicles.Num(); ++VehicleIndex)
	{
		TickVehicle(LineIndex, VehicleIndex, DeltaSeconds);
	}

	DispatchQueues(LineIndex);
	TickInspection(LineIndex, DeltaSeconds);
	UpdateBottleneck(LineIndex);
	UpdateBoards(LineIndex);
}

void AGenesisFactoryAssemblyManager::TickVehicle(int32 LineIndex, int32 VehicleIndex, float DeltaSeconds)
{
	FGenesisVehicleRuntime& Vehicle = RuntimeLines[LineIndex].Vehicles[VehicleIndex];

	if (Vehicle.State == EGenesisVehicleFlowState::MovingToBuffer ||
		Vehicle.State == EGenesisVehicleFlowState::MovingToStation ||
		Vehicle.State == EGenesisVehicleFlowState::MovingToUnload ||
		Vehicle.State == EGenesisVehicleFlowState::ReturningEmpty)
	{
		const float Duration = Vehicle.State == EGenesisVehicleFlowState::ReturningEmpty ? EmptyReturnTime : TravelTime;
		Vehicle.MoveAlpha = FMath::Min(1.0f, Vehicle.MoveAlpha + DeltaSeconds / FMath::Max(0.1f, Duration));
		const float Distance = FMath::Lerp(Vehicle.MoveStartDistance, Vehicle.MoveTargetDistance, Vehicle.MoveAlpha);
		const FTransform NewTransform = GetLineTransformAtDistance(LineIndex, Distance);
		if (IsValid(Vehicle.Hanger))
		{
			Vehicle.Hanger->SetActorTransform(NewTransform);
		}

		if (Vehicle.MoveAlpha >= 1.0f)
		{
			if (Vehicle.State == EGenesisVehicleFlowState::MovingToBuffer)
			{
				Vehicle.State = EGenesisVehicleFlowState::Queued;
			}
			else if (Vehicle.State == EGenesisVehicleFlowState::MovingToStation)
			{
				StartStationProcess(LineIndex, VehicleIndex, Vehicle.StationIndex);
			}
			else if (Vehicle.State == EGenesisVehicleFlowState::MovingToUnload)
			{
				UnloadVehicle(LineIndex, VehicleIndex);
			}
			else
			{
				FinishEmptyReturn(LineIndex, VehicleIndex);
			}
		}
		return;
	}

	if (Vehicle.State == EGenesisVehicleFlowState::Processing)
	{
		if (Lines[LineIndex].Stations.IsValidIndex(Vehicle.StationIndex) &&
			Lines[LineIndex].Stations[Vehicle.StationIndex].bRunning)
		{
			if (!Vehicle.bStationWorkStarted)
			{
				TriggerStationVisuals(LineIndex, Vehicle.StationIndex, Vehicle.Car);
				Vehicle.bStationWorkStarted = true;
			}

			Vehicle.RemainingProcessTime -= DeltaSeconds;
			if (Vehicle.RemainingProcessTime <= 0.0f)
			{
				CompleteStationProcess(LineIndex, VehicleIndex);
			}
		}
		return;
	}

	if (Vehicle.State == EGenesisVehicleFlowState::BlockedAfterProcess)
	{
		CompleteStationProcess(LineIndex, VehicleIndex);
	}
}

void AGenesisFactoryAssemblyManager::DispatchQueues(int32 LineIndex)
{
	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	for (int32 StationIndex = 0; StationIndex < ProcessStationCount; ++StationIndex)
	{
		if (Runtime.StationOccupants[StationIndex] != INDEX_NONE || Runtime.StationQueues[StationIndex].Num() == 0)
		{
			continue;
		}

		const int32 VehicleIndex = Runtime.StationQueues[StationIndex][0];
		Runtime.StationQueues[StationIndex].RemoveAt(0);
		Runtime.StationOccupants[StationIndex] = VehicleIndex;
		StartMovingToStation(LineIndex, VehicleIndex, StationIndex);

		for (int32 QueueIndex = 0; QueueIndex < Runtime.StationQueues[StationIndex].Num(); ++QueueIndex)
		{
			const int32 QueuedVehicleIndex = Runtime.StationQueues[StationIndex][QueueIndex];
			if (Runtime.Vehicles.IsValidIndex(QueuedVehicleIndex))
			{
				FGenesisVehicleRuntime& QueuedVehicle = Runtime.Vehicles[QueuedVehicleIndex];
				if (QueuedVehicle.State == EGenesisVehicleFlowState::Queued ||
					QueuedVehicle.State == EGenesisVehicleFlowState::MovingToBuffer)
				{
					QueuedVehicle.State = EGenesisVehicleFlowState::MovingToBuffer;
					QueuedVehicle.MoveAlpha = 0.0f;
					const FVector CurrentLocation = IsValid(QueuedVehicle.Hanger)
						? QueuedVehicle.Hanger->GetActorLocation()
						: GetBufferLocation(LineIndex, StationIndex, QueueIndex);
					if (USplineComponent* Spline = Runtime.HangerLine ? Runtime.HangerLine->GetLineSpline() : nullptr)
					{
						const float InputKey = Spline->FindInputKeyClosestToWorldLocation(CurrentLocation);
						QueuedVehicle.MoveStartDistance = Spline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
					}
					QueuedVehicle.MoveTargetDistance = GetLineDistanceForX(
						LineIndex,
						GetBufferLocation(LineIndex, StationIndex, QueueIndex).X);
				}
			}
		}
	}
}

void AGenesisFactoryAssemblyManager::StartMovingToStation(int32 LineIndex, int32 VehicleIndex, int32 StationIndex)
{
	FGenesisVehicleRuntime& Vehicle = RuntimeLines[LineIndex].Vehicles[VehicleIndex];
	Vehicle.State = EGenesisVehicleFlowState::MovingToStation;
	Vehicle.StationIndex = StationIndex;
	Vehicle.MoveAlpha = 0.0f;
	Vehicle.MoveStart = IsValid(Vehicle.Hanger) ? Vehicle.Hanger->GetActorLocation() : GetBufferLocation(LineIndex, StationIndex, 0);
	Vehicle.MoveTarget = GetStationLocation(LineIndex, StationIndex);
	if (RuntimeLines[LineIndex].HangerLine && RuntimeLines[LineIndex].HangerLine->GetLineSpline())
	{
		USplineComponent* Spline = RuntimeLines[LineIndex].HangerLine->GetLineSpline();
		const float InputKey = Spline->FindInputKeyClosestToWorldLocation(Vehicle.MoveStart);
		Vehicle.MoveStartDistance = Spline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
	}
	else
	{
		Vehicle.MoveStartDistance = 0.0f;
	}
	Vehicle.MoveTargetDistance = GetLineDistanceForX(LineIndex, GetVehicleStopX(StationIndex));
}

void AGenesisFactoryAssemblyManager::StartStationProcess(int32 LineIndex, int32 VehicleIndex, int32 StationIndex)
{
	if (!Lines[LineIndex].Stations.IsValidIndex(StationIndex))
	{
		return;
	}

	FGenesisVehicleRuntime& Vehicle = RuntimeLines[LineIndex].Vehicles[VehicleIndex];
	Vehicle.State = EGenesisVehicleFlowState::Processing;
	Vehicle.StationIndex = StationIndex;
	Vehicle.bStationWorkStarted = false;
	const FGenesisStationTelemetry& Telemetry = Lines[LineIndex].Stations[StationIndex];
	const float MqttCycleTime = FMath::Clamp(Telemetry.CycleTime, 0.5f, MqttCycleTimeMax);
	Vehicle.RemainingProcessTime = (bUseMqttCycleTimeAsAuthoritative && Telemetry.bCycleTimeFromMqtt)
		? MqttCycleTime
		: FMath::Max(MqttCycleTime, GetMinimumProcessTime(StationIndex));

	if (!Vehicle.bDefective && FMath::FRand() <= Lines[LineIndex].Stations[StationIndex].DefectRate)
	{
		Vehicle.bDefective = true;
		SetDefectHighlight(Vehicle.Car, true);
	}
}

void AGenesisFactoryAssemblyManager::CompleteStationProcess(int32 LineIndex, int32 VehicleIndex)
{
	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	FGenesisVehicleRuntime& Vehicle = Runtime.Vehicles[VehicleIndex];
	const int32 CurrentStation = Vehicle.StationIndex;

	ApplyCompletedStationVisuals(CurrentStation, Vehicle.Car);
	if (CurrentStation == 2)
	{
		SetStationEquipmentWaiting(LineIndex, CurrentStation, true);
	}
	else if (CurrentStation == 3)
	{
		SetStationEquipmentWaiting(LineIndex, CurrentStation, true);
	}

	if (CurrentStation >= ProcessStationCount - 1)
	{
		if (Runtime.InspectionVehicles.Num() >= BufferCapacity)
		{
			Vehicle.State = EGenesisVehicleFlowState::BlockedAfterProcess;
			return;
		}
		Runtime.StationOccupants[CurrentStation] = INDEX_NONE;
		MoveVehicleToUnload(LineIndex, VehicleIndex);
		return;
	}

	const int32 NextStation = CurrentStation + 1;
	if (Runtime.StationQueues[NextStation].Num() >= BufferCapacity)
	{
		Vehicle.State = EGenesisVehicleFlowState::BlockedAfterProcess;
		return;
	}

	Runtime.StationOccupants[CurrentStation] = INDEX_NONE;
	Runtime.StationQueues[NextStation].Add(VehicleIndex);
	Vehicle.State = EGenesisVehicleFlowState::MovingToBuffer;
	Vehicle.StationIndex = NextStation;
	Vehicle.MoveAlpha = 0.0f;
	if (USplineComponent* Spline = Runtime.HangerLine ? Runtime.HangerLine->GetLineSpline() : nullptr)
	{
		const FVector CurrentLocation = IsValid(Vehicle.Hanger) ? Vehicle.Hanger->GetActorLocation() : GetStationLocation(LineIndex, CurrentStation);
		const float InputKey = Spline->FindInputKeyClosestToWorldLocation(CurrentLocation);
		Vehicle.MoveStartDistance = Spline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
	}
	Vehicle.MoveTargetDistance = GetLineDistanceForX(
		LineIndex,
		GetBufferLocation(LineIndex, NextStation, Runtime.StationQueues[NextStation].Num() - 1).X);

	if (CurrentStation == 0)
	{
		TryRampUpEntryHanger(LineIndex);
	}
}

void AGenesisFactoryAssemblyManager::MoveVehicleToUnload(int32 LineIndex, int32 VehicleIndex)
{
	FGenesisVehicleRuntime& Vehicle = RuntimeLines[LineIndex].Vehicles[VehicleIndex];
	Vehicle.State = EGenesisVehicleFlowState::MovingToUnload;
	Vehicle.MoveAlpha = 0.0f;
	Vehicle.MoveStart = IsValid(Vehicle.Hanger) ? Vehicle.Hanger->GetActorLocation() : GetStationLocation(LineIndex, 4);
	Vehicle.MoveTarget = GetUnloadLocation(LineIndex);
	Vehicle.MoveStartDistance = GetLineDistanceForX(LineIndex, GetVehicleStopX(StationX.Num() - 1));
	Vehicle.MoveTargetDistance = GetLineDistanceForX(LineIndex, UnloadX);
}

void AGenesisFactoryAssemblyManager::UnloadVehicle(int32 LineIndex, int32 VehicleIndex)
{
	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	FGenesisVehicleRuntime& Vehicle = Runtime.Vehicles[VehicleIndex];

	if (IsValid(Vehicle.Car))
	{
		Vehicle.Car->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		const FVector DroppedStartLocation(
			Vehicle.Car->GetActorLocation().X,
			Vehicle.Car->GetActorLocation().Y,
			InspectionVehicleFloorZ);
		Vehicle.Car->SetActorLocation(DroppedStartLocation, false, nullptr, ETeleportType::TeleportPhysics);

		FGenesisInspectionVehicleRuntime InspectionVehicle;
		InspectionVehicle.Car = Vehicle.Car;
		InspectionVehicle.Start = DroppedStartLocation;
		InspectionVehicle.End = GetInspectionEndLocation(LineIndex);
		InspectionVehicle.Duration = Lines[LineIndex].Stations.IsValidIndex(5)
			? FMath::Max(FMath::Clamp(Lines[LineIndex].Stations[5].CycleTime, 0.5f, MqttCycleTimeMax), GetMinimumProcessTime(5))
			: InspectionTravelTime;
		Runtime.InspectionVehicles.Add(InspectionVehicle);

		if (!Vehicle.bDefective &&
			Lines[LineIndex].Stations.IsValidIndex(5) &&
			FMath::FRand() <= Lines[LineIndex].Stations[5].DefectRate)
		{
			Vehicle.bDefective = true;
			SetDefectHighlight(Vehicle.Car, true);
		}
	}

	Vehicle.Car = nullptr;
	Vehicle.State = EGenesisVehicleFlowState::ReturningEmpty;
	Vehicle.MoveAlpha = 0.0f;
	Vehicle.MoveStart = GetUnloadLocation(LineIndex);
	Vehicle.MoveTarget = GetBufferLocation(LineIndex, 0, 0);
	Vehicle.MoveStartDistance = GetLineDistanceForX(LineIndex, UnloadX);
	const float StartDistance = GetLineDistanceForX(LineIndex, GetVehicleStopX(0) - 800.0f);
	const float SplineLength = IsValid(Runtime.HangerLine) && Runtime.HangerLine->GetLineSpline()
		? Runtime.HangerLine->GetLineSpline()->GetSplineLength()
		: Vehicle.MoveStartDistance;
	Vehicle.MoveTargetDistance = SplineLength + StartDistance;
}

void AGenesisFactoryAssemblyManager::FinishEmptyReturn(int32 LineIndex, int32 VehicleIndex)
{
	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	FGenesisVehicleRuntime& Vehicle = Runtime.Vehicles[VehicleIndex];

	if (Runtime.StationQueues[0].Num() >= BufferCapacity)
	{
		Vehicle.MoveAlpha = 1.0f;
		return;
	}

	Runtime.bHasEmptyHangerReturned = true;
	SpawnCarForHanger(LineIndex, VehicleIndex);
	Vehicle.StationIndex = 0;
	Vehicle.State = EGenesisVehicleFlowState::MovingToBuffer;
	Runtime.StationQueues[0].Add(VehicleIndex);
	Vehicle.MoveAlpha = 0.0f;
	Vehicle.MoveStartDistance = GetLineDistanceForX(LineIndex, GetVehicleStopX(0) - 800.0f);
	Vehicle.MoveTargetDistance = GetLineDistanceForX(
		LineIndex,
		GetBufferLocation(LineIndex, 0, Runtime.StationQueues[0].Num() - 1).X);
}

bool AGenesisFactoryAssemblyManager::SpawnEntryHanger(int32 LineIndex, bool bWithCar)
{
	if (!RuntimeLines.IsValidIndex(LineIndex) || !HangerClass)
	{
		return false;
	}

	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	if (Runtime.StationQueues.Num() == 0 || Runtime.StationQueues[0].Num() >= BufferCapacity)
	{
		return false;
	}

	FGenesisVehicleRuntime Vehicle;
	const int32 StationIndex = 0;
	const FTransform StationTransform = GetLineTransformAtDistance(
		LineIndex,
		GetLineDistanceForX(LineIndex, GetBufferLocation(LineIndex, StationIndex, Runtime.StationQueues[0].Num()).X));

	Vehicle.Hanger = GetWorld()->SpawnActor<AActor>(HangerClass, StationTransform);
	Vehicle.StationIndex = StationIndex;
	Vehicle.State = EGenesisVehicleFlowState::Queued;
	Runtime.Vehicles.Add(Vehicle);

	const int32 NewVehicleIndex = Runtime.Vehicles.Num() - 1;
	Runtime.StationQueues[0].Add(NewVehicleIndex);
	if (bWithCar)
	{
		SpawnCarForHanger(LineIndex, NewVehicleIndex);
	}

	return true;
}

void AGenesisFactoryAssemblyManager::TryRampUpEntryHanger(int32 LineIndex)
{
	if (!bRampUpHangersUntilFirstReturn || !RuntimeLines.IsValidIndex(LineIndex))
	{
		return;
	}

	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	if (Runtime.bHasEmptyHangerReturned || Runtime.Vehicles.Num() >= MaxRampUpHangersPerLine)
	{
		return;
	}

	SpawnEntryHanger(LineIndex, true);
}

void AGenesisFactoryAssemblyManager::SpawnCarForHanger(int32 LineIndex, int32 VehicleIndex)
{
	if (!CarClass || !RuntimeLines[LineIndex].Vehicles.IsValidIndex(VehicleIndex))
	{
		return;
	}

	FGenesisVehicleRuntime& Vehicle = RuntimeLines[LineIndex].Vehicles[VehicleIndex];
	USceneComponent* CarAnchor = nullptr;
	if (AGenesisHangerCarrier* Carrier = Cast<AGenesisHangerCarrier>(Vehicle.Hanger))
	{
		CarAnchor = Carrier->GetCarAnchor();
	}
	const FTransform SpawnTransform = IsValid(CarAnchor)
		? CarAnchor->GetComponentTransform()
		: (IsValid(Vehicle.Hanger) ? Vehicle.Hanger->GetActorTransform() : FTransform(GetBufferLocation(LineIndex, 0, 0)));
	Vehicle.Car = GetWorld()->SpawnActor<AActor>(CarClass, SpawnTransform);
	Vehicle.bDefective = false;

	if (IsValid(Vehicle.Car))
	{
		Vehicle.Car->SetActorScale3D(CarSpawnScale);
	}

	if (IsValid(Vehicle.Car) && IsValid(CarAnchor))
	{
		Vehicle.Car->AttachToComponent(CarAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	else if (IsValid(Vehicle.Car) && IsValid(Vehicle.Hanger))
	{
		Vehicle.Car->AttachToActor(Vehicle.Hanger, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	SetMappedComponentsVisible(Vehicle.Car, CarVisuals.DoorComponentNames, true);
	SetMappedComponentsVisible(Vehicle.Car, CarVisuals.SeatComponentNames, false);
	SetMappedComponentsVisible(Vehicle.Car, CarVisuals.LightComponentNames, false);
	SetMappedComponentsVisible(Vehicle.Car, CarVisuals.BatteryComponentNames, false);
	SetMappedComponentsVisible(Vehicle.Car, CarVisuals.WheelComponentNames, false);
	SetDefectHighlight(Vehicle.Car, false);
}

void AGenesisFactoryAssemblyManager::TickInspection(int32 LineIndex, float DeltaSeconds)
{
	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	for (int32 Index = Runtime.InspectionVehicles.Num() - 1; Index >= 0; --Index)
	{
		FGenesisInspectionVehicleRuntime& Vehicle = Runtime.InspectionVehicles[Index];
		if (!IsValid(Vehicle.Car))
		{
			Runtime.InspectionVehicles.RemoveAt(Index);
			continue;
		}

		Vehicle.MoveAlpha = FMath::Min(1.0f, Vehicle.MoveAlpha + DeltaSeconds / FMath::Max(0.1f, Vehicle.Duration));
		Vehicle.Car->SetActorLocation(FMath::Lerp(Vehicle.Start, Vehicle.End, Vehicle.MoveAlpha));
		RotateMappedWheels(Vehicle.Car, 180.0f * DeltaSeconds);

		if (Vehicle.MoveAlpha >= 1.0f)
		{
			Vehicle.Car->Destroy();
			Runtime.InspectionVehicles.RemoveAt(Index);
			++Runtime.TotalProduced;
			UE_LOG(
				LogTemp,
				Display,
				TEXT("GenesisFactory: Line %d produced vehicle %d."),
				Lines[LineIndex].LineId,
				Runtime.TotalProduced);
		}
	}
}

void AGenesisFactoryAssemblyManager::TriggerStationVisuals(int32 LineIndex, int32 StationIndex, AActor* Car)
{
	if (StationIndex == 0 || StationIndex == 1 || StationIndex == 4)
	{
		PlayStationRobots(LineIndex, StationIndex);
	}
	else if (StationIndex == 2)
	{
		EnsureStationEquipmentSpawned(LineIndex, StationIndex);
		SetStationEquipmentWaiting(LineIndex, StationIndex, false);

		AActor* AgvActor = Lines[LineIndex].Actors.AGV;
		AActor* LiftActor = Lines[LineIndex].Actors.BatteryLift;
		if (AGenesisBatteryLiftActor* NativeLift = Cast<AGenesisBatteryLiftActor>(LiftActor))
		{
			NativeLift->LiftHeight = FMath::Max(NativeLift->LiftHeight, BatteryLiftRuntimeHeight);
			NativeLift->LiftEndLocation = NativeLift->LiftStartLocation + FVector(0.0f, 0.0f, NativeLift->LiftHeight);
			NativeLift->OnLiftStarted.AddUniqueDynamic(this, &AGenesisFactoryAssemblyManager::HandleBatteryLiftStarted);
		}
		if (AGenesisAGVActor* NativeAgv = Cast<AGenesisAGVActor>(AgvActor))
		{
			NativeAgv->BatteryLiftRef = LiftActor;
			NativeAgv->MoveSpeed = FMath::Max(NativeAgv->MoveSpeed, AgvRuntimeMoveSpeed);

			if (bUseLegacyAgvRoutes)
			{
				const float RouteY = IsValid(LiftActor)
					? LiftActor->GetActorLocation().Y
					: Lines[LineIndex].CenterY;
				NativeAgv->Waypoints = MakeLegacyAgvRoute(RouteY, NativeAgv->GetActorLocation().Z);
			}
			else if (NativeAgv->Waypoints.Num() <= 1)
			{
				const FVector Start = NativeAgv->GetActorLocation();
				const FVector Lift = IsValid(LiftActor)
					? FVector(LiftActor->GetActorLocation().X, LiftActor->GetActorLocation().Y, Start.Z)
					: FVector(StationX[2] - 520.0f, Lines[LineIndex].CenterY + RobotSideOffset, Start.Z);
				const FVector MidA(Start.X, Lift.Y, Start.Z);
				const FVector Approach(FMath::Lerp(Start.X, Lift.X, 0.85f), Lift.Y, Start.Z);

				NativeAgv->Waypoints.Reset();
				NativeAgv->Waypoints.Add(Start);
				NativeAgv->Waypoints.Add(MidA);
				NativeAgv->Waypoints.Add(Approach);
				NativeAgv->Waypoints.Add(Lift);
			}

			NativeAgv->CurrentWaypointIndex = 0;
			NativeAgv->GoingForward = true;
			NativeAgv->bReturnAfterDelivery = true;
			NativeAgv->StartBatteryInstall();
		}
		else
		{
			TriggerNoArgFunction(AgvActor, TEXT("StartBatteryInstall"));
		}
	}
	else if (StationIndex == 3)
	{
		EnsureStationEquipmentSpawned(LineIndex, StationIndex);
		SetStationEquipmentWaiting(LineIndex, StationIndex, false);
		for (AActor* Cell : {Lines[LineIndex].Actors.TireCellRight.Get(), Lines[LineIndex].Actors.TireCellLeft.Get()})
		{
			TriggerNoArgFunction(Cell, TEXT("StartVehicleTireMount"));
		}
	}
}

void AGenesisFactoryAssemblyManager::HandleBatteryLiftStarted(AGenesisBatteryLiftActor* LiftActor)
{
	if (!IsValid(LiftActor))
	{
		return;
	}

	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		if (!RuntimeLines.IsValidIndex(LineIndex) || Lines[LineIndex].Actors.BatteryLift.Get() != LiftActor)
		{
			continue;
		}

		constexpr int32 BatteryStationIndex = 2;
		if (!Lines[LineIndex].Stations.IsValidIndex(BatteryStationIndex) ||
			!Lines[LineIndex].Stations[BatteryStationIndex].bRunning ||
			!RuntimeLines[LineIndex].StationOccupants.IsValidIndex(BatteryStationIndex))
		{
			return;
		}

		const int32 VehicleIndex = RuntimeLines[LineIndex].StationOccupants[BatteryStationIndex];
		if (!RuntimeLines[LineIndex].Vehicles.IsValidIndex(VehicleIndex))
		{
			return;
		}

		const FGenesisVehicleRuntime& Vehicle = RuntimeLines[LineIndex].Vehicles[VehicleIndex];
		if (Vehicle.State == EGenesisVehicleFlowState::Processing && Vehicle.StationIndex == BatteryStationIndex)
		{
			PlayStationRobots(LineIndex, BatteryStationIndex);
		}
		return;
	}
}

void AGenesisFactoryAssemblyManager::ApplyCompletedStationVisuals(int32 StationIndex, AActor* Car)
{
	if (StationIndex == 0)
	{
		SetMappedComponentsVisible(Car, CarVisuals.DoorComponentNames, false);
	}
	else if (StationIndex == 1)
	{
		SetMappedComponentsVisible(Car, CarVisuals.LightComponentNames, true);
	}
	else if (StationIndex == 2)
	{
		SetMappedComponentsVisible(Car, CarVisuals.BatteryComponentNames, true);
	}
	else if (StationIndex == 3)
	{
		SetMappedComponentsVisible(Car, CarVisuals.WheelComponentNames, true);
	}
	else if (StationIndex == 4)
	{
		SetMappedComponentsVisible(Car, CarVisuals.DoorComponentNames, true);
		SetMappedComponentsVisible(Car, CarVisuals.SeatComponentNames, true);
	}
}

void AGenesisFactoryAssemblyManager::SetMappedComponentsVisible(AActor* Car, const TArray<FName>& Names, bool bVisible) const
{
	if (!IsValid(Car))
	{
		return;
	}

	TInlineComponentArray<USceneComponent*> Components(Car);
	for (const FName Name : Names)
	{
		if (Name.IsNone())
		{
			continue;
		}
		for (USceneComponent* Component : Components)
		{
			if (VisualComponentMatchesName(Component, Name))
			{
				Component->SetVisibility(bVisible, true);
				break;
			}
		}
	}
}

void AGenesisFactoryAssemblyManager::SetDefectHighlight(AActor* Car, bool bDefective) const
{
	if (!IsValid(Car))
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> Components(Car);
	for (UPrimitiveComponent* Component : Components)
	{
		if (IsValid(Component))
		{
			Component->SetRenderCustomDepth(bDefective);
			Component->SetCustomDepthStencilValue(bDefective ? 1 : 0);
		}
	}
}

void AGenesisFactoryAssemblyManager::RotateMappedWheels(AActor* Car, float DeltaDegrees) const
{
	if (!IsValid(Car))
	{
		return;
	}

	TInlineComponentArray<USceneComponent*> Components(Car);
	for (const FName Name : CarVisuals.RotatingWheelComponentNames)
	{
		if (Name.IsNone())
		{
			continue;
		}
		for (USceneComponent* Component : Components)
		{
			if (VisualComponentMatchesName(Component, Name))
			{
				Component->AddLocalRotation(FRotator(DeltaDegrees, 0.0f, 0.0f));
				break;
			}
		}
	}
}

void AGenesisFactoryAssemblyManager::TriggerNoArgFunction(AActor* Target, const FName FunctionName) const
{
	if (!IsValid(Target))
	{
		UE_LOG(LogTemp, Warning, TEXT("GenesisFactory: cannot trigger %s because target is invalid."), *FunctionName.ToString());
		return;
	}

	if (UFunction* Function = Target->FindFunction(FunctionName))
	{
		TArray<uint8> Params;
		Params.SetNumZeroed(Function->ParmsSize);
		Target->ProcessEvent(Function, Params.GetData());
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("GenesisFactory: %s does not implement function/event %s."),
			*GetRuntimeActorLabel(Target),
			*FunctionName.ToString());
	}
}

void AGenesisFactoryAssemblyManager::CacheEquipmentInitialTransform(AActor* Target)
{
	if (!IsValid(Target))
	{
		return;
	}

	const TWeakObjectPtr<AActor> Key(Target);
	if (!EquipmentInitialTransforms.Contains(Key))
	{
		EquipmentInitialTransforms.Add(Key, Target->GetActorTransform());
	}

	TInlineComponentArray<UActorComponent*> Components(Target);
	for (UActorComponent* Component : Components)
	{
		if (IsValid(Component))
		{
			Component->SetComponentTickEnabled(true);
		}
	}
}

void AGenesisFactoryAssemblyManager::SetEquipmentPaused(AActor* Target, bool bPaused) const
{
	if (!IsValid(Target))
	{
		return;
	}

	Target->SetActorTickEnabled(!bPaused);
	Target->CustomTimeDilation = bPaused ? 0.0f : 1.0f;
}

void AGenesisFactoryAssemblyManager::SetEquipmentWaiting(AActor* Target, bool bWaiting)
{
	if (!IsValid(Target))
	{
		return;
	}

	CacheEquipmentInitialTransform(Target);

	if (bWaiting)
	{
		if (const FTransform* InitialTransform = EquipmentInitialTransforms.Find(TWeakObjectPtr<AActor>(Target)))
		{
			Target->SetActorTransform(*InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	Target->SetActorHiddenInGame(false);
	Target->SetActorEnableCollision(true);
	SetEquipmentPaused(Target, bWaiting);

}

void AGenesisFactoryAssemblyManager::SetLineEquipmentPaused(int32 LineIndex, bool bPaused) const
{
	if (!Lines.IsValidIndex(LineIndex))
	{
		return;
	}

	SetEquipmentPaused(Lines[LineIndex].Actors.AGV, bPaused);
	SetEquipmentPaused(Lines[LineIndex].Actors.BatteryLift, bPaused);
	SetEquipmentPaused(Lines[LineIndex].Actors.TireCellRight, bPaused);
	SetEquipmentPaused(Lines[LineIndex].Actors.TireCellLeft, bPaused);
}

void AGenesisFactoryAssemblyManager::SetLineEquipmentWaiting(int32 LineIndex, bool bWaiting)
{
	if (!Lines.IsValidIndex(LineIndex))
	{
		return;
	}

	SetEquipmentWaiting(Lines[LineIndex].Actors.AGV, bWaiting);
	SetEquipmentWaiting(Lines[LineIndex].Actors.BatteryLift, bWaiting);
	SetEquipmentWaiting(Lines[LineIndex].Actors.TireCellRight, bKeepTireCellsRunningBetweenVehicles ? false : bWaiting);
	SetEquipmentWaiting(Lines[LineIndex].Actors.TireCellLeft, bKeepTireCellsRunningBetweenVehicles ? false : bWaiting);
}

void AGenesisFactoryAssemblyManager::SetStationEquipmentWaiting(int32 LineIndex, int32 StationIndex, bool bWaiting)
{
	if (!Lines.IsValidIndex(LineIndex))
	{
		return;
	}

	if (StationIndex == 2)
	{
		SetEquipmentWaiting(Lines[LineIndex].Actors.AGV, bWaiting);
		SetEquipmentWaiting(Lines[LineIndex].Actors.BatteryLift, bWaiting);
	}
	else if (StationIndex == 3)
	{
		SetEquipmentWaiting(Lines[LineIndex].Actors.TireCellRight, bKeepTireCellsRunningBetweenVehicles ? false : bWaiting);
		SetEquipmentWaiting(Lines[LineIndex].Actors.TireCellLeft, bKeepTireCellsRunningBetweenVehicles ? false : bWaiting);
	}
}

void AGenesisFactoryAssemblyManager::EnsureStationEquipmentSpawned(int32 LineIndex, int32 StationIndex)
{
	if (!Lines.IsValidIndex(LineIndex))
	{
		return;
	}

	FGenesisLineActorBindings& Bindings = Lines[LineIndex].Actors;
	if (!bSpawnMissingEquipmentAtRuntime)
	{
		if (StationIndex == 2)
		{
			CacheEquipmentInitialTransform(Bindings.AGV);
			CacheEquipmentInitialTransform(Bindings.BatteryLift);
		}
		else if (StationIndex == 3)
		{
			CacheEquipmentInitialTransform(Bindings.TireCellRight);
			CacheEquipmentInitialTransform(Bindings.TireCellLeft);
		}
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (StationIndex == 2)
	{
		if (!IsValid(Bindings.AGV) && AGVClass)
		{
			Bindings.AGV = GetWorld()->SpawnActor<AActor>(
				AGVClass,
				GetEquipmentSpawnTransform(LineIndex, TEXT("AGV")),
				SpawnParameters);
		}
		if (!IsValid(Bindings.BatteryLift) && BatteryLiftClass)
		{
			Bindings.BatteryLift = GetWorld()->SpawnActor<AActor>(
				BatteryLiftClass,
				GetEquipmentSpawnTransform(LineIndex, TEXT("BatteryLift")),
				SpawnParameters);
		}
		return;
	}

	if (StationIndex == 3)
	{
		if (!IsValid(Bindings.TireCellRight) && TireCellRightClass)
		{
			Bindings.TireCellRight = GetWorld()->SpawnActor<AActor>(
				TireCellRightClass,
				GetEquipmentSpawnTransform(LineIndex, TEXT("TireRight")),
				SpawnParameters);
		}
		if (!IsValid(Bindings.TireCellLeft) && TireCellLeftClass)
		{
			Bindings.TireCellLeft = GetWorld()->SpawnActor<AActor>(
				TireCellLeftClass,
				GetEquipmentSpawnTransform(LineIndex, TEXT("TireLeft")),
				SpawnParameters);
		}
	}
}

void AGenesisFactoryAssemblyManager::DestroyStationEquipment(int32 LineIndex, int32 StationIndex)
{
	if (!Lines.IsValidIndex(LineIndex))
	{
		return;
	}

	SetStationEquipmentWaiting(LineIndex, StationIndex, true);
}

FTransform AGenesisFactoryAssemblyManager::GetEquipmentSpawnTransform(int32 LineIndex, const FString& EquipmentType) const
{
	const int32 SafeIndex = FMath::Clamp(LineIndex, 0, 2);
	static const FVector AgvLocations[3] = {
		FVector(-3030.0f, 6690.0f, 0.0f),
		FVector(-3030.0f, 6370.0f, 0.0f),
		FVector(-3030.0f, 6040.0f, 0.0f)
	};
	static const FVector BatteryLiftLocations[3] = {
		FVector(8020.0f, 1620.0f, 90.0f),
		FVector(8020.0f, 6380.0f, 90.0f),
		FVector(8020.0f, 11200.0f, 90.0f)
	};
	static const FVector TireRightLocations[3] = {
		FVector(23900.0f, 3180.0f, 0.0f),
		FVector(23900.0f, 7990.0f, 0.0f),
		FVector(23900.0f, 12780.0f, 0.0f)
	};
	static const FVector TireLeftLocations[3] = {
		FVector(23930.0f, 0.0f, 0.0f),
		FVector(23930.0f, 4810.0f, 0.0f),
		FVector(23930.0f, 9600.0f, 0.0f)
	};

	if (EquipmentType == TEXT("AGV"))
	{
		return FTransform(FRotator::ZeroRotator, AgvLocations[SafeIndex]);
	}
	if (EquipmentType == TEXT("BatteryLift"))
	{
		return FTransform(FRotator::ZeroRotator, BatteryLiftLocations[SafeIndex]);
	}
	const FRotator TireRotation(0.0f, 180.0f, 0.0f);
	if (EquipmentType == TEXT("TireRight"))
	{
		return FTransform(TireRotation, TireRightLocations[SafeIndex]);
	}
	if (EquipmentType == TEXT("TireLeft"))
	{
		return FTransform(TireRotation, TireLeftLocations[SafeIndex]);
	}

	return FTransform(FRotator::ZeroRotator, FVector::ZeroVector);
}

void AGenesisFactoryAssemblyManager::CreateOrFindStationRobots(int32 LineIndex, int32 StationIndex)
{
	if (bUsePreplacedStationRobotsOnly)
	{
		return;
	}

	if (!LeftRobotMesh || !Lines.IsValidIndex(LineIndex) || !StationX.IsValidIndex(StationIndex))
	{
		return;
	}

	const FVector LeftLocation(StationX[StationIndex], Lines[LineIndex].CenterY - RobotSideOffset, 0.0f);
	for (TActorIterator<ASkeletalMeshActor> It(GetWorld()); It; ++It)
	{
		if (FVector::DistSquared(It->GetActorLocation(), LeftLocation) < FMath::Square(100.0f))
		{
			return;
		}
	}

	ASkeletalMeshActor* Robot = GetWorld()->SpawnActor<ASkeletalMeshActor>(LeftLocation, FRotator(0.0f, 90.0f, 0.0f));
	if (IsValid(Robot))
	{
		Robot->GetSkeletalMeshComponent()->SetSkeletalMesh(LeftRobotMesh);
		Robot->Tags.Add(FName(*FString::Printf(TEXT("GenesisLeftRobot_L%d_S%d"), Lines[LineIndex].LineId, StationIndex)));
	}
}

void AGenesisFactoryAssemblyManager::PlayStationRobots(int32 LineIndex, int32 StationIndex)
{
	if (!Lines.IsValidIndex(LineIndex) || !StationX.IsValidIndex(StationIndex))
	{
		return;
	}

	if (!bUsePreplacedStationRobotsOnly)
	{
		CreateOrFindStationRobots(LineIndex, StationIndex);
	}

	ASkeletalMeshActor* LeftRobot = FindNamedStationRobot(LineIndex, StationIndex, TEXT("Left"));
	ASkeletalMeshActor* RightRobot = FindNamedStationRobot(LineIndex, StationIndex, TEXT("Right"));

	if (USkeletalMeshComponent* Mesh = IsValid(LeftRobot) ? LeftRobot->GetSkeletalMeshComponent() : nullptr)
	{
		if (LeftRobotAnimation)
		{
			Mesh->PlayAnimation(LeftRobotAnimation, false);
		}
		else
		{
			Mesh->Play(false);
		}
	}

	if (USkeletalMeshComponent* Mesh = IsValid(RightRobot) ? RightRobot->GetSkeletalMeshComponent() : nullptr)
	{
		if (RightRobotAnimation)
		{
			Mesh->PlayAnimation(RightRobotAnimation, false);
		}
		else
		{
			Mesh->Play(false);
		}
	}
}

void AGenesisFactoryAssemblyManager::StopPreplacedStationRobotAnimations(int32 LineIndex)
{
	if (!Lines.IsValidIndex(LineIndex))
	{
		return;
	}

	for (const int32 StationIndex : {0, 1, 2, 4})
	{
		StopStationRobots(LineIndex, StationIndex);
	}
}

void AGenesisFactoryAssemblyManager::StopStationRobots(int32 LineIndex, int32 StationIndex)
{
	if (!Lines.IsValidIndex(LineIndex))
	{
		return;
	}

	for (const FString& Side : {FString(TEXT("Left")), FString(TEXT("Right"))})
	{
		ASkeletalMeshActor* Robot = FindNamedStationRobot(LineIndex, StationIndex, Side);
		if (USkeletalMeshComponent* Mesh = IsValid(Robot) ? Robot->GetSkeletalMeshComponent() : nullptr)
		{
			Mesh->Stop();
		}
	}
}

ASkeletalMeshActor* AGenesisFactoryAssemblyManager::FindNamedStationRobot(int32 LineIndex, int32 StationIndex, const FString& Side) const
{
	if (!Lines.IsValidIndex(LineIndex))
	{
		return nullptr;
	}

	const int32 LineId = Lines[LineIndex].LineId;
	const int32 ProcessNumber = StationIndex + 1;
	const FString ExpectedLabel = FString::Printf(TEXT("%sRobot%d_%d"), *Side, LineId, ProcessNumber);
	const FString CompactExpected = CompactRuntimeName(ExpectedLabel);

	for (TActorIterator<ASkeletalMeshActor> It(GetWorld()); It; ++It)
	{
		ASkeletalMeshActor* Robot = *It;
		if (!IsValid(Robot))
		{
			continue;
		}

		const FString CompactLabel = CompactRuntimeName(GetRuntimeActorLabel(Robot));
		const FString CompactName = CompactRuntimeName(Robot->GetName());
		if (CompactLabel.Equals(CompactExpected, ESearchCase::IgnoreCase) ||
			CompactName.Equals(CompactExpected, ESearchCase::IgnoreCase) ||
			CompactName.StartsWith(CompactExpected, ESearchCase::IgnoreCase))
		{
			return Robot;
		}
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("GenesisFactory: station robot not found: %s"),
		*ExpectedLabel);
	return nullptr;
}

void AGenesisFactoryAssemblyManager::UpdateBottleneck(int32 LineIndex)
{
	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	const double Now = GetWorld()->GetTimeSeconds();

	for (int32 StationIndex = 1; StationIndex < 6; ++StationIndex)
	{
		const bool bFull = StationIndex < ProcessStationCount
			? Runtime.StationQueues[StationIndex].Num() >= BufferCapacity
			: Runtime.InspectionVehicles.Num() >= BufferCapacity;
		if (bFull && Runtime.BufferFullSince[StationIndex] < 0.0)
		{
			Runtime.BufferFullSince[StationIndex] = Now;
		}
		else if (!bFull)
		{
			Runtime.BufferFullSince[StationIndex] = -1.0;
		}
	}

	Runtime.BottleneckStationIndex = INDEX_NONE;
	double OldestFullTime = TNumericLimits<double>::Max();
	for (int32 StationIndex = 1; StationIndex < 6; ++StationIndex)
	{
		const double FullSince = Runtime.BufferFullSince[StationIndex];
		if (FullSince >= 0.0 && FullSince < OldestFullTime)
		{
			OldestFullTime = FullSince;
			Runtime.BottleneckStationIndex = StationIndex;
		}
	}
}

void AGenesisFactoryAssemblyManager::CreateBoards(int32 LineIndex)
{
	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	Runtime.StationBoards.Reset();
	const int32 LineId = Lines[LineIndex].LineId;

	for (int32 StationIndex = 0; StationIndex < ProcessStationCount; ++StationIndex)
	{
		AActor* Board = FindPreplacedStatusBoard(LineId, StationIndex);
		ConfigureStatusBoardActor(Board, false);
		if (IsValid(Board))
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("GenesisFactory: Status board mapping Line %d Station %d -> %s (%s)"),
				LineId,
				StationIndex + 1,
				*GetRuntimeActorLabel(Board),
				*Board->GetClass()->GetPathName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GenesisFactory: Status board mapping Line %d Station %d -> NOT FOUND"), LineId, StationIndex + 1);
		}
		Runtime.StationBoards.Add(Board);
	}

	Runtime.LineBoard = FindPreplacedLineBoard(LineId);
	ConfigureStatusBoardActor(Runtime.LineBoard, true);
	if (IsValid(Runtime.LineBoard))
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("GenesisFactory: Line board mapping Line %d -> %s (%s)"),
			LineId,
			*GetRuntimeActorLabel(Runtime.LineBoard),
			*Runtime.LineBoard->GetClass()->GetPathName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GenesisFactory: Line board mapping Line %d -> NOT FOUND"), LineId);
	}

	FixKnownPreplacedBoardTransforms(Runtime, LineId);
}

void AGenesisFactoryAssemblyManager::FixKnownPreplacedBoardTransforms(FGenesisAssemblyLineRuntime& Runtime, int32 LineId)
{
	if (LineId != 2 || Runtime.StationBoards.Num() <= 4)
	{
		return;
	}

	AActor* Board23 = Runtime.StationBoards.IsValidIndex(2) ? Runtime.StationBoards[2].Get() : nullptr;
	AActor* Board24 = Runtime.StationBoards.IsValidIndex(3) ? Runtime.StationBoards[3].Get() : nullptr;
	AActor* Board25 = Runtime.StationBoards.IsValidIndex(4) ? Runtime.StationBoards[4].Get() : nullptr;
	if (!IsValid(Board23) || !IsValid(Board24) || !IsValid(Board25))
	{
		return;
	}

	const FVector TargetLocation = (Board23->GetActorLocation() + Board25->GetActorLocation()) * 0.5f;
	const float DistanceFromExpected = FVector::Dist(Board24->GetActorLocation(), TargetLocation);
	if (DistanceFromExpected > 50.0f)
	{
		Board24->SetActorLocation(TargetLocation);
		Board24->SetActorRotation(Board23->GetActorRotation());
		UE_LOG(LogTemp, Display, TEXT("GenesisFactory: Corrected StatusBoard_2_4 location to midpoint between 2_3 and 2_5."));
	}
}

void AGenesisFactoryAssemblyManager::FixKnownPreplacedBoardTransformsByLabels() const
{
	AActor* Board23 = FindPreplacedStatusBoard(2, 2);
	AActor* Board24 = FindPreplacedStatusBoard(2, 3);
	AActor* Board25 = FindPreplacedStatusBoard(2, 4);
	if (!IsValid(Board23) || !IsValid(Board24) || !IsValid(Board25))
	{
		return;
	}

	const FVector TargetLocation = (Board23->GetActorLocation() + Board25->GetActorLocation()) * 0.5f;
	const float DistanceFromExpected = FVector::Dist(Board24->GetActorLocation(), TargetLocation);
	if (DistanceFromExpected > 50.0f)
	{
		Board24->Modify();
		Board24->SetActorLocation(TargetLocation);
		Board24->SetActorRotation(Board23->GetActorRotation());
	}
}

AActor* AGenesisFactoryAssemblyManager::FindPreplacedStatusBoard(int32 LineId, int32 StationIndex) const
{
	const TArray<FString> PreferredCandidates = {
		FString::Printf(TEXT("GenesisFactoryStatusBoard_%d_%d"), LineId, StationIndex + 1),
		FString::Printf(TEXT("GenesisFactoryStatusBoard_%02d_%02d"), LineId, StationIndex + 1),
		FString::Printf(TEXT("StatusBoard_%d_%d"), LineId, StationIndex + 1),
		FString::Printf(TEXT("StatusBoard_%02d_%02d"), LineId, StationIndex + 1),
	};

	if (AActor* Board = FindPreplacedBoardByCandidates(PreferredCandidates))
	{
		return Board;
	}

	if (StationIndex > 0)
	{
		const TArray<FString> LegacyZeroBasedCandidates = {
			FString::Printf(TEXT("GenesisFactoryStatusBoard_%d_%d"), LineId, StationIndex),
			FString::Printf(TEXT("GenesisFactoryStatusBoard_%02d_%02d"), LineId, StationIndex),
			FString::Printf(TEXT("StatusBoard_%d_%d"), LineId, StationIndex),
			FString::Printf(TEXT("StatusBoard_%02d_%02d"), LineId, StationIndex),
		};
		return FindPreplacedBoardByCandidates(LegacyZeroBasedCandidates);
	}

	return nullptr;
}

AActor* AGenesisFactoryAssemblyManager::FindPreplacedLineBoard(int32 LineId) const
{
	const TArray<FString> Candidates = {
		FString::Printf(TEXT("GenesisFactoryStatusBoard_%d_line"), LineId),
		FString::Printf(TEXT("GenesisFactoryStatusBoard_%02d_line"), LineId),
		FString::Printf(TEXT("%d_line"), LineId),
		FString::Printf(TEXT("%02d_line"), LineId),
	};

	return FindPreplacedBoardByCandidates(Candidates);
}

AActor* AGenesisFactoryAssemblyManager::FindPreplacedBoardByCandidates(const TArray<FString>& Candidates) const
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Board = *It;
		if (!IsValid(Board))
		{
			continue;
		}

		const FString Label = GetRuntimeActorLabel(Board);
		const FString Name = Board->GetName();
		for (const FString& Candidate : Candidates)
		{
			if (Label.Equals(Candidate, ESearchCase::IgnoreCase) ||
				Label.StartsWith(Candidate + TEXT("_"), ESearchCase::IgnoreCase) ||
				Label.Contains(Candidate, ESearchCase::IgnoreCase) ||
				Name.Equals(Candidate, ESearchCase::IgnoreCase) ||
				Name.StartsWith(Candidate + TEXT("_"), ESearchCase::IgnoreCase) ||
				Name.Contains(Candidate, ESearchCase::IgnoreCase))
			{
				return Board;
			}
		}
	}

	return nullptr;
}

void AGenesisFactoryAssemblyManager::ConfigureStatusBoardActor(AActor* Board, bool bLineBoard) const
{
	if (!IsValid(Board))
	{
		return;
	}

	if (AGenesisFactoryStatusBoard* NativeBoard = Cast<AGenesisFactoryStatusBoard>(Board))
	{
		if (bLineBoard)
		{
			NativeBoard->ConfigureAsLineBoard();
		}
		else
		{
			NativeBoard->ConfigureAsStationBoard();
		}
		return;
	}

	TArray<UWidgetComponent*> WidgetComponents;
	Board->GetComponents<UWidgetComponent>(WidgetComponents);
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!IsValid(WidgetComponent))
		{
			continue;
		}

		if (WidgetComponent->GetName().Contains(TEXT("MonitorWidget"), ESearchCase::IgnoreCase))
		{
			WidgetComponent->SetHiddenInGame(false);
			WidgetComponent->SetVisibility(true, true);
			WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
			WidgetComponent->SetDrawSize(FVector2D(1280.0f, 720.0f));
			WidgetComponent->SetTwoSided(true);
			WidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
			WidgetComponent->SetWidget(nullptr);
			WidgetComponent->SetWidgetClass(UGenesisFactoryStatusWidget::StaticClass());
			WidgetComponent->InitWidget();
			TrySetWidgetComponentText(WidgetComponent, TEXT("WAITING FOR FACTORY DATA"), FLinearColor(0.05f, 1.0f, 0.15f));
			return;
		}
	}
}

void AGenesisFactoryAssemblyManager::SetStatusBoardText(AActor* Board, const FString& Value, const FLinearColor& Color) const
{
	if (!IsValid(Board))
	{
		return;
	}

	if (AGenesisFactoryStatusBoard* NativeBoard = Cast<AGenesisFactoryStatusBoard>(Board))
	{
		// Native boards are updated through their typed public methods, not this fallback.
		return;
	}

	TArray<UWidgetComponent*> WidgetComponents;
	Board->GetComponents<UWidgetComponent>(WidgetComponents);
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!IsValid(WidgetComponent) ||
			!WidgetComponent->GetName().Contains(TEXT("MonitorWidget"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (!WidgetComponent->GetWidgetClass() ||
			!WidgetComponent->GetWidgetClass()->IsChildOf(UGenesisFactoryStatusWidget::StaticClass()))
		{
			WidgetComponent->SetWidget(nullptr);
			WidgetComponent->SetWidgetClass(UGenesisFactoryStatusWidget::StaticClass());
		}

		WidgetComponent->SetHiddenInGame(false);
		WidgetComponent->SetVisibility(true, true);
		WidgetComponent->InitWidget();

		TrySetWidgetComponentText(WidgetComponent, Value, Color);
		return;
	}
}

bool AGenesisFactoryAssemblyManager::TrySetWidgetComponentText(UWidgetComponent* WidgetComponent, const FString& Value, const FLinearColor& Color) const
{
	if (!IsValid(WidgetComponent))
	{
		return false;
	}

	WidgetComponent->InitWidget();
	UUserWidget* UserWidget = WidgetComponent->GetUserWidgetObject();
	if (!IsValid(UserWidget))
	{
		return false;
	}

	if (UGenesisFactoryStatusWidget* StatusWidget = Cast<UGenesisFactoryStatusWidget>(UserWidget))
	{
		StatusWidget->SetStatusText(Value, Color);
		return true;
	}

	if (!UserWidget->WidgetTree)
	{
		return false;
	}

	TArray<UWidget*> Widgets;
	UserWidget->WidgetTree->GetAllWidgets(Widgets);
	bool bUpdatedAnyText = false;
	for (UWidget* Widget : Widgets)
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			TextBlock->SetText(FText::FromString(Value));
			TextBlock->SetColorAndOpacity(Color);
			FSlateFontInfo Font = TextBlock->GetFont();
			Font.Size = 24;
			TextBlock->SetFont(Font);
			bUpdatedAnyText = true;
		}
	}

	return bUpdatedAnyText;
}

void AGenesisFactoryAssemblyManager::UpdateBoards(int32 LineIndex)
{
	if (bFactoryEndingPlay || !RuntimeLines.IsValidIndex(LineIndex) || !Lines.IsValidIndex(LineIndex))
	{
		return;
	}

	FGenesisAssemblyLineRuntime& Runtime = RuntimeLines[LineIndex];
	for (int32 StationIndex = 0; StationIndex < Runtime.StationBoards.Num(); ++StationIndex)
	{
		if (IsValid(Runtime.StationBoards[StationIndex]) && Lines[LineIndex].Stations.IsValidIndex(StationIndex))
		{
			const FGenesisStationTelemetry& Telemetry = Lines[LineIndex].Stations[StationIndex];
			const int32 BufferCount = StationIndex < ProcessStationCount
				? Runtime.StationQueues[StationIndex].Num()
				: Runtime.InspectionVehicles.Num();
			const bool bBottleneck = Runtime.BottleneckStationIndex == StationIndex;
			const bool bIdle = IsTelemetryIdle(Telemetry);
			AActor* Board = Runtime.StationBoards[StationIndex].Get();
			if (AGenesisFactoryStatusBoard* NativeBoard = Cast<AGenesisFactoryStatusBoard>(Board))
			{
				NativeBoard->SetStationStatus(
					Lines[LineIndex].LineId,
					GetStationDisplayName(StationIndex),
					Telemetry.Utilization,
					Telemetry.DefectRate,
					Telemetry.CycleTime,
					BufferCount,
					bBottleneck,
					bIdle);
			}
			else
			{
				const FString AlertLine = bIdle
					? TEXT("\nIDLE")
					: (bBottleneck ? TEXT("\nBOTTLENECK") : TEXT(""));
				SetStatusBoardText(
					Board,
					FString::Printf(
						TEXT("%s\nUTILIZATION %.1f%%\nDEFECT RATE %.4f%%\nCYCLE TIME %.2fs%s"),
						*GetStationDisplayName(StationIndex),
						Telemetry.Utilization * 100.0f,
						Telemetry.DefectRate * 100.0f,
						Telemetry.CycleTime,
						*AlertLine),
					(bBottleneck || bIdle) ? FLinearColor::Red : FLinearColor(0.05f, 1.0f, 0.15f));
			}
		}
	}

	if (IsValid(Runtime.LineBoard))
	{
		const int32 BottleneckCount = Runtime.BottleneckStationIndex == INDEX_NONE
			? 0
			: (Runtime.BottleneckStationIndex < ProcessStationCount
				? Runtime.StationQueues[Runtime.BottleneckStationIndex].Num()
				: Runtime.InspectionVehicles.Num());
		const FString BottleneckName = Runtime.BottleneckStationIndex == INDEX_NONE ? FString() : GetStationDisplayName(Runtime.BottleneckStationIndex);
		int32 IdleStationIndex = INDEX_NONE;
		for (int32 StationIndex = 0; StationIndex < Lines[LineIndex].Stations.Num(); ++StationIndex)
		{
			if (IsTelemetryIdle(Lines[LineIndex].Stations[StationIndex]))
			{
				IdleStationIndex = StationIndex;
				break;
			}
		}
		const bool bHasIdleStation = IdleStationIndex != INDEX_NONE;
		const FString IdleStationName = bHasIdleStation ? GetStationDisplayName(IdleStationIndex) : FString();
		const FString DisplayBottleneckName = bHasIdleStation
			? IdleStationName
			: BottleneckName;
		const FString ReasonOverride = bHasIdleStation
			? FString::Printf(TEXT("%s MACHINE IS IDLE"), *IdleStationName)
			: FString();
		if (AGenesisFactoryStatusBoard* NativeBoard = Cast<AGenesisFactoryStatusBoard>(Runtime.LineBoard.Get()))
		{
			NativeBoard->SetLineStatusDetailed(
				Lines[LineIndex].LineId,
				Runtime.TotalProduced,
				DisplayBottleneckName,
				BottleneckCount,
				ReasonOverride,
				bHasIdleStation);
		}
		else
		{
			const bool bHasBottleneck = !DisplayBottleneckName.IsEmpty();
			SetStatusBoardText(
				Runtime.LineBoard.Get(),
				FString::Printf(
					TEXT("LINE %d\nPRODUCTION: %d\nBOTTLENECK: %s\nREASON: %s"),
					Lines[LineIndex].LineId,
					Runtime.TotalProduced,
					bHasBottleneck ? *DisplayBottleneckName : TEXT("NONE"),
					!ReasonOverride.IsEmpty()
						? *ReasonOverride
						: (bHasBottleneck
						? *FString::Printf(TEXT("BUFFER FULL %d/2"), BottleneckCount)
						: TEXT("NORMAL FLOW"))),
				(bHasBottleneck || bHasIdleStation)
					? FLinearColor(1.0f, 0.45f, 0.02f)
					: FLinearColor(0.05f, 1.0f, 0.15f));
		}
	}
}

float AGenesisFactoryAssemblyManager::GetMinimumProcessTime(int32 StationIndex) const
{
	if (StationIndex == 2)
	{
		return FMath::Max(
			MinimumStationProcessTimes.IsValidIndex(StationIndex) ? MinimumStationProcessTimes[StationIndex] : 0.0f,
			BatteryAgvLeadTime + BatteryLiftMinimumTime);
	}
	if (StationIndex == 3)
	{
		return FMath::Max(
			MinimumStationProcessTimes.IsValidIndex(StationIndex) ? MinimumStationProcessTimes[StationIndex] : 0.0f,
			TireCellMinimumTime);
	}
	if (StationIndex == 0 || StationIndex == 1 || StationIndex == 4)
	{
		const float AnimationLength = RightRobotAnimation ? RightRobotAnimation->GetPlayLength() : 0.0f;
		return FMath::Max(
			MinimumStationProcessTimes.IsValidIndex(StationIndex) ? MinimumStationProcessTimes[StationIndex] : 0.0f,
			AnimationLength);
	}
	if (StationIndex == 5)
	{
		return FMath::Max(
			MinimumStationProcessTimes.IsValidIndex(StationIndex) ? MinimumStationProcessTimes[StationIndex] : 0.0f,
			InspectionTravelTime);
	}
	return MinimumStationProcessTimes.IsValidIndex(StationIndex) ? MinimumStationProcessTimes[StationIndex] : 0.0f;
}

FVector AGenesisFactoryAssemblyManager::GetStationLocation(int32 LineIndex, int32 StationIndex) const
{
	return GetLineTransformAtDistance(
		LineIndex,
		GetLineDistanceForX(LineIndex, GetVehicleStopX(StationIndex))).GetLocation();
}

FVector AGenesisFactoryAssemblyManager::GetBufferLocation(int32 LineIndex, int32 StationIndex, int32 QueuePosition) const
{
	const float StationPosition = GetVehicleStopX(StationIndex);
	const float PreviousPosition = StationIndex > 0 && StationX.IsValidIndex(StationIndex - 1)
		? GetVehicleStopX(StationIndex - 1)
		: StationPosition - 2400.0f;
	const float Spacing = (StationPosition - PreviousPosition) / 3.0f;
	return GetLineTransformAtDistance(
		LineIndex,
		GetLineDistanceForX(LineIndex, StationPosition - Spacing * (QueuePosition + 1))).GetLocation();
}

float AGenesisFactoryAssemblyManager::GetVehicleStopX(int32 StationIndex) const
{
	if (StationX.IsValidIndex(StationIndex))
	{
		return StationX[StationIndex] + VehicleStopXOffset;
	}

	if (StationX.Num() > 0)
	{
		return StationX[0] + VehicleStopXOffset;
	}

	return VehicleStopXOffset;
}

FVector AGenesisFactoryAssemblyManager::GetUnloadLocation(int32 LineIndex) const
{
	return GetLineTransformAtDistance(LineIndex, GetLineDistanceForX(LineIndex, UnloadX)).GetLocation();
}

FVector AGenesisFactoryAssemblyManager::GetInspectionEndLocation(int32 LineIndex) const
{
	return FVector(InspectionEndX, Lines[LineIndex].CenterY, 140.0f);
}

FTransform AGenesisFactoryAssemblyManager::GetLineTransformAtDistance(int32 LineIndex, float Distance) const
{
	if (!RuntimeLines.IsValidIndex(LineIndex) ||
		!IsValid(RuntimeLines[LineIndex].HangerLine) ||
		!RuntimeLines[LineIndex].HangerLine->GetLineSpline())
	{
		return FTransform(FRotator::ZeroRotator, FVector(0.0f, Lines.IsValidIndex(LineIndex) ? Lines[LineIndex].CenterY : 0.0f, HangerRailHeight));
	}

	USplineComponent* Spline = RuntimeLines[LineIndex].HangerLine->GetLineSpline();
	const float Length = Spline->GetSplineLength();
	const float WrappedDistance = Length > 0.0f ? FMath::Fmod(Distance, Length) : 0.0f;
	return Spline->GetTransformAtDistanceAlongSpline(WrappedDistance, ESplineCoordinateSpace::World, true);
}

float AGenesisFactoryAssemblyManager::GetLineDistanceForX(int32 LineIndex, float X) const
{
	if (!RuntimeLines.IsValidIndex(LineIndex) || !IsValid(RuntimeLines[LineIndex].HangerLine))
	{
		return 0.0f;
	}
	return RuntimeLines[LineIndex].HangerLine->GetForwardDistanceAtX(X);
}

FString AGenesisFactoryAssemblyManager::GetStationDisplayName(int32 StationIndex) const
{
	static const TArray<FString> Names = {
		TEXT("DOOR REMOVAL"),
		TEXT("LIGHT INSTALL"),
		TEXT("BATTERY INSTALL"),
		TEXT("WHEEL / TIRE"),
		TEXT("DOOR & SEAT"),
		TEXT("INSPECTION")
	};
	return Names.IsValidIndex(StationIndex) ? Names[StationIndex] : TEXT("UNKNOWN");
}

int32 AGenesisFactoryAssemblyManager::GetLineProductionCount(int32 LineId) const
{
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		if (Lines[Index].LineId == LineId && RuntimeLines.IsValidIndex(Index))
		{
			return RuntimeLines[Index].TotalProduced;
		}
	}
	return 0;
}

EGenesisAssemblyStation AGenesisFactoryAssemblyManager::GetLineBottleneck(int32 LineId, bool& bHasBottleneck) const
{
	bHasBottleneck = false;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		if (Lines[Index].LineId == LineId && RuntimeLines.IsValidIndex(Index))
		{
			const int32 StationIndex = RuntimeLines[Index].BottleneckStationIndex;
			bHasBottleneck = StationIndex != INDEX_NONE;
			return bHasBottleneck ? static_cast<EGenesisAssemblyStation>(StationIndex) : EGenesisAssemblyStation::DoorRemoval;
		}
	}
	return EGenesisAssemblyStation::DoorRemoval;
}

void AGenesisFactoryAssemblyManager::BindMqtt()
{
	for (TActorIterator<APaho_Manager_Sync> It(GetWorld()); It; ++It)
	{
		MqttManager = *It;
		break;
	}
	if (!IsValid(MqttManager))
	{
		MqttManager = GetWorld()->SpawnActor<APaho_Manager_Sync>();
	}
	if (!IsValid(MqttManager))
	{
		return;
	}

	MqttManager->Delegate_Message_Arrived.AddUniqueDynamic(this, &AGenesisFactoryAssemblyManager::HandleMqttMessage);
	FPahoClientParams Params;
	Params.Address = BrokerAddress;
	Params.ClientId = FString::Printf(TEXT("genesis_factory_%d"), FMath::RandRange(1000, 999999));
	Params.UserName = UserName;
	Params.Password = Password;
	Params.KeepAliveInterval = 20;
	Params.Version = EMQTTVERSION::V3_1_1;

	FDelegate_Paho_Connection ConnectionDelegate;
	ConnectionDelegate.BindDynamic(this, &AGenesisFactoryAssemblyManager::HandleMqttConnected);
	MqttManager->MQTT_Sync_Init(ConnectionDelegate, Params);
}

void AGenesisFactoryAssemblyManager::HandleMqttConnected(bool bSuccess, FJsonObjectWrapper Result)
{
	if (bFactoryEndingPlay || !bSuccess || !IsValid(MqttManager))
	{
		return;
	}
	FJsonObjectWrapper OutCode;
	OutCode.JsonObject = MakeShared<FJsonObject>();
	MqttManager->MQTT_Sync_Subscribe(OutCode, SubscribeTopic, EMQTTQOS::QoS_0);
}

void AGenesisFactoryAssemblyManager::HandleMqttMessage(FJsonObjectWrapper Message)
{
	if (bFactoryEndingPlay || !IsValid(this))
	{
		return;
	}

	if (!Message.JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("GenesisFactory: MQTT delegate received an invalid message wrapper."));
		return;
	}

	const TSharedPtr<FJsonObject>* Payload = nullptr;
	if (Message.JsonObject->TryGetObjectField(TEXT("Message"), Payload) && Payload && Payload->IsValid())
	{
		UE_LOG(LogTemp, Display, TEXT("GenesisFactory: MQTT delegate received object payload."));
		ApplyPayloadObject(*Payload);
		return;
	}

	FString PayloadString;
	if (Message.JsonObject->TryGetStringField(TEXT("Message"), PayloadString))
	{
		UE_LOG(LogTemp, Display, TEXT("GenesisFactory: MQTT delegate received string payload."));
		ApplyMqttPayloadJson(PayloadString);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("GenesisFactory: MQTT delegate received a message without a Message payload field."));
}

void AGenesisFactoryAssemblyManager::ApplyMqttPayloadJson(const FString& Payload)
{
	if (bFactoryEndingPlay)
	{
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
	{
		ApplyPayloadObject(Root);
	}
}

void AGenesisFactoryAssemblyManager::ApplyPayloadObject(const TSharedPtr<FJsonObject>& Root)
{
	if (bFactoryEndingPlay)
	{
		return;
	}

	if (!Root.IsValid())
	{
		return;
	}

	TArray<TSharedPtr<FJsonObject>> LineObjects;
	const TArray<TSharedPtr<FJsonValue>>* LinesArray = nullptr;
	if (Root->TryGetArrayField(TEXT("lines"), LinesArray) && LinesArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *LinesArray)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Object) && Object && Object->IsValid())
			{
				LineObjects.Add(*Object);
			}
		}
	}
	else
	{
		LineObjects.Add(Root);
	}

	for (const TSharedPtr<FJsonObject>& LineObject : LineObjects)
	{
		const int32 LineId = LineIdToInt(LineObject);
		const int32 FoundLineIndex = Lines.IndexOfByPredicate([LineId](const FGenesisAssemblyLineConfig& Line)
		{
			return Line.LineId == LineId;
		});
		int32 ResolvedLineIndex = FoundLineIndex;
		if (!Lines.IsValidIndex(ResolvedLineIndex) && Lines.IsValidIndex(LineId - 1))
		{
			ResolvedLineIndex = LineId - 1;
			Lines[ResolvedLineIndex].LineId = LineId;
			if (RuntimeLines.IsValidIndex(ResolvedLineIndex))
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("GenesisFactory: MQTT line_id %d matched by array index fallback. Check the Manager Lines array if this was unexpected."),
					LineId);
			}
		}
		if (!Lines.IsValidIndex(ResolvedLineIndex))
		{
			FString RawLineIdForLog;
			TryGetJsonFieldAsString(LineObject, TEXT("line_id"), RawLineIdForLog);
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("GenesisFactory: MQTT payload ignored because parsed line_id %d from raw '%s' did not match any configured line."),
				LineId,
				*RawLineIdForLog);
			continue;
		}
		const int32 LineIndex = ResolvedLineIndex;

		bool bResetLineRuntimeStats = false;
		LineObject->TryGetBoolField(TEXT("reset_flow"), bResetLineRuntimeStats);
		const TSharedPtr<FJsonObject>* LineStateObject = nullptr;
		if (LineObject->TryGetObjectField(TEXT("line"), LineStateObject) && LineStateObject && LineStateObject->IsValid())
		{
			bool bNestedResetFlow = false;
			if ((*LineStateObject)->TryGetBoolField(TEXT("reset_flow"), bNestedResetFlow))
			{
				bResetLineRuntimeStats = bResetLineRuntimeStats || bNestedResetFlow;
			}
		}
		if (bResetLineRuntimeStats && RuntimeLines.IsValidIndex(LineIndex))
		{
			RuntimeLines[LineIndex].TotalProduced = 0;
			RuntimeLines[LineIndex].BottleneckStationIndex = INDEX_NONE;
		}

		const TArray<TSharedPtr<FJsonValue>>* StationsArray = nullptr;
		if (!LineObject->TryGetArrayField(TEXT("stations"), StationsArray) || !StationsArray)
		{
			UE_LOG(LogTemp, Warning, TEXT("GenesisFactory: MQTT payload for Line %d has no stations array."), LineId);
			continue;
		}

		int32 AppliedStationCount = 0;
		for (const TSharedPtr<FJsonValue>& Value : *StationsArray)
		{
			const TSharedPtr<FJsonObject>* StationObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(StationObject) || !StationObject || !StationObject->IsValid())
			{
				continue;
			}

			FString StationId;
			(*StationObject)->TryGetStringField(TEXT("station_id"), StationId);
			if (StationId.IsEmpty())
			{
				(*StationObject)->TryGetStringField(TEXT("process_id"), StationId);
			}
			const int32 StationIndex = StationIdToIndex(StationId);
			if (!Lines[LineIndex].Stations.IsValidIndex(StationIndex))
			{
				UE_LOG(LogTemp, Warning, TEXT("GenesisFactory: MQTT station id '%s' did not map to a configured station."), *StationId);
				continue;
			}

			FGenesisStationTelemetry& Telemetry = Lines[LineIndex].Stations[StationIndex];
			double Number = 0.0;
			bool bReceivedCycleTime = false;
			bool bCycleTimeMeansIdle = false;
			if ((*StationObject)->TryGetNumberField(TEXT("cycle_time"), Number))
			{
				Telemetry.CycleTime = FMath::Clamp(static_cast<float>(Number), 0.5f, MqttCycleTimeMax);
				Telemetry.bCycleTimeFromMqtt = true;
				bReceivedCycleTime = true;
				bCycleTimeMeansIdle = Telemetry.CycleTime >= 998.5f;
			}
			if ((*StationObject)->TryGetNumberField(TEXT("utilization"), Number))
			{
				Telemetry.Utilization = FMath::Clamp(static_cast<float>(Number), 0.0f, 1.0f);
			}
			if ((*StationObject)->TryGetNumberField(TEXT("defect_rate"), Number))
			{
				Telemetry.DefectRate = FMath::Clamp(static_cast<float>(Number), 0.0f, 1.0f);
			}

			FString Status;
			bool bReceivedRunStatus = false;
			auto ApplyIdleTransition = [this, LineIndex, StationIndex]()
			{
				StopStationRobots(LineIndex, StationIndex);
				if (StationIndex == 2 || StationIndex == 3)
				{
					SetStationEquipmentWaiting(LineIndex, StationIndex, true);
				}
				if (RuntimeLines.IsValidIndex(LineIndex) && RuntimeLines[LineIndex].StationOccupants.IsValidIndex(StationIndex))
				{
					const int32 OccupantIndex = RuntimeLines[LineIndex].StationOccupants[StationIndex];
					if (RuntimeLines[LineIndex].Vehicles.IsValidIndex(OccupantIndex))
					{
						RuntimeLines[LineIndex].Vehicles[OccupantIndex].bStationWorkStarted = false;
					}
				}
			};
			if ((*StationObject)->TryGetStringField(TEXT("run_status"), Status))
			{
				bReceivedRunStatus = true;
				const bool bNewRunning = Status.Equals(TEXT("RUN"), ESearchCase::IgnoreCase);
				const bool bWasRunning = Telemetry.bRunning;
				Telemetry.bRunning = bNewRunning;
				if (!bNewRunning)
				{
					ApplyIdleTransition();
				}
				else if (!bWasRunning && RuntimeLines.IsValidIndex(LineIndex) && RuntimeLines[LineIndex].StationOccupants.IsValidIndex(StationIndex))
				{
					const int32 OccupantIndex = RuntimeLines[LineIndex].StationOccupants[StationIndex];
					if (RuntimeLines[LineIndex].Vehicles.IsValidIndex(OccupantIndex))
					{
						RuntimeLines[LineIndex].Vehicles[OccupantIndex].bStationWorkStarted = false;
					}
				}
			}
			if (bReceivedCycleTime)
			{
				if (bCycleTimeMeansIdle)
				{
					Telemetry.bRunning = false;
					ApplyIdleTransition();
				}
				else if (!bReceivedRunStatus)
				{
					Telemetry.bRunning = true;
				}
			}

			if (bUseMqttCycleTimeAsAuthoritative && bReceivedCycleTime && RuntimeLines.IsValidIndex(LineIndex) &&
				RuntimeLines[LineIndex].StationOccupants.IsValidIndex(StationIndex))
			{
				const int32 OccupantIndex = RuntimeLines[LineIndex].StationOccupants[StationIndex];
				if (RuntimeLines[LineIndex].Vehicles.IsValidIndex(OccupantIndex))
				{
					FGenesisVehicleRuntime& Vehicle = RuntimeLines[LineIndex].Vehicles[OccupantIndex];
					if (Vehicle.State == EGenesisVehicleFlowState::Processing && Vehicle.StationIndex == StationIndex)
					{
						Vehicle.RemainingProcessTime = FMath::Min(Vehicle.RemainingProcessTime, Telemetry.CycleTime);
					}
				}
			}

			++AppliedStationCount;
		}

		if (RuntimeLines.IsValidIndex(LineIndex))
		{
			UpdateBoards(LineIndex);
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("GenesisFactory: Applied MQTT Line %d, stations=%d, doorCycle=%.2f, lightCycle=%.2f, batteryCycle=%.2f, wheelCycle=%.2f, doorSeatCycle=%.2f."),
			LineId,
			AppliedStationCount,
			Lines[LineIndex].Stations.IsValidIndex(0) ? Lines[LineIndex].Stations[0].CycleTime : -1.0f,
			Lines[LineIndex].Stations.IsValidIndex(1) ? Lines[LineIndex].Stations[1].CycleTime : -1.0f,
			Lines[LineIndex].Stations.IsValidIndex(2) ? Lines[LineIndex].Stations[2].CycleTime : -1.0f,
			Lines[LineIndex].Stations.IsValidIndex(3) ? Lines[LineIndex].Stations[3].CycleTime : -1.0f,
			Lines[LineIndex].Stations.IsValidIndex(4) ? Lines[LineIndex].Stations[4].CycleTime : -1.0f);
	}
}
