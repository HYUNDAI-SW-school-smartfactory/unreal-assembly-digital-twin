# Genesis Digital Twin Blueprint 분석 및 C++ 전환 평가

## 결론

이 프로젝트의 핵심 목표는 **Genesis 차량 최종 조립 라인을 시각화하고, 행거·로봇·타이어·배터리 공정을 MQTT 설비 상태와 동기화하는 디지털 트윈**이다.

모든 실행 로직을 C++로 옮기는 것은 가능하다. 다만 메시, 위젯 레이아웃, 애니메이션, 타임라인과 같은 콘텐츠까지 `.uasset` 없이 순수 C++로 만드는 것은 효율적이지 않다. 권장 목표는 다음과 같다.

- 공정 상태, MQTT, 스폰, 큐, 이동, 상호작용 규칙: C++
- 메시·재질·애니메이션·위젯 배치와 튜닝 값: C++ 기반의 Data-Only Blueprint 또는 Data Asset
- 레벨에 배치된 인스턴스와 참조: 기존 Blueprint를 단계적으로 C++ 기반 클래스로 Reparent

## 현재 확인된 기술 상태

- Unreal Engine 5.7
- Asset Registry에서 확인된 Blueprint/Widget Blueprint: 80개
- 플러그인의 상속형 MQTT 템플릿 3개를 포함하면 분석 대상 Blueprint 계열: 83개
- 기존 네이티브 코드:
  - `AGenesisHangerFlowManager`: 행거 이동, 공정 정지/재개, 생산량, MQTT 상태 반영
  - `APaho_Manager_Sync`: MQTT 연결·발행·구독
  - `AMQTT_Machine_Test_Actor`: 설비 상태 시각화
  - `AFactory_Dashboard_Board_Actor`, `UFactory_Dashboard_Widget`: 공장 대시보드
- 현재 `GenesisDigitalTwin` 게임 모듈 DLL이 없으며 Windows SDK 플랫폼 지원도 정상 인식되지 않아 C++ 빌드가 막혀 있다.
- 차량 Blueprint는 구형 `/Script/PhysXVehicles`를 사용한다. UE 5.7 기준 C++ 전환 시 Chaos Vehicles로 먼저 교체하는 편이 안전하다.

## 핵심 공정 Blueprint

