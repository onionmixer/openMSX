# BaDCaT 카트리지 에뮬레이션 (Fork 추가 기능)

**추가: 2026-10-15 (`47094ef8f`) · 문서: 2026-10-19**
**Fork: https://github.com/onionmixer/openMSX**

BaDCaT(BaDCaT Electronics · Andres Ortiz · 2020)는 TL16C550C UART 를 쓰는 MSX RS-232C / WiFi 모뎀 카트리지다.
이 fork 는 그 카트리지의 **UART 와 외부 RS-232 포트**를 에뮬레이트한다. 기존 장치(i8251 기반 `rs232` 확장 등)는 바뀌지 않았다.

---

## 1. 무엇을 에뮬레이트하나

| 항목 | 내용 |
|---|---|
| UART | TL16C550C(NS16550A 호환) · 클록 **1.8432 MHz** |
| I/O | 포트 **`0x80`–`0x87`**(레지스터 0..7 · `port & 7`) |
| 인터럽트 | UART `INTRPT` → MSX **/INT** — `OUT2`(MCR bit 3)로 막지 않는다(BDterm 은 MCR=`$02`, TELNETF 는 `$0F`/`$2F` 로 인터럽트를 받는다) |
| ROM | **없음** — 소프트웨어가 UART 를 직접 다룬다 |
| 외부 포트 | 커넥터 **`badcat-rs232`** — openMSX 의 RS-232 장치(`rs232-net` · `rs232-host` · `rs232-tester` 등)를 꽂는다 |
| WiFi | **에뮬레이트하지 않는다** — 카트리지의 wifi 스위치 OFF(외부 RS-232 포트 사용) 상태에 해당. ESP8266 모뎀 없음 |

### 1.1 UART 기능 (`src/serial/NS16550.*`)
- 레지스터 전부 · 분주기(DLL/DLM · DLAB) · 문자 길이·정지 비트·패리티 설정 · 문자 단위 시간 진행(보레이트에 따른 송수신 시간).
- **16 바이트 RX/TX FIFO** · FIFO 트리거 레벨(1/4/8/14) · **문자 타임아웃 인터럽트**.
- IER/IIR 인터럽트(수신 데이터 · 송신 보유 레지스터 빔(THRE) · 라인 상태 · 모뎀 상태) · LSR(DR · OE · THRE · TEMT …).
- 모뎀 상태 델타 비트(MSR) · **루프백**(MCR bit 4).
- TL16C550C **자동 흐름 제어**(MCR bit 5 — auto-RTS / auto-CTS · FIFO 모드에서만).
- 구현하지 않은 것: 패리티·프레이밍·브레이크 **오류 검출**, DMA 모드 신호, FIFO 모드의 THRE 지연 인터럽트 규칙.

### 1.2 수신 백프레셔 설정
- **`badcat_rx_backpressure`**(BooleanSetting · 기본 **true** · 저장 안 함)
  - `true`: RX FIFO 가 차 있으면 꽂힌 장치가 다음 바이트를 **기다린다**(`isRxBusy()` = 설정 true **그리고** RX FIFO 가득 · `acceptsData()` 는 늘 true — 16550 에는 수신 허용 비트가 없다) — 바이트를 잃지 않는다(이 fork 의 i8251 경로와 같은 성질 · §4).
  - `false`: 회선 속도대로 계속 들어와 **실기처럼 오버런**(LSR OE)이 난다 — 수신 손실 대책을 시험할 때 쓴다.
- 카트리지를 두 개 꽂으면(실기에선 I/O 충돌이지만 openMSX 는 허용) 이름이 겹치지 않게 `BaDCaT (1)` → 커넥터 `badcat_1-rs232` · 설정 `badcat_1_rx_backpressure` 가 된다.

---

## 2. 사용법

