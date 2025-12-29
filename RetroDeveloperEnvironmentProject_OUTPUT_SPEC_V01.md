# Retro Developer Environment Project - Debug Output Specification V01

## 개요

이 문서는 AppleWin과 openMSX 에뮬레이터에서 공통으로 사용할 수 있는 **통합 디버그 출력 포맷**을 정의합니다.

### 목표
- 단일 TCP 포트를 통한 모든 디버그 정보 통합 출력
- 실시간 스트리밍에 적합한 JSON Lines 형식
- 두 에뮬레이터 간 호환 가능한 공통 필드 정의
- 클라이언트 측에서 유연한 필터링 및 처리 가능
- **모든 라인에 에뮬레이터 식별자 포함**

### 적용 범위
| 에뮬레이터 | emu 값 | CPU | 지원 플랫폼 |
|-----------|--------|-----|------------|
| AppleWin | `apple` | 6502/65C02 | Apple II, II+, IIe, IIc |
| openMSX | `msx` | Z80/R800 | MSX1, MSX2, MSX2+, MSXturboR |

---

## 1. 프로토콜 사양

### 1.1 연결 방식
- **프로토콜**: Raw TCP Socket (또는 Telnet 호환)
- **포트**: 설정 가능 (기본값: 6502 또는 사용자 지정)
- **바인드 주소**: 127.0.0.1 (기본, 보안 목적)
- **인코딩**: UTF-8
- **연결 유형**: 지속 연결 (Persistent Connection)

### 1.2 데이터 전송 모델
```
[Emulator] ──TCP──> [Client]
     │
     ├── 연결 수립 시 초기 상태 전송
     ├── 이벤트 발생 시 즉시 전송 (Push 방식)
     └── 주기적 상태 업데이트 (설정에 따라)
```

### 1.3 연결 핸드셰이크
클라이언트 연결 시 에뮬레이터는 즉시 `hello` 메시지를 전송합니다:
```json
{"emu":"apple","cat":"sys","sec":"conn","fld":"hello","val":"AppleWin 1.30.0","ver":"1.0","ts":1704067200000}
```

---

## 2. 출력 포맷 정의

### 2.1 기본 형식 (JSON Lines)
각 데이터는 **한 줄의 독립된 JSON 객체**로 출력되며, 줄바꿈(`\n`)으로 구분됩니다.

```
{"emu":"..","cat":"..","sec":"..","fld":"..","val":".."}\n
{"emu":"..","cat":"..","sec":"..","fld":"..","val":".."}\n
```

### 2.2 필드 정의

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `emu` | string | **Y** | **Emulator** - 에뮬레이터 식별자 (`apple` 또는 `msx`) |
| `cat` | string | Y | **Category** - 데이터 대분류 |
| `sec` | string | Y | **Section** - 중분류/하위 그룹 |
| `fld` | string | Y | **Field** - 구체적 필드명 |
| `val` | string | Y | **Value** - 값 (항상 문자열) |
| `ts` | number | N | **Timestamp** - Unix 밀리초 (선택) |
| `idx` | number | N | **Index** - 배열/순서 인덱스 (선택) |
| `addr` | string | N | **Address** - 메모리 주소 (선택) |
| `len` | number | N | **Length** - 데이터 길이 (선택) |

### 2.3 에뮬레이터 식별자 (`emu`)

| 값 | 에뮬레이터 | 설명 |
|----|-----------|------|
| `apple` | AppleWin | Apple II 시리즈 에뮬레이터 |
| `msx` | openMSX | MSX 시리즈 에뮬레이터 |

**중요**: `emu` 필드는 **모든 출력 라인에 필수**입니다. 이를 통해 클라이언트는 여러 에뮬레이터의 데이터를 단일 스트림에서 구분할 수 있습니다.

### 2.4 값(val) 형식 규칙
- **16진수**: 대문자, `$` 접두사 없음 (예: `"C600"`, `"FF"`)
- **불리언**: `"0"` 또는 `"1"`
- **숫자**: 10진수 문자열 (예: `"12345"`)
- **문자열**: 그대로 (JSON 이스케이프 적용)