| Blueprint | 파악된 목적 | C++ 전환 판단 |
|---|---|---|
| `BP_HangerMovingAlongSpline` | 행거 차량을 스폰하고 스플라인의 공정 포인트 사이로 이동시킨다. 공정 인덱스·상태·로봇 완료 수를 관리하고 `BP_ProcessManager`와 이벤트를 주고받는다. | 최우선. 기존 `AGenesisHangerFlowManager`와 기능이 겹치므로 통합 |
| `BP_ProcessManager` | 현재 공정을 선택하고 행거 도착 수와 로봇 완료 수를 집계해 다음 공정을 시작한다. | 최우선. C++ 상태 머신/공정 오케스트레이터로 전환 |
| `BP_RobotBase` | 공정 번호와 상태를 가진 로봇 공통 기반. 전체 공정 시작 이벤트를 받고 완료 이벤트를 반환한다. | 최우선. `AAssemblyRobotBase` 권장 |
| `BP_HangerVehicle` | 행거에 매달리는 차량/클러치 표현과 부품 Attach·Detach·표시 제어를 담당한다. | 동작은 C++, 메시 구성은 Data-Only BP |
| `E_ProcessState` | 공정 상태 열거형 | `UENUM`으로 전환 |
| `BP_CarMovingAlongSpline` | 차량을 스플라인 거리·회전에 따라 이동시킨다. | 공통 `ASplineTransportActor`로 통합 |
| `BP_CarConveyorLine` | 차량 컨베이어와 배터리 팩 리프트가 포함된 조립 구간을 구성한다. | 로직은 C++, 배치는 Data-Only BP |
| `BP_AGV` | AGV가 배터리 리프트를 참조해 배터리 장착 공정을 시작한다. | C++ AGV 공정 액터 |
| `BP_BatteryLift` | 타임라인으로 배터리 리프트를 승강하고 설치 시퀀스를 실행한다. | C++ Curve/Timeline 또는 상태 기반 보간 |
| `BP_CarAssembly` | Genesis 차량을 여러 메시 부품으로 분해해 조립 상태를 표현한다. | 로직만 C++; 대규모 메시 구성은 BP 유지 |
| `Bp_GENESIS` | Genesis 차량 전체 메시 조합과 재질을 가진 기존 차량 표현이다. | `BP_CarAssembly`와 중복 검토 후 Data-Only 자산으로 정리 |
| `BP_GenesisComplete` | 완성 차량 전체 모델을 담은 표시용 액터다. | Data-Only BP 유지 가능 |
| `BP_Car` (`/Game/Blueprints`) | 다수의 차량 부품 메시로 구성된 차량 액터다. | 조립 데이터 모델로 정리; 순수 C++ 생성은 비권장 |
| `BP_Car` (`Genesis_Model_New`) | 새 차량 모델용 액터이며 기존 `BP_Car`를 참조한다. | 모델 변형용 Data-Only BP |
| `BP_LineVehicle` | 새 차량 모델을 기반으로 조명 공정까지 타임라인 이동한다. | 이동 로직을 공통 C++ 운송 컴포넌트로 통합 |
| `BP_Beam` | 금속 빔 메시와 오버랩 처리가 있는 공장 구조물 액터다. | 로직이 작으면 C++ 불필요; Static Mesh Actor로 단순화 가능 |
| `BP_CutoffArea` | Material Parameter Collection을 갱신해 절단/강조 영역을 시각화한다. | 작은 C++ 시각화 컴포넌트 또는 BP 유지 |
| `BP_FactoryGenerator` | 공장 바닥/설비 배치를 생성하는 절차적 생성 액터다. | 에디터용 C++ 생성기 또는 PCG로 전환 |
| `BP_Spline_Simple` | 스플라인 구간마다 Spline Mesh Component를 생성한다. | 재사용 가능한 C++ spline mesh 액터 |

## 타이어 조립 셀

| Blueprint | 파악된 목적 | C++ 전환 판단 |
|---|---|---|
| `BP_Tire` | 타이어 메시, 충돌 영역, `TireState`를 가진 운반 단위다. | `ATireWorkpiece` |
| `BP_TireConveyor` | 타이어를 스폰하고 스플라인 위로 이동시켜 픽업 위치에 대기시킨다. | 최우선 C++ 전환 |
| `BP_TireTableBuffer` | 컨베이어에서 받은 타이어를 배열에 저장하고 로봇 요청 시 하나를 반환한다. | C++ 큐/버퍼 컴포넌트 |
| `BP_RobotTirePicker` | 컨베이어 타이어를 집어 테이블 버퍼에 놓고 다음 타이어를 요청한다. | C++ 로봇 작업 시퀀스 |
| `BP_RobotTirePickerToCar` | 테이블에서 타이어를 꺼내 차량의 장착 인덱스 위치에 설치한다. | C++ 로봇 작업 시퀀스 |
| `BP_TireAssemblyCell` | 컨베이어·버퍼·두 로봇을 묶고 타이어 공급부터 차량 장착까지 순서를 조정한다. | `ATireAssemblyCell` 오케스트레이터 |
| `BP_TireAssemblyCell_L` | 반대편/좌측 배치용 타이어 셀 변형으로 보인다. | 동일 C++ 클래스 + 방향/소켓 데이터로 통합 |

## MQTT 및 모니터링

