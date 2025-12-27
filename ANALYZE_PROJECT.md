# openMSX 프로젝트 분석 보고서

## 1. 프로젝트 개요

openMSX는 MSX 컴퓨터를 에뮬레이트하는 오픈소스 프로젝트입니다. "완벽한 에뮬레이션"을 목표로 하며, Z80 및 R800 CPU를 지원합니다.

이 문서는 **onionmixer/openMSX** fork 버전을 기준으로 작성되었습니다.

### 1.1 디렉토리 구조 및 파일 통계

```
openMSX/
├── src/                    # 소스 코드 (500+ .cc, 700+ .hh 파일)
│   ├── cpu/               # CPU 에뮬레이션 (12 .cc, 18 .hh)
│   ├── memory/            # 메모리 관리 (91 .cc, 93 .hh)
│   ├── debugger/          # 디버거 코어 + HTTP 서버 (11 .cc, 12 .hh)
│   ├── imgui/             # Dear ImGui 디버거 UI (38 .cc, 43 .hh)
│   ├── video/             # VDP 에뮬레이션 (38 .cc, 49 .hh)
│   │   ├── scalers/       # 픽셀 스케일러 (18 파일)
│   │   ├── v9990/         # V9990 VDP (22 파일)
│   │   └── ld/            # Laserdisc VDP (8 파일)
│   ├── sound/             # 사운드 칩 (53 .cc, 58 .hh)
│   ├── fdc/               # 플로피 디스크 컨트롤러 (46 .cc, 47 .hh)
│   ├── ide/               # IDE 장치 (18 .cc, 21 .hh)
│   ├── serial/            # 시리얼 통신 (30 .cc, 31 .hh)
│   ├── input/             # 입력 장치 (26 .cc, 30 .hh)
│   ├── cassette/          # 카세트 테이프 (11 .cc, 11 .hh)
│   ├── laserdisc/         # Laserdisc 플레이어 (5 .cc, 5 .hh)
│   ├── console/           # 콘솔 UI (9 .cc, 9 .hh)
│   ├── commands/          # Tcl 명령어 시스템 (13 .cc, 17 .hh)
│   ├── config/            # 설정 관리 (4 .cc, 8 .hh)
│   ├── events/            # 이벤트 시스템 (18 .cc, 20 .hh)
│   ├── file/              # 파일 I/O (14 .cc, 19 .hh)
│   ├── settings/          # 사용자 설정 (13 .cc, 13 .hh)
│   ├── utils/             # 유틸리티 함수 (16 .cc, 85 .hh)
│   ├── thread/            # 스레딩 (2 .cc, 2 .hh)
│   ├── security/          # 보안 함수 (3 .cc, 3 .hh)
│   └── 3rdparty/          # 서드파티 라이브러리
├── build/                  # 빌드 시스템
│   ├── main.mk            # 메인 빌드 스크립트
│   ├── 3rdparty.mk        # 3rd party 빌드
│   ├── flavour-*.mk       # 빌드 플레이버 (13개)
│   ├── platform-*.mk      # 플랫폼별 설정 (12개)
│   └── *.py               # Python 빌드 도구
├── share/                  # 리소스 파일 (660+ 파일)
│   ├── machines/          # 머신 정의 XML (100+)
│   ├── scripts/           # Tcl 스크립트 (67개)
│   ├── extensions/        # 하드웨어 확장
│   ├── systemroms/        # 시스템 ROM 정의
│   ├── software/          # 소프트웨어 ROM DB
│   └── skins/             # UI 스킨
├── doc/                    # 문서
└── derived/                # 빌드 결과물 (자동 생성)
```

### 1.2 주요 컴포넌트별 파일 수