---

## 3. 카테고리 정의

### 3.1 카테고리 목록

| cat | 설명 | 주요 section |
|-----|------|-------------|
| `cpu` | CPU 상태 | `reg`, `flag`, `int`, `state` |
| `mem` | 메모리 | `read`, `write`, `dump`, `bank` |
| `io` | 입출력 | `port`, `switch`, `slot` |
| `mach` | 머신 상태 | `info`, `status`, `video` |
| `dbg` | 디버거 | `bp`, `watch`, `trace` |
| `sys` | 시스템 메시지 | `conn`, `error`, `event` |

---

## 4. CPU 카테고리 (`cpu`)

### 4.1 레지스터 (`sec: "reg"`)

#### 공통 레지스터 (추상화)
```json
{"emu":"apple","cat":"cpu","sec":"reg","fld":"pc","val":"C600"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"pc","val":"0000"}
```

#### AppleWin (6502/65C02)
| fld | 크기 | 설명 |
|-----|------|------|
| `a` | 8-bit | Accumulator |
| `x` | 8-bit | X Index |
| `y` | 8-bit | Y Index |
| `sp` | 8-bit | Stack Pointer |
| `pc` | 16-bit | Program Counter |
| `p` | 8-bit | Processor Status |

```json
{"emu":"apple","cat":"cpu","sec":"reg","fld":"a","val":"F3"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"x","val":"00"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"y","val":"01"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"sp","val":"FF"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"pc","val":"C600"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"p","val":"30"}
```

#### openMSX (Z80/R800)
| fld | 크기 | 설명 |
|-----|------|------|
| `af` | 16-bit | AF pair |
| `bc` | 16-bit | BC pair |
| `de` | 16-bit | DE pair |
| `hl` | 16-bit | HL pair |
| `af2` | 16-bit | AF' (alternate) |
| `bc2` | 16-bit | BC' (alternate) |
| `de2` | 16-bit | DE' (alternate) |
| `hl2` | 16-bit | HL' (alternate) |
| `ix` | 16-bit | Index X |
| `iy` | 16-bit | Index Y |
| `sp` | 16-bit | Stack Pointer |
| `pc` | 16-bit | Program Counter |
| `i` | 8-bit | Interrupt Vector |
| `r` | 8-bit | Refresh |

```json
{"emu":"msx","cat":"cpu","sec":"reg","fld":"af","val":"F3A0"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"bc","val":"0100"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"hl","val":"C000"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"sp","val":"F380"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"pc","val":"0000"}
```

### 4.2 플래그 (`sec: "flag"`)

#### AppleWin (6502)
| fld | 비트 | 설명 |
|-----|------|------|
| `n` | 7 | Negative/Sign |
| `v` | 6 | Overflow |
| `b` | 4 | Break |
| `d` | 3 | Decimal |
| `i` | 2 | Interrupt Disable |
| `z` | 1 | Zero |
| `c` | 0 | Carry |

```json
{"emu":"apple","cat":"cpu","sec":"flag","fld":"n","val":"0"}
{"emu":"apple","cat":"cpu","sec":"flag","fld":"z","val":"1"}
{"emu":"apple","cat":"cpu","sec":"flag","fld":"c","val":"0"}
```

#### openMSX (Z80)
| fld | 비트 | 설명 |
|-----|------|------|
| `s` | 7 | Sign |
| `z` | 6 | Zero |
| `h` | 4 | Half-Carry |
| `pv` | 2 | Parity/Overflow |
| `n` | 1 | Subtract |
| `c` | 0 | Carry |

```json
{"emu":"msx","cat":"cpu","sec":"flag","fld":"s","val":"1"}
{"emu":"msx","cat":"cpu","sec":"flag","fld":"z","val":"0"}
{"emu":"msx","cat":"cpu","sec":"flag","fld":"pv","val":"0"}
```