| Blueprint | 파악된 목적 | C++ 전환 판단 |
|---|---|---|
| `BP_Template_Paho_Sync` | Paho MQTT 연결, 구독, 수신 및 연결 손실 delegate 사용 예제다. | 플러그인 C++ API 사용 예제로만 보존 |
| `BP_Template_MQTT_Sync` | Paho Sync 템플릿을 상속하는 호환/예제 계층이다. | 사용처가 없으면 제거 후보 |
| `BP_Template_MQTT` | Sync 템플릿을 다시 감싼 예제 계층이다. | 사용처가 없으면 제거 후보 |
| `BP_MQTT_Dijitalis_Sync` | 특정 MQTT/Dijitalis 연동용 파생 템플릿으로 보인다. | 실제 브로커 스키마 확인 후 C++ 서비스로 통합 |
| `BP_MQTT_MachineTest` | 기본 설비 상태 시험 액터. 기본 ID는 C++의 `BODY_INPUT_01`이다. | 이미 C++ 기반 Data-Only BP |
| `BP_MQTT_MachineTest1` | `LIGHT_ASSEMBLY_01` 상태 표시 | Data-Only BP |
| `BP_MQTT_MachineTest2` | `BATTERY_MOUNT_01` 상태 표시 | Data-Only BP |
| `BP_MQTT_MachineTest3` | `WHEEL_ASSEMBLY_01` 상태 표시 | Data-Only BP |
| `BP_MQTT_MachineTest4` | `DOOR_ASSEMBLY_01` 상태 표시 | Data-Only BP |
| `BP_MQTT_MachineTest5` | `INSPECTION_01` 상태 표시 | Data-Only BP |
| `BP_MQTT_MachineTest6` | `OUTPUT_01` 상태 표시 | Data-Only BP |
| `BP_FactoryDisplayBoard` | C++ `AFactory_Dashboard_Board_Actor`의 배치/외형 파생 액터다. | 이미 적절한 C++ 기반 구조 |
| `BP_MQTT_DisplayBoard` | `WBP_MQTT_Monitor`를 공장 전광판 메시 위에 표시한다. | 새 C++ 대시보드와 통합 |
| `WBP_MQTT_Monitor` | AGV 상태, 라인 3개, 속도 텍스트를 표시하는 구형 MQTT UI다. | `UFactory_Dashboard_Widget`와 중복 검토 |

## 범용 상호작용 시스템

이 그룹은 공장 핵심 로직이라기보다 VR/3인칭 체험용 상호작용 프레임워크로 보인다.

| Blueprint | 파악된 목적 | C++ 전환 판단 |
|---|---|---|
| `BPI_GamePlay` | 플레이어와 상호작용 액터 사이의 공통 호출 인터페이스 | `UINTERFACE` |
| `BP_FunctionLibrary` | 차량 탑승/하차, 컴포넌트 검색·제거 등의 공통 함수 | C++ Blueprint Function Library |
| `Virtual_AC_RunAction` | 상호작용 실행 컴포넌트의 부모. 태그와 trace channel, 무시 상태를 관리한다. | C++ 추상 Actor Component |
| `AC_CallAction` | 플레이어 입력으로 대상의 `PreAction`/`CallAction`을 호출하고 단축키 UI를 표시한다. | C++ 상호작용 탐지 컴포넌트 |
| `AC_DoorAction` | 문 열기/닫기와 자동 닫힘 시간을 처리한다. | 공통 C++ 액션 컴포넌트 |
| `AC_Elevator` | 엘리베이터 재생 상태와 재생 속도를 제어한다. | 공통 C++ 액션 컴포넌트 |
| `AC_OnOffLightAction` | 초기 조명 밝기를 저장하고 조명을 켜고 끈다. | C++ |
| `AC_PlayAnimation` | 대상 애니메이션을 실행하고 재생 속도를 제어한다. | C++ |
| `AC_RunTruck` | 트럭 액터의 운행 시작 함수를 호출한다. | 실제 트럭 기능이 필요할 때만 이관 |
| `AC_Shower` | 샤워 설비 상호작용을 실행한다. | 범용 토글 액션으로 통합 |
| `AC_SitInCar` | 플레이어가 차량에 탑승하고 원래 캐릭터로 복귀하게 한다. | C++ possession 시스템 |
| `AC_SitInDrone` | 플레이어가 `PW_Fly` 드론에 탑승/빙의한다. | C++ possession 시스템 |
| `AC_AttachToSocket` | 컴포넌트를 지정 소켓에 부착한다. | 범용 C++ attach 컴포넌트 |
| `AC_AttachParticleToSocket` | 파티클을 Static/Skeletal Mesh 소켓에 부착한다. | 범용 C++ VFX attach 컴포넌트 |
| `AC_LightBroken` | 조명 컴포넌트들을 찾아 고장/깜빡임 효과를 수행한다. | C++ |
| `ML_A_MacroLibrary` | Actor용 지연·보조 매크로 | C++ 유틸리티로 치환 |
| `ML_AC_MacroLibrary` | Actor Component용 지연·조명 액션 매크로 | C++ 유틸리티로 치환 |
| `ML_Ob_MacroLibrary` | Object용 컴포넌트 검색·trace/action 매크로 | C++ 유틸리티로 치환 |
| `AC_TilleArray` | Static Mesh와 재질을 복제해 타일/인스턴스 배열을 만드는 컴포넌트 | C++ HISM 생성 컴포넌트 |
| `A_RemoveNullActor` | 레벨의 불필요하거나 빈 Static/Skeletal Mesh Actor를 찾아 제거한다. | 에디터 전용 C++ 도구로 전환 |
| `GM_Factory` | 공장 체험의 GameMode이며 ThirdPersonCharacter를 사용한다. | C++ GameMode 또는 얇은 Data-Only BP |