| 컴포넌트 | .cc 파일 | .hh 파일 | 합계 | 설명 |
|----------|----------|----------|------|------|
| memory/ | 91 | 93 | 184 | 메모리 매핑, ROM/RAM, 카트리지 |
| sound/ | 53 | 58 | 111 | 사운드 칩 에뮬레이션 |
| fdc/ | 46 | 47 | 93 | 플로피 디스크 컨트롤러 |
| video/ | 38 | 49 | 87 | VDP 에뮬레이션 |
| imgui/ | 38 | 43 | 81 | Dear ImGui 디버거 UI |
| serial/ | 30 | 31 | 61 | 시리얼/MIDI 통신 |
| input/ | 26 | 30 | 56 | 입력 장치 |
| ide/ | 18 | 21 | 39 | IDE 하드 드라이브 |
| events/ | 18 | 20 | 38 | 이벤트 시스템 |
| file/ | 14 | 19 | 33 | 파일 I/O |
| cpu/ | 12 | 18 | 30 | CPU 에뮬레이션 코어 |
| commands/ | 13 | 17 | 30 | Tcl 명령어 |
| settings/ | 13 | 13 | 26 | 설정 관리 |
| debugger/ | 11 | 12 | 23 | 디버거 코어 + HTTP 서버 |
| cassette/ | 11 | 11 | 22 | 카세트 테이프 |

---

## 2. CPU 에뮬레이션

### 2.1 아키텍처 개요

```
┌─────────────────────────────────────────────────────────────┐
│                      Debugger (Tcl API)                      │
│  src/debugger/Debugger.hh                                   │
├─────────────────────────────────────────────────────────────┤
│  BreakPoint    │  WatchPoint     │  DebugCondition          │
│  (주소 BP)     │  (메모리/IO 감시)│  (조건부 중단)           │
├─────────────────────────────────────────────────────────────┤
│                    MSXCPUInterface                           │
│  src/cpu/MSXCPUInterface.hh (19,670 bytes)                  │
├─────────────────────────────────────────────────────────────┤
│         MSXCPU                    │       CPUCore<T>         │
│  src/cpu/MSXCPU.hh (225 lines)   │  src/cpu/CPUCore.hh      │
├──────────────────────────────────┼───────────────────────────┤
│       Z80TYPE                    │       R800TYPE            │
│   3.58 MHz (표준)                │  3.58/5.37/7.16/10.74 MHz │
└──────────────────────────────────┴───────────────────────────┘
```

### 2.2 핵심 클래스

#### 2.2.1 MSXCPU (`src/cpu/MSXCPU.hh`)

```cpp
class MSXCPU final : private Observer<Setting> {
public:
    enum class Type : uint8_t { Z80, R800 };

    void doReset(EmuTime time);
    void setActiveCPU(Type cpu);

    void raiseIRQ();
    void lowerIRQ();
    void raiseNMI();
    void lowerNMI();

    void invalidateAllSlotsRWCache(uint16_t start, unsigned size);
    CPURegs& getRegisters();

private:
    std::unique_ptr<CPUCore<Z80TYPE>> z80;
    std::unique_ptr<CPUCore<R800TYPE>> r800;  // turboR만
    bool z80Active{true};
};
```

#### 2.2.2 CPUCore (`src/cpu/CPUCore.hh`)

```cpp
template<typename CPU_POLICY>
class CPUCore final : public CPUBase, public CPURegs, public CPU_POLICY {
public:
    void execute(bool fastForward);
    void doReset(EmuTime time);

    void exitCPULoopSync();   // 동기 (메인 스레드)
    void exitCPULoopAsync();  // 비동기 (다른 스레드)

    void raiseIRQ();
    void lowerIRQ();
    void raiseNMI();
    void lowerNMI();

private:
    std::array<const uint8_t*, CacheLine::NUM> readCacheLine;
    std::array<uint8_t*, CacheLine::NUM> writeCacheLine;

    Probe<int> IRQStatus;
    Probe<void> IRQAccept;

    std::atomic<bool> exitLoop = false;
    bool tracingEnabled;
};
```

#### 2.2.3 CPURegs (`src/cpu/CPURegs.hh`)

```cpp
class CPURegs {
public:
    // 8비트 레지스터
    uint8_t getA() const;
    uint8_t getF() const;
    uint8_t getB() const;
    // ... (C, D, E, H, L, A', F', B', C', D', E', H', L', IXh, IXl, IYh, IYl)

    // 16비트 레지스터
    uint16_t getAF() const;
    uint16_t getBC() const;
    uint16_t getDE() const;
    uint16_t getHL() const;
    uint16_t getIX() const;
    uint16_t getIY() const;
    uint16_t getPC() const;
    uint16_t getSP() const;

    // 특수 레지스터
    uint8_t getIM() const;   // 인터럽트 모드
    uint8_t getI() const;    // 인터럽트 벡터
    uint8_t getR() const;    // 리프레시 레지스터
    bool getIFF1() const;
    bool getIFF2() const;

private:
    z80regPair AF_, BC_, DE_, HL_;
    z80regPair AF2_, BC2_, DE2_, HL2_;
    z80regPair IX_, IY_, PC_, SP_;
    uint8_t Rmask;  // Z80: 0x7F, R800: 0xFF
};
```