### 4.3 인터럽트 상태 (`sec: "int"`)

```json
{"emu":"apple","cat":"cpu","sec":"int","fld":"enabled","val":"1"}
{"emu":"msx","cat":"cpu","sec":"int","fld":"mode","val":"1"}
{"emu":"msx","cat":"cpu","sec":"int","fld":"halted","val":"0"}
```

### 4.4 CPU 상태 (`sec: "state"`)

```json
{"emu":"apple","cat":"cpu","sec":"state","fld":"type","val":"65C02"}
{"emu":"apple","cat":"cpu","sec":"state","fld":"cycles","val":"12345678"}
{"emu":"apple","cat":"cpu","sec":"state","fld":"jammed","val":"0"}
{"emu":"msx","cat":"cpu","sec":"state","fld":"type","val":"R800"}
```

---

## 5. 메모리 카테고리 (`mem`)

### 5.1 메모리 읽기 이벤트 (`sec: "read"`)
```json
{"emu":"apple","cat":"mem","sec":"read","fld":"byte","val":"A9","addr":"C600"}
{"emu":"msx","cat":"mem","sec":"read","fld":"word","val":"20A9","addr":"C600"}
```

### 5.2 메모리 쓰기 이벤트 (`sec: "write"`)
```json
{"emu":"apple","cat":"mem","sec":"write","fld":"byte","val":"FF","addr":"0300"}
{"emu":"msx","cat":"mem","sec":"write","fld":"byte","val":"00","addr":"8000"}
```

### 5.3 메모리 덤프 (`sec: "dump"`)
연속된 메모리 영역을 16진수 문자열로 출력:
```json
{"emu":"apple","cat":"mem","sec":"dump","fld":"data","val":"A9208500A900850120","addr":"C600","len":9}
{"emu":"msx","cat":"mem","sec":"dump","fld":"data","val":"F3EDA0210080","addr":"0000","len":6}
```

### 5.4 뱅크/슬롯 정보 (`sec: "bank"`)

#### AppleWin (뱅크 스위칭)
```json
{"emu":"apple","cat":"mem","sec":"bank","fld":"auxread","val":"0"}
{"emu":"apple","cat":"mem","sec":"bank","fld":"auxwrite","val":"0"}
{"emu":"apple","cat":"mem","sec":"bank","fld":"80store","val":"1"}
{"emu":"apple","cat":"mem","sec":"bank","fld":"altzp","val":"0"}
{"emu":"apple","cat":"mem","sec":"bank","fld":"page2","val":"0"}
{"emu":"apple","cat":"mem","sec":"bank","fld":"hires","val":"1"}
```

#### openMSX (슬롯 시스템)
```json
{"emu":"msx","cat":"mem","sec":"bank","fld":"page_pri","val":"0","idx":0}
{"emu":"msx","cat":"mem","sec":"bank","fld":"page_sec","val":"0","idx":0}
{"emu":"msx","cat":"mem","sec":"bank","fld":"page_pri","val":"1","idx":1}
{"emu":"msx","cat":"mem","sec":"bank","fld":"expanded","val":"1","idx":0}
```

---

## 6. I/O 카테고리 (`io`)

### 6.1 I/O 포트 (`sec: "port"`)
```json
{"emu":"msx","cat":"io","sec":"port","fld":"read","val":"FF","addr":"A0"}
{"emu":"msx","cat":"io","sec":"port","fld":"write","val":"00","addr":"A1"}
```

### 6.2 소프트 스위치 (`sec: "switch"`) - AppleWin
```json
{"emu":"apple","cat":"io","sec":"switch","fld":"80store","val":"1","addr":"C000"}
{"emu":"apple","cat":"io","sec":"switch","fld":"ramrd","val":"0","addr":"C002"}
{"emu":"apple","cat":"io","sec":"switch","fld":"ramwrt","val":"0","addr":"C004"}
```

### 6.3 슬롯 정보 (`sec: "slot"`)

