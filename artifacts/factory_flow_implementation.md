# MainMaps 공장 흐름 구현 메모

## 실행 구조

- 기본 맵: `/Game/Maps/MainMaps`
- 런타임 관리자 Blueprint: `/Game/Blueprints/BP_GenesisFactoryAssemblyManager`
- `MainMaps`가 시작되면 `UGenesisFactoryWorldSubsystem`이 관리자 Blueprint를 자동 생성한다.
- 라인 중심 Y 좌표:
  - Line 1: `1620`
  - Line 2: `6400`
  - Line 3: `11200`

## 차량 부품 매핑

`BP_GenesisFactoryAssemblyManager`를 열고 `Class Defaults > Factory > Visuals > Car Visuals`에서 다음 배열에 `BP_Car`의 컴포넌트 이름을 입력한다.

- `Door Component Names`
- `Seat Component Names`
- `Light Component Names`
- `Battery Component Names`
- `Wheel Component Names`

현재 값은 의도적으로 비어 있다. 잘못된 이름이나 `None` 항목은 안전하게 무시된다.

## 공정 흐름

1. Door Removal
2. Light Install
3. Battery Install
4. Wheel / Tire Install
5. Door & Seat Install
6. Inspection

각 공정 입력 버퍼는 최대 2대다. 다음 버퍼가 가득 차면 완료된 차량이 현재 공정을 점유한 채 멈추며, 이 정체가 이전 공정으로 전파된다.

현재 가득 찬 버퍼 중 가장 먼저 가득 찬 시각을 기록해 최초 병목 공정으로 표시한다. 뒤쪽 병목 때문에 앞 공정까지 밀려도 원래 병목만 유지된다.

## 기존 Blueprint 연결

- AGV: `BP_AGV`, `BP_AGV2`, `BP_AGV3`
- Battery Lift: 각 라인 중심에 가장 가까운 `BP_BatteryLift`
- Tire Cell Right: 각 라인에 가장 가까운 `BP_TireAssemblyCell`
- Tire Cell Left: 각 라인에 가장 가까운 `BP_TireAssemblyCell_L`
- 왼쪽 공정 로봇: `SK_RoboArm04`로 런타임 생성
- 오른쪽 공정 로봇: 맵의 `SK_TruckWelding_Anim*` Actor와 `SK_TruckWelding_Anim` 애니메이션 사용

## MQTT 형식

생산량과 병목 위치는 MQTT에서 받지 않고 로컬에서 계산한다.

```json
{
  "lines": [
    {
      "line_id": 1,
      "stations": [
        {
          "station_id": "DOOR_REMOVAL",
          "run_status": "RUN",
          "utilization": 0.84,
          "defect_rate": 0.00002,
          "cycle_time": 6.4
        }
      ]
    }
  ]
}
```

지원 공정 ID:

- `DOOR_REMOVAL`
- `LIGHT_INSTALL`
- `BATTERY_INSTALL`
- `WHEEL_INSTALL` 또는 `TIRE_INSTALL`
- `DOOR_SEAT_INSTALL`
- `INSPECTION`

`defect_rate`는 비율 값이다. `0.00002`는 화면에서 `0.002%`로 표시된다.