---

## 3. 메모리 시스템

### 3.1 메모리 컴포넌트 (184 파일)

| 카테고리 | 주요 클래스 | 설명 |
|----------|-------------|------|
| 기본 | MSXMemoryMapper, MSXRam, MSXRom | 기본 메모리 |
| 카트리지 | RomPlain, RomKonami, RomASCII8/16 | ROM 매퍼 타입 |
| 고급 | MegaFlashRomSCC, Carnivore2 | 고급 카트리지 |
| 플래시 | AmdFlash | 플래시 메모리 |
| EEPROM | EEPROM_93C46 | SPI EEPROM |

### 3.2 Debuggable 인터페이스

```cpp
class Debuggable {
public:
    virtual unsigned getSize() const = 0;
    virtual std::string_view getDescription() const = 0;
    virtual uint8_t read(unsigned address) = 0;
    virtual void write(unsigned address, uint8_t value) = 0;
    virtual void readBlock(unsigned start, std::span<uint8_t> output);
};
```

**주요 Debuggable 구현체:**

| 클래스 | 설명 | 크기 |
|--------|------|------|
| `MemoryDebug` | 현재 슬롯의 64KB 메모리 | 0x10000 |
| `SlottedMemoryDebug` | 모든 슬롯의 메모리 | 0x10000 * 16 |
| `IODebug` | I/O 포트 | 0x100 |
| `MSXCPU::Debuggable` | CPU 레지스터 | ~28 바이트 |

### 3.3 CheckedRam - 미초기화 메모리 탐지

```cpp
class CheckedRam final : private Observer<Setting> {
public:
    uint8_t read(size_t addr);   // 초기화 검사 포함
    uint8_t peek(size_t addr) const;
    void write(size_t addr, uint8_t value);

private:
    std::vector<bool> completely_initialized_cacheline;
    std::vector<std::bitset<CacheLine::SIZE>> uninitialized;
    Ram ram;
    TclCallback umrCallback;  // 미초기화 읽기 콜백
};
```

---

## 4. 비디오 시스템 (VDP)

### 4.1 지원 VDP 칩

| 칩 | 기종 | VRAM | 특징 |
|----|------|------|------|
| TMS9918A | MSX1 | 16KB | 기본 VDP |
| V9938 | MSX2 | 128KB | 확장 그래픽 모드 |
| V9958 | MSX2+ | 128KB | 수평 스크롤, YJK |
| V9990 | turboR | 512KB | 고급 그래픽 |

### 4.2 렌더링 파이프라인

```
DisplayMode
    │
    ▼
BitmapConverter / CharacterConverter
    │
    ▼
PixelRenderer
    │
    ▼
RawFrame (unscaled)
    │
    ▼
PostProcessor (scalers)
    │
    ▼
Rasterizer (SDL/OpenGL)
```

### 4.3 스케일러

- **2xSaI**: 안티앨리어싱 보간
- **HQ2x/HQ3x**: 고품질 스케일링
- **Scale2x/Scale3x**: 정수배 스케일링
- **Simple**: 기본 스케일링

---

## 5. 사운드 시스템

### 5.1 사운드 칩 에뮬레이션 (111 파일)

| 칩 | 클래스 | 설명 |
|----|--------|------|
| AY-3-8910 | AY8910 | 3채널 PSG + 노이즈 |
| Y8950 | Y8950 | FM + 드럼 |
| YM2413 | MSXMusic | FM-PAC |
| YM2151 | - | OPM FM |
| SCC | SCC | 파형 메모리 |
| PCM | TurboRPCM | 8비트 DAC |

### 5.2 MSXMixer

```cpp
class MSXMixer {
public:
    void registerSound(SoundDevice& device);
    void unregisterSound(SoundDevice& device);
    void setVolume(float volume);
    void setMasterVolume(int volume);

private:
    std::vector<SoundDevice*> devices;
    float masterVolume;
};
```

---

## 6. Debug HTTP Server (Fork 추가 기능)

### 6.1 개요