#### AppleWin
```json
{"emu":"apple","cat":"io","sec":"slot","fld":"type","val":"Disk II","idx":6}
{"emu":"apple","cat":"io","sec":"slot","fld":"active","val":"1","idx":6}
```

#### openMSX
```json
{"emu":"msx","cat":"io","sec":"slot","fld":"device","val":"ROM","idx":0}
{"emu":"msx","cat":"io","sec":"slot","fld":"primary","val":"0","idx":0}
{"emu":"msx","cat":"io","sec":"slot","fld":"secondary","val":"0","idx":0}
```

---

## 7. 머신 카테고리 (`mach`)

### 7.1 머신 정보 (`sec: "info"`)
```json
{"emu":"apple","cat":"mach","sec":"info","fld":"id","val":"apple2e"}
{"emu":"apple","cat":"mach","sec":"info","fld":"name","val":"Enhanced Apple //e"}
{"emu":"apple","cat":"mach","sec":"info","fld":"type","val":"Apple2e"}
```

```json
{"emu":"msx","cat":"mach","sec":"info","fld":"id","val":"Panasonic_A1F"}
{"emu":"msx","cat":"mach","sec":"info","fld":"name","val":"Panasonic FS-A1F"}
{"emu":"msx","cat":"mach","sec":"info","fld":"type","val":"MSXturboR"}
```

### 7.2 실행 상태 (`sec: "status"`)
```json
{"emu":"apple","cat":"mach","sec":"status","fld":"mode","val":"running"}
{"emu":"msx","cat":"mach","sec":"status","fld":"mode","val":"paused"}
{"emu":"apple","cat":"mach","sec":"status","fld":"powered","val":"1"}
```

### 7.3 비디오 모드 (`sec: "video"`)
```json
{"emu":"apple","cat":"mach","sec":"video","fld":"mode","val":"hires"}
{"emu":"apple","cat":"mach","sec":"video","fld":"page","val":"1"}
{"emu":"apple","cat":"mach","sec":"video","fld":"mixed","val":"0"}
```

---

## 8. 디버그 카테고리 (`dbg`)

### 8.1 브레이크포인트 (`sec: "bp"`)
```json
{"emu":"apple","cat":"dbg","sec":"bp","fld":"hit","val":"1","addr":"C600","idx":0}
{"emu":"msx","cat":"dbg","sec":"bp","fld":"add","val":"exec","addr":"D000","idx":1}
{"emu":"apple","cat":"dbg","sec":"bp","fld":"remove","val":"","addr":"D000","idx":1}
{"emu":"msx","cat":"dbg","sec":"bp","fld":"enabled","val":"1","idx":0}
```

### 8.2 워치포인트 (`sec: "watch"`)
```json
{"emu":"apple","cat":"dbg","sec":"watch","fld":"read","val":"A9","addr":"0300"}
{"emu":"msx","cat":"dbg","sec":"watch","fld":"write","val":"FF","addr":"0300"}
```

### 8.3 실행 추적 (`sec: "trace"`)
```json
{"emu":"apple","cat":"dbg","sec":"trace","fld":"exec","val":"LDA #$20","addr":"C600"}
{"emu":"apple","cat":"dbg","sec":"trace","fld":"opcode","val":"A9","addr":"C600"}
{"emu":"msx","cat":"dbg","sec":"trace","fld":"exec","val":"LD A,#FF","addr":"0000"}
```

---

## 9. 시스템 카테고리 (`sys`)

### 9.1 연결 관리 (`sec: "conn"`)
```json
{"emu":"apple","cat":"sys","sec":"conn","fld":"hello","val":"AppleWin 1.30.0","ver":"1.0","ts":1704067200000}
{"emu":"msx","cat":"sys","sec":"conn","fld":"hello","val":"openMSX 19.1","ver":"1.0","ts":1704067200000}
{"emu":"apple","cat":"sys","sec":"conn","fld":"goodbye","val":"","ts":1704067300000}
```

