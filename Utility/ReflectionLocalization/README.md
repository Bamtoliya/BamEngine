# Reflection Localization Export

`Engine/Source`의 Reflection 선언을 분석한 결과에서 Localization 키를 추출한다.
메타데이터 해석은 Clang 생성기가 담당하고, CSV 병합은 Python 표준 라이브러리로 처리한다.

## 빌드에서 실행

저장소 루트에서 기존 CMake 빌드 디렉터리를 사용한다.

```powershell
cmake -S . -B out/build/engine-reflection-unified
cmake --build out/build/engine-reflection-unified --config Debug --target ExportReflectionLocalization
```

전체 기본 빌드에도 `ExportReflectionLocalization`이 포함된다. 이 타깃은 Engine 빌드 성공 후 실행된다.
`--target Engine`만 지정한 빌드에는 별도 추출 타깃이 포함되지 않는다.
기존 CSV→JSON 변환 타깃은 추출 완료 후 실행된다.

출력은 다음과 같다.

- `Engine/Generated/BamEngine.gen.metadata.json`: 메타데이터 값과 소유 선언, 소스 위치
- `Resources/Data/Localization/Engine.csv`: 기존 번역을 보존하면서 누락된 키를 추가하는 CSV

현재 활성 생성 모듈은 Engine이다. Editor의 별도 Reflection 생성은 이 작업에 포함하지 않는다.

## Utility만 실행

```powershell
python Utility/ReflectionLocalization/export_localization.py `
    --input Engine/Generated/BamEngine.gen.metadata.json `
    --catalog-dir Resources/Data/Localization --base Engine.csv `
    --output Resources/Data/Localization/Engine.csv --in-place
```

기본 추출 항목은 `DisplayName`, `Category`, `Tooltip`이다. `--field Units`처럼 항목을 추가할 수 있다.
`--input` 뒤에 여러 메타데이터 JSON 경로를 전달하면 키를 합쳐서 처리한다.

## 병합 정책

- 빌드에서는 `--in-place`로 원본 Engine.csv에 병합한다. 다른 카탈로그 파일은 수정하지 않는다.
- 기존 Engine.csv의 키와 English/Korean 번역을 보존한다. 신규 키의 번역은 비워 둔다.
- 같은 메타데이터 키의 반복 사용은 합친다. 다른 CSV에 이미 있는 키는 추가하지 않는다.
- 원본 CSV의 중복 최종 키는 오류로 처리한다. 최종 키는 기존 변환기와 같은 `Group.Key` 규칙이다.
- Group은 Reflection의 Category와 별개다. 그룹 없는 행을 먼저 출력해 그룹 상속 오류를 방지한다.
- 출력은 UTF-8 BOM CSV다. 입력 CSV는 UTF-8 BOM 및 CP949를 지원한다.
- 내용이 같으면 다시 쓰지 않는다. 변경 시 임시 파일을 기록한 뒤 교체한다.

추가된 빈 번역을 Engine.csv에서 작성하면 기존 CSV→JSON 빌드에 포함된다.
검토용 사본이 필요하면 `--in-place`를 생략하고 `--output out/localization-review/Engine.csv`로 실행한다.
`--in-place`는 출력이 `--base`와 같은 파일일 때만 허용한다. 별도 출력은 원본 카탈로그 폴더 밖에 둔다.
키 추출은 UI의 Category/Tooltip 번역 호출을 자동으로 추가하지 않는다.

## Utility 검증

```powershell
python -B -m unittest discover -s Utility/ReflectionLocalization -p test_*.py -v
```

번역 보존, 반복 키 통합, 다른 카탈로그의 키 재사용, 기존 변환기와의 그룹·여러 줄 문자열 호환,
실패 시 출력 보존, CP949 입력, 명시적 원본 병합, 잘못된 덮어쓰기 방지와 불필요한 재기록 방지를 확인한다.

`Projects/Reflection/examples/generator-tests/MetadataExport.h`는 생성기 검증용 입력이다.
부동소수점 접미사, FLT_MAX, 전방 선언 포인터 프로퍼티, 타입·함수·enum 메타데이터를 포함한다.