이 fork에서 추가된 Debug HTTP Server는 웹 브라우저를 통해 실시간 디버깅 정보를 확인할 수 있는 기능입니다.

### 6.2 아키텍처

```
┌─────────────────────────────────────────────────────────────────┐
│                      DebugHttpServer                             │
│  - 4개 포트 관리 (65501-65504)                                  │
│  - TCL 설정 연동 (debug_http_enable)                            │
├─────────────────────────────────────────────────────────────────┤
│  DebugHttpServerPort (×4)                                       │
│  - 소켓 리스닝                                                  │
│  - 클라이언트 연결 수락                                         │
├─────────────────────────────────────────────────────────────────┤
│  DebugHttpConnection                                            │
│  - HTTP 요청 파싱                                               │
│  - HTML/JSON/SSE 응답                                           │
├──────────────────┬──────────────────────────────────────────────┤
│  HtmlGenerator   │  DebugInfoProvider                           │
│  - HTML 대시보드 │  - JSON 데이터 생성                          │
│  - CSS 스타일링  │  - 에뮬레이터 상태 수집                      │
│  - 네비게이션    │  - 스레드 안전 (mutex)                       │
└──────────────────┴──────────────────────────────────────────────┘
```

### 6.3 파일 구조

| 파일 | 크기 | 설명 |
|------|------|------|
| DebugHttpServer.hh/cc | 1.6KB + 3.0KB | 메인 서버 코디네이터 |
| DebugHttpServerPort.hh/cc | 2.2KB + 3.6KB | 포트별 리스너 |
| DebugHttpConnection.hh/cc | 2.4KB + 9.9KB | 클라이언트 연결 처리 |
| DebugInfoProvider.hh/cc | 1.7KB + 11.5KB | JSON 데이터 생성 |
| HtmlGenerator.hh/cc | 1.9KB + 26.5KB | HTML 대시보드 생성 |

### 6.4 포트 할당

| 포트 | 타입 | 정보 |
|------|------|------|
| 65501 | MACHINE | 슬롯, 드라이브, 카트리지 |
| 65502 | IO | I/O 포트 읽기/쓰기 활동 |
| 65503 | CPU | 레지스터, 플래그, PC |
| 65504 | MEMORY | 메모리 덤프/검사 |

### 6.5 HTTP 엔드포인트

| 경로 | 응답 형식 | 설명 |
|------|-----------|------|
| `/` | HTML | 시각적 대시보드 |
| `/info` | HTML/JSON | Accept 헤더에 따라 결정 |
| `/api` | JSON | API 엔드포인트 |
| `/stream` | SSE | 실시간 스트리밍 |

### 6.6 주요 클래스

#### DebugHttpServer

```cpp
class DebugHttpServer {
public:
    explicit DebugHttpServer(Reactor& reactor);
    ~DebugHttpServer();

    void setEnable(bool enable);
    [[nodiscard]] bool isEnabled() const;

private:
    void startServers();
    void stopServers();

    std::array<std::unique_ptr<DebugHttpServerPort>, 4> ports;
    std::unique_ptr<DebugInfoProvider> infoProvider;

    BooleanSetting enableSetting;
    IntegerSetting machinePortSetting;
    IntegerSetting ioPortSetting;
    IntegerSetting cpuPortSetting;
    IntegerSetting memoryPortSetting;
};
```

#### DebugInfoProvider

```cpp
class DebugInfoProvider {
public:
    explicit DebugInfoProvider(Reactor& reactor);

    [[nodiscard]] std::string getMachineInfo();
    [[nodiscard]] std::string getIOInfo();
    [[nodiscard]] std::string getCPUInfo();
    [[nodiscard]] std::string getMemoryInfo(unsigned start, unsigned size);

private:
    Reactor& reactor;
    mutable std::mutex mutex;
};
```

#### HtmlGenerator

```cpp
class HtmlGenerator {
public:
    static std::string generateMachinePage(DebugInfoProvider& provider);
    static std::string generateIOPage(DebugInfoProvider& provider);
    static std::string generateCPUPage(DebugInfoProvider& provider);
    static std::string generateMemoryPage(DebugInfoProvider& provider,
                                           unsigned start, unsigned size);

    static std::string generatePage(DebugInfoType type,
                                     DebugInfoProvider& provider,
                                     unsigned memStart = 0,
                                     unsigned memSize = 256);
private:
    static std::string wrapPage(const std::string& title,
                                const std::string& content,
                                DebugInfoType activeTab);
    static std::string getCSS();
    static std::string getNavigation(DebugInfoType activeTab);
};
```

