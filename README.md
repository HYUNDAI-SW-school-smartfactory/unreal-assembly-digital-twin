# Genesis Assembly Digital Twin

Unreal Engine 5 기반의 Genesis 자동차 조립 라인 디지털 트윈 프로젝트입니다.

이 프로젝트는 실제 공장의 행거, 로봇, AGV, 배터리 리프트, 타이어 장착 셀, 검사 공정을 가상 환경에 재현하고, Node-RED와 MQTT로 전달되는 생산 데이터를 Unreal 씬의 차량 흐름과 모니터링 대시보드에 동기화하는 것을 목표로 합니다.

쉽게 말해, “공장에서 지금 어떤 공정이 얼마나 걸리고, 어디서 병목이 생기고, 차량이 어떤 상태로 이동하는지”를 3D 공장 안에서 실시간처럼 보여주는 디지털 트윈입니다.

---

## 프로젝트가 표현하는 공장 흐름

Genesis 조립 라인은 총 3개 라인으로 구성되어 있으며, 각 라인에는 동일한 6단계 공정이 존재합니다.

```text
1. 도어 제거
2. 라이트 장착
3. 배터리 팩 장착
4. 휠/타이어 장착
5. 도어 & 시트 장착
6. 검사 및 출하
```

차량은 행거에 매달린 상태로 조립 라인을 따라 이동합니다. 각 공정 위치에 도착하면 해당 공정의 로봇, AGV, 리프트, 타이어 셀이 동작하고, 작업이 끝난 뒤 다음 공정으로 이동합니다.

마지막 도어 & 시트 장착 공정이 끝나면 차량은 행거에서 내려와 바닥 위를 주행해 검사 공정으로 들어갑니다. 검사 구간을 통과한 차량은 End Point에서 제거되고, 라인별 생산량이 1 증가합니다.

---

## 주요 기능

### 1. 3개 라인 기반 조립 라인 시뮬레이션

- Line 01, Line 02, Line 03 독립 운영
- 각 라인은 동일한 6개 공정 보유
- 행거가 차량을 싣고 공정을 순차 이동
- 빈 행거는 순환 레일을 따라 다시 첫 공정으로 복귀
- 라인별 생산량과 병목 상태를 독립 관리

### 2. 공정별 차량 상태 변화

각 공정에서는 차량 메쉬의 일부가 Hide/Show 되며 조립 과정을 시각적으로 표현합니다.

| 공정 | 시각 표현 |
| --- | --- |
| 도어 제거 | 도어 컴포넌트 숨김 |
| 라이트 장착 | 라이트 및 관련 바디 파츠 표시 |
| 배터리 장착 | AGV가 배터리 팩 운반 후 Battery Lift 동작 |
| 휠/타이어 장착 | 타이어 셀과 로봇이 타이어 장착 동작 |
| 도어 & 시트 장착 | 도어와 시트 관련 컴포넌트 표시 |
| 검사 | 차량이 바닥으로 내려와 직진 주행 후 출하 |

### 3. 로봇 애니메이션 동기화

공정에 차량이 도착했을 때만 기존 배치 로봇의 애니메이션이 실행됩니다.

- 왼쪽 로봇: `RobotArm_02`
- 오른쪽 로봇: `SK_TruckWelding_Anim`
- 적용 공정: 1, 2, 3, 5번 공정
- 4번 휠/타이어 장착 공정은 별도의 타이어 셀 로직 사용

배터리 공정의 경우 차량 도착 즉시 로봇이 움직이지 않고, AGV가 배터리 팩을 가져오고 Battery Lift가 동작하는 타이밍에 맞춰 로봇 애니메이션이 실행됩니다.

### 4. AGV와 Battery Lift 연동

배터리 공정에서는 라인별 AGV가 자신의 라인만 담당합니다.

```text
AGV 1 → Line 01
AGV 2 → Line 02
AGV 3 → Line 03
```

AGV는 배터리 팩을 운반하고, 이후 Battery Lift가 배터리 장착 동작을 수행합니다. Battery Lift 동작이 끝나기 전까지 차량은 다음 공정으로 이동하지 않습니다.

### 5. 타이어 장착 셀 연동

휠/타이어 공정은 기존 Blueprint 기반 타이어 컨베이어와 타이어 셀을 활용합니다.

- 컨베이어는 타이어를 공급
- Picker 로봇은 타이어를 테이블 버퍼로 이동
- PickerToCar 로봇은 차량이 도착했을 때만 타이어 장착 수행
- 타이어 버퍼가 이미 차 있는 경우에도 차량 도착 요청 이후에만 장착 동작 시작

### 6. MQTT 기반 실시간 공정 데이터 반영

Node-RED에서 MQTT로 보내는 라인/공정 데이터를 Unreal에서 수신합니다.