## 로봇·운송·효과
ㄱ
| Blueprint | 파악된 목적 | C++ 전환 판단 |
|---|---|---|
| `BP_IndustrialRobot` | 오버랩 시 산업용 로봇 애니메이션과 파티클을 재생한다. | 공정 로봇 기반 클래스와 통합 |
| `BP_RobotHend` | 로봇 손/엔드이펙터의 소켓 선택, IK 방향, 오버랩과 파티클을 제어한다. | C++ 엔드이펙터 컴포넌트 |
| `BP_SplineMovable` | 상호작용으로 스플라인 위 물체를 이동시키며 시작/종료 액션을 제공한다. | 공통 C++ spline movement component |
| `BP_SplineMovable_Blueprint` | 스플라인 이동용 메시·제약 컴포넌트 조합 액터다. | 위 C++ 기반 Data-Only BP |
| `BP_Train` | 열차 메시, 팬, 조명, 연기 파티클을 묶고 동작을 제어한다. | 공장 범위에 필요할 때만 이관 |
| `N_Sparks_ON` | 애니메이션 Notify 시 파티클을 활성화한다. | C++ AnimNotify 또는 Niagara Notify |
| `N_Sparks_OFF` | 애니메이션 Notify 시 파티클을 비활성화한다. | C++ AnimNotify 또는 Niagara Notify |
| `N_On_Off_Particles` | 태그/클래스로 찾은 파티클을 켜거나 끈다. | 위 두 Notify와 하나로 통합 |

## 플레이어·차량·UI