### 6.7 사용 방법

```tcl
# TCL 콘솔에서 활성화
set debug_http_enable on

# 브라우저에서 접속
# http://127.0.0.1:65501/  (Machine)
# http://127.0.0.1:65502/  (I/O)
# http://127.0.0.1:65503/  (CPU)
# http://127.0.0.1:65504/  (Memory)
```

---

## 7. 브레이크포인트 시스템

### 7.1 브레이크포인트 타입

```cpp
// 주소 기반 브레이크포인트
class BreakPoint final : public BreakPointBase<BreakPoint> {
    static constexpr std::string_view prefix = "bp#";
    std::optional<uint16_t> getAddress() const;
};

// 메모리/IO 워치포인트
class WatchPoint final : public BreakPointBase<WatchPoint> {
    static constexpr std::string_view prefix = "wp#";
    enum class Type : uint8_t {
        READ_IO, WRITE_IO, READ_MEM, WRITE_MEM
    };
};

// 조건부 중단
class DebugCondition final : public BreakPointBase<DebugCondition> {
    static constexpr std::string_view prefix = "cond#";
};
```

### 7.2 브레이크포인트 검사 흐름

```
CPU 명령 실행
    │
    ▼
anyBreakPoints() ─── false ──► 정상 실행
    │ true
    ▼
checkBreakPoints(PC)
    │
    ├── BreakPoints 검사
    │   └── 주소 일치 && 조건 만족 → 명령 실행
    │
    └── Conditions 검사
        └── 조건 만족 → 명령 실행
```

---

## 8. 디버거 UI (ImGui)

### 8.1 주요 UI 컴포넌트

| 클래스 | 기능 |
|--------|------|
| ImGuiDebugger | 메인 디버거 창 (제어, 레지스터, 스택) |
| ImGuiBreakPoints | 브레이크포인트 관리 UI |
| ImGuiDisassembly | 디스어셈블리 뷰 |
| DebuggableEditor | 헥스 에디터 |

### 8.2 ImGuiDebugger

```cpp
class ImGuiDebugger final : public ImGuiPart {
public:
    void paint(MSXMotherBoard* motherBoard) override;

    void signalBreak();
    void signalContinue();
    void setGotoTarget(uint16_t target);

private:
    void drawControl(MSXCPUInterface&, MSXMotherBoard&);
    void drawSlots(MSXCPUInterface&, Debugger&);
    void drawStack(const CPURegs&, const MSXCPUInterface&, EmuTime);
    void drawRegisters(CPURegs&);
    void drawFlags(CPURegs&);

    void actionBreakContinue(MSXCPUInterface&);
    void actionStepIn(MSXCPUInterface&);
    void actionStepOver();
    void actionStepOut();
    void actionStepBack();

private:
    std::vector<std::unique_ptr<ImGuiDisassembly>> disassemblyViewers;
    std::vector<std::unique_ptr<DebuggableEditor>> hexEditors;
};
```

---

## 9. 심볼 관리

### 9.1 SymbolManager

```cpp
struct Symbol {
    std::string name;
    uint16_t value;
    std::optional<uint8_t> slot;
    std::optional<uint16_t> segment;
};

class SymbolManager {
public:
    bool reloadFile(const std::string& filename,
                    LoadEmpty loadEmpty,
                    SymbolFile::Type type,
                    std::optional<uint8_t> slot,
                    std::optional<uint16_t> segment);

    void removeFile(std::string_view filename);

    std::span<Symbol const * const> lookupValue(uint16_t value);
    std::optional<uint16_t> lookupSymbol(std::string_view s) const;

private:
    std::vector<SymbolFile> files;
    hash_map<uint16_t, std::vector<const Symbol*>> lookupValueCache;
};
```

### 9.2 지원 심볼 파일 형식

- ASMSX
- Generic (PASMO, SJASM, TNIASM)
- HTC
- LinkMap
- NoICE
- VASM
- WLALINK (no GMB)

---

## 10. Tcl 디버거 API

### 10.1 debug 명령어