```bash
# 확장으로 꽂는다(share/extensions/BaDCaT.xml)
openmsx -machine Panasonic_FS-A1F -ext BaDCaT

# TCL 콘솔(또는 -command): TCP 로 외부 프로그램과 잇기
set rs232-net-ip232 false              # ★텔넷 IAC(0xFF…)를 그대로 넘기려면 IP232 프로토콜을 끈다(기본 ON)
set rs232-net-address 127.0.0.1:25232
plug badcat-rs232 rs232-net

# 호스트 시리얼 장치(예: /dev/ttyUSB0)와 직접 잇기
plug badcat-rs232 rs232-host

# 실기처럼 넘치게(손실 시험)
set badcat_rx_backpressure false
```

- I/O 포트만 쓰는 장치라 **카트리지 슬롯을 차지하지 않는다**(`<io base="0x80" num="8"/>`) — 디스크·DOS2 카트리지와 함께 꽂아도 슬롯이 모자라지 않다.
- `plug` 가 시작 명령에서 실패하면 openMSX 가 종료한다 — 스크립트에서 오타를 바로 알 수 있다.

### 2.1 이 작업공간에서의 사용
- `DKFS_retro/prototype_20_MSX_ROM_MSXDOS` — `run_openmsx.sh`(ROM) · `run_openmsx_dos2.sh` · `run_openmsx_dos1.sh` 의 **`--badcat`**:
  `-ext BaDCaT` + `set badcat_rx_backpressure $BADCAT_BP`(환경 `BADCAT_BP` · 기본 `true`) + `plug badcat-rs232 rs232-net`(gtm_sim 경유).
  클라이언트는 같은 스위치로 BaDCaT 변종(16C550 드라이버 · 수신 인터럽트)으로 빌드된다.
- `Examples/prototype_12_MSX_ROM_MSXDOS_BADCAT` — BaDCaT 시험 프로그램(BCTEST) · 개발 기록 `PLAN_OPENMSX_BADCAT.md` · `HANDOFF_OPENMSX_BADCAT.md`.
- 실기 확인(2026-10-17 ~ 10-19 · 같은 클라이언트의 BaDCaT 판): DOS2 판 = CPC-400 + BaDCaT 전 경로 · DOS1 판 = 메모리 개조 FS-A1 + BaDCaT · ROM 판 = PicoVerse 2040 카트리지 — 에뮬레이터에서 먼저 확인한 동작과 같았다.
- ⚠`badcat_rx_backpressure false` + FS-A1F 조합은 **실기보다 비관적**이다(이 기종의 VBlank 인터럽트 처리가 길어 폴링 수신이 실기보다 더 잃는다) — 손실 대책의 **존재** 확인용이지 손실률 예측용이 아니다.

---

## 3. 구현 파일

| 파일 | 역할 |
|---|---|
| `src/serial/NS16550.cc/.hh` | 16C550 UART 코어(레지스터·FIFO·타이밍·인터럽트·흐름 제어) · 다른 장치에서도 재사용 가능한 형태 |
| `src/serial/MSXBaDCaT.cc/.hh` | MSX 장치: I/O 매핑 · `/INT`(IRQHelper) · `RS232Connector`(`badcat-rs232`) · 백프레셔 설정 · 세이브스테이트(버전 1) |
| `share/extensions/BaDCaT.xml` | 확장 정의(`-ext BaDCaT`) |
| `src/DeviceFactory.cc` · `src/meson.build` · `build/msvc/openmsx.vcxproj(.filters)` | 등록 |

- 빌드 의존성 추가 없음(`HOWTO_COMPILE.md` 그대로).

---

## 4. 관련 fork 수정 (시리얼)

| 커밋 | 내용 |
|---|---|
| `2133181e8` · `a268e42fb` | **`rs232-host`** pluggable — RS-232 포트를 호스트 시리얼 장치(`/dev/ttyUSB0` · socat pty)에 직결 · 모뎀 라인 캐시(포트 읽기마다 ioctl 하지 않음) · `isRxBusy()` 로 i8251 RXRDY 중 전달 보류 |
| `f02010e81` | `RS232Net::signal` 에 `isRxBusy()` 백프레셔(9600 bps 수신 폭주 때 i8251 1 바이트 버퍼 오버런 방지) · `RS232Host` 송신 EAGAIN 재시도 |
| `47094ef8f` | BaDCaT 카트리지(이 문서) |

---

*마지막 업데이트: 2026-10-19*