### 9.2 에러 메시지 (`sec: "error"`)
```json
{"emu":"apple","cat":"sys","sec":"error","fld":"message","val":"Invalid address format"}
{"emu":"msx","cat":"sys","sec":"error","fld":"code","val":"E001"}
```

### 9.3 이벤트 알림 (`sec: "event"`)
```json
{"emu":"apple","cat":"sys","sec":"event","fld":"reset","val":"cold"}
{"emu":"msx","cat":"sys","sec":"event","fld":"reset","val":"warm"}
{"emu":"apple","cat":"sys","sec":"event","fld":"disk","val":"insert","idx":1}
{"emu":"msx","cat":"sys","sec":"event","fld":"cart","val":"insert","idx":0}
```

---

## 10. 데이터 출력 시점

### 10.1 이벤트 기반 출력 (Push)
다음 이벤트 발생 시 즉시 출력:

| 이벤트 | 출력 데이터 |
|--------|------------|
| CPU 명령 실행 | `cpu.reg.*`, `cpu.flag.*` (변경된 것만) |
| 메모리 접근 | `mem.read`, `mem.write` |
| 브레이크포인트 히트 | `dbg.bp.hit` |
| 머신 상태 변경 | `mach.status` |
| I/O 접근 | `io.port`, `io.switch` |

### 10.2 주기적 출력 (선택적)
클라이언트 요청 또는 설정에 따라 주기적으로 전체 상태 전송:

```json
{"emu":"apple","cat":"sys","sec":"event","fld":"snapshot","val":"start","ts":1704067200000}
... (전체 상태 데이터) ...
{"emu":"apple","cat":"sys","sec":"event","fld":"snapshot","val":"end","ts":1704067200100}
```

---

## 11. 클라이언트 명령 (선택적 확장)

클라이언트에서 에뮬레이터로 명령을 보낼 수 있는 선택적 기능:

### 11.1 명령 형식
```json
{"cmd":"subscribe","args":["cpu","mem"]}
{"cmd":"unsubscribe","args":["io"]}
{"cmd":"snapshot","args":[]}
{"cmd":"setbp","args":["C600","exec"]}
{"cmd":"delbp","args":["0"]}
```

### 11.2 응답 형식
```json
{"emu":"apple","cat":"sys","sec":"resp","fld":"ok","val":"subscribe"}
{"emu":"msx","cat":"sys","sec":"resp","fld":"error","val":"unknown command"}
```

---

## 12. 구현 가이드라인

### 12.1 서버 측 (에뮬레이터)
1. 단일 TCP 포트에서 여러 클라이언트 연결 허용
2. 각 연결에 대해 독립적인 스트림 유지
3. 비동기 출력 (에뮬레이션 성능에 영향 최소화)
4. **모든 출력 라인에 `emu` 필드 필수 포함**
5. 버퍼링 및 배치 전송 지원 (옵션)

### 12.2 클라이언트 측
1. JSON Lines 파서 사용 (줄 단위 파싱)
2. **`emu` 필드로 에뮬레이터 구분** (1차 필터링)
3. `cat` 필드로 카테고리 필터링
4. `sec`, `fld` 필드로 세부 처리
5. 타임스탬프 기반 순서 정렬 (필요시)

### 12.3 권장 버퍼 크기
- 송신 버퍼: 64KB
- 수신 버퍼: 16KB
- 최대 라인 길이: 4KB

---

## 13. 확장성

### 13.1 새로운 에뮬레이터 추가
새로운 에뮬레이터 추가 시:
1. 고유한 `emu` 값 정의 (예: `c64`, `nes`, `sega`)
2. 해당 CPU 레지스터, 플래그 문서화
3. 기존 클라이언트는 unknown `emu` 값 무시 또는 처리

### 13.2 새로운 카테고리 추가
1. 고유한 `cat` 값 정의
2. 해당 `sec`, `fld` 문서화
3. 기존 클라이언트는 unknown 카테고리 무시