```tcl
# 디버거블 관리
debug list                          # 등록된 디버거블 목록
debug desc <debuggable>             # 설명
debug size <debuggable>             # 크기
debug read <debuggable> <addr>      # 읽기
debug write <debuggable> <addr> <value>  # 쓰기

# 브레이크포인트
debug set_bp <addr> [cond] [cmd]    # BP 설정
debug remove_bp <id>                # BP 제거
debug list_bp                       # BP 목록

# 워치포인트
debug set_watchpoint <type> <addr> [cond] [cmd]
debug remove_watchpoint <id>
debug list_watchpoints

# 제어
debug break                         # 즉시 중단
debug cont                          # 계속 실행
debug step                          # 한 단계 실행

# 심볼
debug symbols load <file> [type]    # 심볼 파일 로드
debug symbols remove <file>         # 심볼 파일 제거
debug symbols lookup <name|value>   # 심볼 검색
```

---

## 11. 빌드 시스템

### 11.1 빌드 플레이버

| 플레이버 | 설명 |
|----------|------|
| opt | 일반 최적화 (기본값) |
| devel | 개발용 (assert, 디버그 심볼) |
| debug | 디버그용 (최적화 없음) |
| lto | 링크 타임 최적화 |

### 11.2 지원 플랫폼

- Linux
- macOS (Darwin)
- Windows (MinGW, MSVC)
- FreeBSD, OpenBSD, NetBSD
- Android

### 11.3 의존성

**필수:**
- C++23 컴파일러 (GCC 12+, Clang 17+)
- SDL2, SDL2_ttf
- libpng, zlib
- Tcl 8.6+
- POSIX threads

**선택:**
- OpenGL + GLEW (하드웨어 렌더링)
- Ogg + Theora + Vorbis (Laserdisc)
- ALSA (Linux MIDI)

---

## 12. 설계 패턴

### 12.1 CRTP (Curiously Recurring Template Pattern)

`BreakPointBase<Derived>`에서 사용:
- ID 생성 및 문자열화
- 조건 검사 및 명령 실행 공통 로직

### 12.2 Observer Pattern

- `MSXCPU` → `Setting` 변경 감시
- `CheckedRam` → 설정 변경 감시
- `SymbolObserver` → 심볼 변경 알림

### 12.3 Template Method

- `CPUCore<T>` - Z80/R800 추상화
- `SimpleDebuggable` - 시간 기반 read/write 변형

### 12.4 Factory Pattern

- 장치 팩토리 생성
- 카트리지 감지 및 인스턴스화
- 렌더러 팩토리 (SDL, OpenGL)

---

## 13. 성능 최적화

### 13.1 메모리 캐시 라인

```cpp
namespace CacheLine {
    static constexpr unsigned BITS = 8;
    static constexpr unsigned SIZE = 1 << BITS;  // 256 bytes
    static constexpr unsigned NUM = 0x10000 / SIZE;  // 256 lines
}
```

### 13.2 빠른 경로 / 느린 경로

```cpp
uint8_t readMem(uint16_t address, EmuTime time) {
    if (disallowReadCache[address >> BITS]) [[unlikely]] {
        return readMemSlow(address, time);  // 워치포인트 등
    }
    return visibleDevices[address >> 14]->readMem(address, time);
}
```

### 13.3 Fast-Forward 모드

```cpp
void setFastForward(bool fastForward_) { fastForward = fastForward_; }
// fastForward 중에는 브레이크포인트 무시
```

---

## 14. 결론

openMSX의 디버거 시스템은 다음과 같은 특징을 가집니다:

1. **계층적 설계**: UI → Core → CPU → Memory
2. **Tcl 스크립팅**: 모든 디버거 기능에 프로그래밍 가능한 인터페이스
3. **다양한 중단점**: 주소 BP, 메모리/IO WP, 조건부 중단
4. **심볼 지원**: 다양한 어셈블러 형식의 심볼 파일 지원
5. **성능 고려**: 캐시 라인, fast-forward 모드, 조건부 느린 경로
6. **확장성**: Debuggable 인터페이스로 새 디버깅 대상 추가 용이
7. **HTTP 디버그 서버** (Fork 추가): 웹 브라우저를 통한 실시간 디버깅

이 에뮬레이터는 MSX 개발자가 어셈블리 레벨에서 프로그램을 분석하고 디버깅하는 데 필요한 모든 기능을 제공합니다.

---

*마지막 업데이트: 2025-12-27*
*Fork: https://github.com/onionmixer/openMSX*