수신 데이터는 다음 항목에 반영됩니다.

- 공정별 가동률
- 공정별 불량률
- 공정별 사이클 타임
- 공정별 RUN / IDLE 상태
- 라인별 생산량
- 라인별 병목 위치
- 라인별 병목 사유

---

## MQTT 데이터 구조

기본 Topic은 다음과 같습니다.

```text
factory/genesis/line/status
```

라인별 payload 예시는 다음과 같습니다.

```json
{
  "line_id": "GENESIS_ASSEMBLY_LINE_01",
  "timestamp": "2026-06-28T14:08:15.057Z",
  "scenario": "NORMAL",
  "scenario_second": 20,
  "line": {
    "run_status": "RUN",
    "total_produced": 8,
    "total_defects": 0,
    "defect_rate": 0,
    "bottleneck_station": "",
    "reset_flow": false,
    "initial_vehicles": 5,
    "max_vehicles": 5,
    "input_spawn_interval": 9999
  },
  "stations": [
    {
      "process_id": "DOOR_REMOVAL_01",
      "run_status": "RUN",
      "cycle_time": 22,
      "utilization": 0.643,
      "defect_rate": 0.00000202,
      "queue_length": 0
    },
    {
      "process_id": "LIGHT_ASSEMBLY_01",
      "run_status": "RUN",
      "cycle_time": 22,
      "utilization": 0.748,
      "defect_rate": 0.00000202,
      "queue_length": 0
    }
  ]
}
```

지원하는 주요 `process_id`는 다음과 같습니다.

| process_id | Unreal 공정 |
| --- | --- |
| `DOOR_REMOVAL_01` | 도어 제거 |
| `LIGHT_ASSEMBLY_01` | 라이트 장착 |
| `BATTERY_MOUNT_01` | 배터리 팩 장착 |
| `WHEEL_ASSEMBLY_01` | 휠/타이어 장착 |
| `DOOR_ASSEMBLY_01` | 도어 & 시트 장착 |

---

## 병목 및 IDLE 표현

### 사이클 타임 기반 병목

Node-RED에서 특정 공정의 `cycle_time`을 길게 보내면 해당 공정이 병목처럼 작동합니다.

예를 들어 Line 02의 배터리 공정을 병목으로 만들려면:

```json
{
  "process_id": "BATTERY_MOUNT_01",
  "run_status": "RUN",
  "cycle_time": 40,
  "utilization": 0.96,
  "defect_rate": 0.000003,
  "queue_length": 7
}
```

### IDLE 상태

공정이 정지 상태가 되면 해당 공정의 로봇과 장비는 동작하지 않습니다.

IDLE은 두 가지 방식으로 표현할 수 있습니다.

```json
{
  "run_status": "IDLE"
}
```

또는:

```json
{
  "cycle_time": 999
}
```

`cycle_time`이 999로 들어오면 Unreal에서는 해당 공정을 IDLE로 간주합니다.

공정별 모니터에는 빨간색 글씨와 함께 `IDLE`이 표시되고, 라인별 통합 모니터에는 다음과 같은 Reason이 출력됩니다.

```text
REASON: DOOR ASSEMBLY MACHINE IS IDLE
```

---

## 모니터링 화면

공정별 모니터는 각 공정 옆에 배치되며 다음 정보를 표시합니다.

```text
공정명
UTILIZATION
DEFECT RATE
CYCLE TIME
IDLE 또는 BOTTLENECK 상태
```

라인별 통합 모니터는 검사 공정 입구 쪽에 배치되며 다음 정보를 표시합니다.

```text
LINE 번호
PRODUCTION
BOTTLENECK
REASON
```

---

## Node-RED 시나리오 예시

현재 프로젝트는 다음과 같은 시나리오를 기준으로 설계되었습니다.

### NORMAL

병목이 없는 정상 흐름입니다. 모든 공정의 사이클 타임을 가장 오래 걸리는 공정 기준으로 맞춰 차량 흐름이 밀리지 않게 합니다.

예시:

```text
모든 공정 cycle_time = 22초
```

### CYCLE_TIME_BOTTLENECK

특정 공정의 사이클 타임이 길어져 병목이 발생하는 상황입니다.

예시:

```text
Line 02
배터리 공정 cycle_time = 40초
나머지 공정 cycle_time = 10초
```

### IDLE_BOTTLENECK

특정 공정이 정지되어 차량 흐름이 막히는 상황입니다.

예시:

```text
Line 03
DOOR_ASSEMBLY_01 run_status = IDLE
또는 cycle_time = 999
```

---

## 주요 Unreal 맵과 액터

### MainMaps

현재 메인 데모 및 최종 배치는 `MainMaps`를 기준으로 진행합니다.