| Blueprint | 파악된 목적 | C++ 전환 판단 |
|---|---|---|
| `ThirdPersonCharacter` | 이동, 시점, 점프, 달리기, Action 입력과 상호작용 컴포넌트를 가진 기본 캐릭터 | C++ Character |
| `PW_Fly` | 드론 비행 캐릭터. 6축 입력, 카메라 Render Target, 드론 UI를 사용한다. | C++ Pawn/Character |
| `PW_Crane` | 크레인 운전 Pawn. 축 입력으로 크레인 애니메이션을 제어하고 전용 UI를 표시한다. | C++ Pawn |
| `PW_ForkLift` | 포크리프트 조향·가속·포크 조작·탑승 UI를 담당한다. | Chaos Vehicle C++로 재작성 |
| `PW_Loader` | 로더 차량 조향·가속·버킷/차체 조작과 탑승/하차를 담당한다. | Chaos Vehicle C++로 재작성 |
| `V_ForkLift_Front` | 포크리프트 앞바퀴 설정 | Chaos Vehicle Wheel 데이터 |
| `V_ForkLift_Back` | 포크리프트 뒷바퀴 설정 | Chaos Vehicle Wheel 데이터 |
| `V_Loader_Front` | 로더 앞바퀴 설정 | Chaos Vehicle Wheel 데이터 |
| `V_Loader_Back` | 로더 뒷바퀴 설정 | Chaos Vehicle Wheel 데이터 |
| `W_ActionHotkey` | 상호작용 키 안내 UI | UMG 레이아웃은 BP 유지 가능 |
| `W_Crane` | 크레인 조작 UI와 하차 UI 조합 | C++ ViewModel + UMG |
| `W_Drone` | 드론 조작 UI와 하차 UI 조합 | C++ ViewModel + UMG |
| `W_ForkCar` | 포크리프트 조작 UI와 하차 UI 조합 | C++ ViewModel + UMG |
| `W_HitPoint` | 상호작용 대상의 화면상 Hit Point 표시 | UMG 유지 |
| `W_OutCar` | 차량/장비 하차 안내 UI | UMG 유지 |

## C++ 전환 순서

1. Windows SDK와 `GenesisDigitalTwinEditor` 빌드를 정상화한다.
2. 기존 레벨과 Blueprint 참조를 자동 수집하고 기능별 회귀 테스트 맵을 만든다.
3. `E_ProcessState`, 공정 데이터 구조, MQTT 메시지 DTO를 C++로 고정한다.
4. `BP_ProcessManager`, `BP_HangerMovingAlongSpline`, `BP_RobotBase`를 하나의 공정 런타임 계층으로 옮긴다.
5. 타이어 셀을 C++ 큐·작업 명령·로봇 상태 머신으로 옮긴다.
6. AGV·배터리·차량 조립 공정을 같은 작업 인터페이스로 통합한다.
7. 상호작용 컴포넌트와 플레이어 Pawn을 이관한다.
8. PhysX 차량을 Chaos Vehicles로 교체한다.
9. UI는 C++ ViewModel/상태 공급자로 바꾸고 UMG 레이아웃은 유지한다.
10. 기존 BP를 C++ 부모로 Reparent한 뒤 이벤트 그래프가 비면 Data-Only로 만들고, 레벨 참조가 완전히 치환된 자산만 제거한다.

## 주요 구조 위험

- `BP_HangerMovingAlongSpline` ↔ `BP_ProcessManager` ↔ `BP_RobotBase` 사이에 강한 양방향 참조가 있다.
- 타이어 셀도 컨베이어·버퍼·두 로봇이 서로 직접 참조한다.
- 이동 기능이 행거, 차량, 범용 SplineMovable, 타이어 컨베이어에 중복 구현되어 있다.
- 구형 MQTT 모니터와 새 C++ 대시보드가 중복된다.
- Genesis 차량 표현 자산이 여러 세대(`Bp_GENESIS`, `BP_Car`, `BP_CarAssembly`, `BP_GenesisComplete`, 새 모델 `BP_Car`)로 중복된다.
- 프로젝트 기본 맵 설정은 아직 `/Engine/Maps/Templates/OpenWorld`이며 실제 공장 맵이 기본 실행 맵으로 지정되지 않았다.

## 분석 근거와 한계

이 문서는 Asset Registry의 자산 클래스·부모 클래스·의존성, `.uasset` 내부 함수/이벤트/변수 심볼, 기존 C++ 구현을 결합해 작성했다. 원본 게임 모듈을 현재 빌드할 수 없어 일부 그래프는 Unreal Editor에서 완전 로드되지 않았다. 따라서 표의 역할은 신뢰도 높은 구조 분석이지만, 노드 단위 1:1 변환 명세를 만들기 위해서는 Windows SDK 복구 후 원본 프로젝트로 그래프 핀·기본값·레벨 인스턴스 참조를 한 번 더 추출해야 한다.