### 13.3 버전 관리
- `hello` 메시지의 `ver` 필드로 프로토콜 버전 식별
- 하위 호환성 유지 (필드 추가만, 삭제 없음)

---

## 14. 예제 출력 스트림

### 14.1 AppleWin 세션 예제
```json
{"emu":"apple","cat":"sys","sec":"conn","fld":"hello","val":"AppleWin 1.30.0","ver":"1.0","ts":1704067200000}
{"emu":"apple","cat":"mach","sec":"info","fld":"type","val":"Apple2e"}
{"emu":"apple","cat":"mach","sec":"status","fld":"mode","val":"running"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"pc","val":"C600"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"a","val":"00"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"x","val":"00"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"y","val":"00"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"sp","val":"FF"}
{"emu":"apple","cat":"dbg","sec":"trace","fld":"exec","val":"LDA #$20","addr":"C600"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"a","val":"20"}
{"emu":"apple","cat":"cpu","sec":"flag","fld":"z","val":"0"}
{"emu":"apple","cat":"mem","sec":"read","fld":"byte","val":"20","addr":"C601"}
{"emu":"apple","cat":"dbg","sec":"trace","fld":"exec","val":"STA $00","addr":"C602"}
{"emu":"apple","cat":"mem","sec":"write","fld":"byte","val":"20","addr":"0000"}
```

### 14.2 openMSX 세션 예제
```json
{"emu":"msx","cat":"sys","sec":"conn","fld":"hello","val":"openMSX 19.1","ver":"1.0","ts":1704067200000}
{"emu":"msx","cat":"mach","sec":"info","fld":"type","val":"MSX2"}
{"emu":"msx","cat":"mach","sec":"status","fld":"mode","val":"running"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"pc","val":"0000"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"af","val":"F3A0"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"bc","val":"0100"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"sp","val":"F380"}
{"emu":"msx","cat":"mem","sec":"bank","fld":"page_pri","val":"0","idx":0}
{"emu":"msx","cat":"dbg","sec":"trace","fld":"exec","val":"LD A,#FF","addr":"0000"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"af","val":"FF00"}
{"emu":"msx","cat":"cpu","sec":"flag","fld":"s","val":"1"}
```

### 14.3 혼합 스트림 예제 (다중 에뮬레이터)
클라이언트가 여러 에뮬레이터에 연결된 경우:
```json
{"emu":"apple","cat":"cpu","sec":"reg","fld":"pc","val":"C600","ts":1704067200001}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"pc","val":"0100","ts":1704067200002}
{"emu":"apple","cat":"dbg","sec":"trace","fld":"exec","val":"JSR $C700","addr":"C600","ts":1704067200003}
{"emu":"msx","cat":"dbg","sec":"trace","fld":"exec","val":"CALL #8000","addr":"0100","ts":1704067200004}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"pc","val":"C700","ts":1704067200005}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"pc","val":"8000","ts":1704067200006}
```

---

## 15. 필드 요약 테이블

### 15.1 필수 필드
| 순서 | 필드 | 설명 |
|------|------|------|
| 1 | `emu` | 에뮬레이터 식별자 |
| 2 | `cat` | 카테고리 |
| 3 | `sec` | 섹션 |
| 4 | `fld` | 필드명 |
| 5 | `val` | 값 |

### 15.2 선택 필드
| 필드 | 사용 시점 |
|------|----------|
| `ts` | 이벤트 시간 추적 필요시 |
| `idx` | 배열/슬롯 인덱스 |
| `addr` | 메모리/I/O 주소 |
| `len` | 데이터 길이 |
| `ver` | 프로토콜 버전 (hello 메시지) |

---

## 변경 이력

| 버전 | 날짜 | 변경 내용 |
|------|------|----------|
| V01 | 2024-12-29 | 초안 작성 - 모든 라인에 `emu` 필드 필수 포함 |

---

## 참고 문서

- AppleWin Debug Server: `AppleWin/source/debugserver/`
- openMSX Debug HTTP: `openMSX/src/debugger/`