이 맵에는 다음 요소들이 배치됩니다.

- 3개 조립 라인
- 행거 레일
- 공정별 로봇
- AGV
- Battery Lift
- Tire Assembly Cell
- 공정별 Status Board
- 라인별 통합 Status Board

### GenesisFactoryAssemblyManager

전체 조립 라인의 핵심 제어 액터입니다.

담당 역할:

- 3개 라인 초기화
- 행거 및 차량 흐름 제어
- 공정별 작업 시작/완료 처리
- MQTT 데이터 수신 및 적용
- 공정별/라인별 모니터 업데이트
- 로봇, AGV, Battery Lift, Tire Cell 연동
- 생산량 및 병목 상태 관리

MainMaps에는 이 Manager가 반드시 하나만 존재해야 합니다.

---

## 실행 방법

### 1. 프로젝트 열기

```text
GenesisDigitalTwin/GenesisDigitalTwin.uproject
```

Unreal Engine 5에서 프로젝트를 엽니다.

### 2. MainMaps 열기

Content Browser에서 다음 맵을 엽니다.

```text
Content/Maps/MainMaps
```

### 3. Node-RED 실행 및 Deploy

Node-RED에서 MQTT publish flow를 Deploy합니다.

MQTT Broker는 HiveMQ Cloud를 사용합니다.

```text
Protocol: MQTT V3.1.1
Port: 8883
TLS: Enabled
Topic: factory/genesis/line/status
```

### 4. Unreal Play

Unreal Editor에서 Play를 누르면 차량 흐름, 공정 애니메이션, MQTT 모니터링 데이터가 반영됩니다.

---

## 빌드 및 주의사항

C++ 또는 플러그인 코드가 변경된 경우 Unreal Editor를 완전히 종료한 뒤 빌드해야 합니다.

이 프로젝트에서는 Live Coding보다 Editor 종료 후 전체 빌드를 권장합니다.

```text
BuildEditor.ps1
```

빌드가 성공하면 다음과 같이 표시됩니다.

```text
Result: Succeeded
```

특히 MQTT 플러그인이나 C++ Actor를 수정한 경우에는 반드시 Unreal Editor를 완전히 껐다가 다시 열어야 합니다.

---

## 개발 중 해결한 주요 안정성 이슈

### MQTT payload 파싱 안정화

Paho MQTT payload는 null-terminated 문자열이 아니기 때문에 payload length 기준으로 읽도록 수정했습니다. 이 작업으로 payload 뒤에 깨진 문자가 붙거나 PIE 재시작 시 크래시가 발생하는 문제를 줄였습니다.

### PIE 재시작 안정화

Play를 멈추고 다시 Play할 때 MQTT callback과 Unreal UObject 정리 타이밍이 충돌하지 않도록 방어 로직을 추가했습니다.

### 보드 Widget 정리

PIE 종료 시 Status Board의 WidgetComponent를 정리해 렌더링 리소스가 남지 않도록 처리했습니다.

### 타이어 장착 중복 실행 방지

두 번째, 세 번째 차량부터 타이어 버퍼가 이미 차 있는 상태에서 PickerToCar가 너무 빠르게 실행되는 문제를 완화하기 위해 차량 도착 요청 이후 최소 딜레이와 중복 실행 방지 플래그를 추가했습니다.

---

## 프로젝트 구조

```text
GenesisDigitalTwin/
├─ Config/                 # 프로젝트 설정
├─ Content/
│  ├─ Blueprints/          # 기존 Blueprint 자산
│  ├─ Factory/             # 공장 관련 자산
│  ├─ Machines/            # 로봇, 장비 자산
│  ├─ Maps/                # MainMaps 등 레벨
│  ├─ Materials/           # 머티리얼
│  ├─ Meshes/              # 공장/장비/차량 메쉬
│  ├─ MQTT/                # MQTT 관련 Blueprint/자산
│  ├─ UI/                  # UI/Widget 자산
│  └─ Vehicles/            # 차량 관련 자산
├─ Plugins/
│  └─ FF_MQTT_Sync/        # Paho 기반 MQTT 플러그인
└─ GenesisDigitalTwin.uproject
```

---

## 이 프로젝트의 목표

이 프로젝트는 단순한 3D 공장 배경이 아니라, 실시간 생산 데이터와 공정 상태를 반영하는 조립 라인 디지털 트윈을 목표로 합니다.

차량이 어디서 멈추는지, 어떤 공정이 병목인지, 어떤 공정이 IDLE인지, 생산량이 얼마나 증가했는지를 시각적으로 확인할 수 있도록 설계되었습니다.

최종적으로는 Node-RED, MQTT, Unreal Engine을 연결해 실제 공정 데이터 기반의 공장 모니터링 및 병목 분석 데모로 활용할 수 있습니다.

