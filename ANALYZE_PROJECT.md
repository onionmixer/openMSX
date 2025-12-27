# openMSX 프로젝트 분석 보고서

## 1. 프로젝트 개요

openMSX는 MSX 컴퓨터를 에뮬레이트하는 오픈소스 프로젝트입니다. "완벽한 에뮬레이션"을 목표로 하며, Z80 및 R800 CPU를 지원합니다.

### 1.1 디렉토리 구조

```
openMSX/src/
├── cpu/           # CPU 에뮬레이션 (Z80, R800)
├── memory/        # 메모리 관리 (RAM, ROM, 매퍼)
├── debugger/      # 디버거 코어 기능
├── imgui/         # Dear ImGui 기반 디버거 UI
├── video/         # VDP 에뮬레이션
├── sound/         # 사운드 칩 에뮬레이션
├── fdc/           # 플로피 디스크 컨트롤러
├── ide/           # IDE 장치
├── serial/        # 시리얼 통신
├── input/         # 입력 장치
├── console/       # 콘솔 UI
├── commands/      # Tcl 명령어 시스템
├── config/        # 설정 관리
├── events/        # 이벤트 시스템
├── file/          # 파일 I/O
├── settings/      # 사용자 설정
├── utils/         # 유틸리티 함수
└── 3rdparty/      # 서드파티 라이브러리 (ImGui 등)
```

---

## 2. CPU 디버거 분석

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
│  src/cpu/MSXCPUInterface.hh                                 │
├─────────────────────────────────────────────────────────────┤
│         MSXCPU                    │       CPUCore<T>         │
│  src/cpu/MSXCPU.hh               │  src/cpu/CPUCore.hh      │
├──────────────────────────────────┼───────────────────────────┤
│       Z80TYPE                    │       R800TYPE            │
└──────────────────────────────────┴───────────────────────────┘
```

### 2.2 핵심 클래스 상세 분석

#### 2.2.1 MSXCPU (`src/cpu/MSXCPU.hh`)

MSX CPU를 추상화하는 최상위 클래스입니다.

```cpp
class MSXCPU final : private Observer<Setting> {
public:
    enum class Type : uint8_t { Z80, R800 };

    // CPU 리셋
    void doReset(EmuTime time);

    // Z80/R800 전환
    void setActiveCPU(Type cpu);

    // 인터럽트 처리
    void raiseIRQ();
    void lowerIRQ();
    void raiseNMI();
    void lowerNMI();

    // 메모리 캐시 무효화
    void invalidateAllSlotsRWCache(uint16_t start, unsigned size);

    // 레지스터 접근
    CPURegs& getRegisters();

private:
    std::unique_ptr<CPUCore<Z80TYPE>> z80;
    std::unique_ptr<CPUCore<R800TYPE>> r800;  // nullptr 가능
    bool z80Active{true};
};
```

**주요 기능:**
- Z80과 R800 CPU 간 전환 지원 (turboR)
- IRQ/NMI 인터럽트 관리
- 메모리 슬롯 캐시 관리
- 디버그용 레지스터 접근 제공

#### 2.2.2 CPUCore (`src/cpu/CPUCore.hh`)

템플릿 기반 CPU 코어 구현입니다.

```cpp
template<typename CPU_POLICY>
class CPUCore final : public CPUBase, public CPURegs, public CPU_POLICY {
public:
    void execute(bool fastForward);
    void doReset(EmuTime time);

    // CPU 루프 종료 요청
    void exitCPULoopSync();   // 동기 (메인 스레드)
    void exitCPULoopAsync();  // 비동기 (다른 스레드)

    // 인터럽트
    void raiseIRQ();
    void lowerIRQ();
    void raiseNMI();
    void lowerNMI();

private:
    // 메모리 캐시
    std::array<const uint8_t*, CacheLine::NUM> readCacheLine;
    std::array<uint8_t*, CacheLine::NUM> writeCacheLine;

    // 프로브 (디버거용)
    Probe<int> IRQStatus;
    Probe<void> IRQAccept;

    std::atomic<bool> exitLoop = false;
    bool tracingEnabled;
};
```

**핵심 설계:**
- `CPU_POLICY` 템플릿으로 Z80/R800 차이 추상화
- 메모리 캐시 라인으로 읽기/쓰기 성능 최적화
- `exitLoop` atomic 변수로 스레드 안전한 중단 지원
- 명령어 트레이싱 지원

#### 2.2.3 CPURegs (`src/cpu/CPURegs.hh`)

Z80/R800 레지스터 표현 클래스입니다.

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
    bool getIFF1() const;    // 인터럽트 플래그
    bool getIFF2() const;
    uint8_t getHALT() const;

    // 명령어 시퀀스 추적
    void setCurrentEI();     // EI 명령 실행 중
    void setCurrentLDAI();   // LD A,I/R 명령 실행 중
    bool prevWasEI() const;  // 이전 명령이 EI였는지

private:
    z80regPair AF_, BC_, DE_, HL_;
    z80regPair AF2_, BC2_, DE2_, HL2_;
    z80regPair IX_, IY_, PC_, SP_;
    uint8_t Rmask;  // Z80: 0x7F, R800: 0xFF
};
```

**특징:**
- 엔디안 독립적 레지스터 쌍 접근
- 명령어 시퀀스 추적 (EI 후 인터럽트 지연 등)
- Z80/R800 리프레시 레지스터 차이 처리

### 2.3 브레이크포인트 시스템

#### 2.3.1 BreakPointBase (`src/cpu/BreakPointBase.hh`)

모든 브레이크포인트/워치포인트의 CRTP 기반 클래스입니다.

```cpp
template<typename Derived>
class BreakPointBase {
public:
    unsigned getId() const;
    std::string getIdStr() const;  // "bp#1", "wp#2" 등

    TclObject getCondition() const;
    TclObject getCommand() const;
    bool isEnabled() const;
    bool onlyOnce() const;

    // 조건 검사 및 명령 실행
    bool checkAndExecute(GlobalCliComm& cliComm, Interpreter& interp);

private:
    TclObject command{"debug break"};  // 기본 명령
    TclObject condition;                // Tcl 조건식
    bool enabled = true;
    bool once = false;
    bool executing = false;  // 재귀 실행 방지

    static inline unsigned lastId = 0;  // 전역 ID 카운터
};
```

#### 2.3.2 BreakPoint (`src/cpu/BreakPoint.hh`)

주소 기반 브레이크포인트입니다.

```cpp
class BreakPoint final : public BreakPointBase<BreakPoint> {
public:
    static constexpr std::string_view prefix = "bp#";

    std::optional<uint16_t> getAddress() const;
    TclObject getAddressString() const;

    void setAddress(Interpreter& interp, const TclObject& addr);
    void evaluateAddress(Interpreter& interp);  // Tcl 표현식 평가

private:
    TclObject addrStr;
    std::optional<uint16_t> address;  // 평가된 주소
};
```

**특징:**
- Tcl 표현식으로 동적 주소 지정 가능 (예: `[reg PC] + 0x100`)
- 주소 평가 실패 시 `std::optional` 사용

#### 2.3.3 WatchPoint (`src/cpu/WatchPoint.hh`)

메모리/IO 감시 포인트입니다.

```cpp
class WatchPoint final : public BreakPointBase<WatchPoint> {
public:
    static constexpr std::string_view prefix = "wp#";

    enum class Type : uint8_t {
        READ_IO,    // I/O 포트 읽기
        WRITE_IO,   // I/O 포트 쓰기
        READ_MEM,   // 메모리 읽기
        WRITE_MEM   // 메모리 쓰기
    };

    Type getType() const;
    auto getBeginAddress() const;
    auto getEndAddress() const;

    // I/O 감시 등록/해제
    void registerIOWatch(MSXMotherBoard& motherBoard,
                         std::span<MSXDevice*, 256> devices);
    void unregisterIOWatch(std::span<MSXDevice*, 256> devices);

private:
    Type type = Type::WRITE_MEM;
    TclObject beginAddrStr, endAddrStr;
    std::optional<uint16_t> beginAddr, endAddr;  // [begin, end] 범위

    std::vector<std::unique_ptr<MSXWatchIODevice>> ios;
};
```

**I/O 워치 구현:**
```cpp
class MSXWatchIODevice final : public MSXMultiDevice {
    uint8_t readIO(uint16_t port, EmuTime time) override;
    void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

private:
    WatchPoint& wp;
    MSXDevice* device = nullptr;  // 래핑된 실제 장치
};
```

I/O 워치는 실제 장치를 래핑하여 읽기/쓰기를 가로챕니다.

#### 2.3.4 DebugCondition (`src/cpu/DebugCondition.hh`)

주소와 무관한 조건부 중단점입니다.

```cpp
class DebugCondition final : public BreakPointBase<DebugCondition> {
public:
    static constexpr std::string_view prefix = "cond#";
    // BreakPointBase의 condition과 command만 사용
};
```

### 2.4 MSXCPUInterface - 브레이크포인트 관리

```cpp
class MSXCPUInterface {
public:
    // 브레이크포인트 관리
    void insertBreakPoint(BreakPoint bp);
    void removeBreakPoint(const BreakPoint& bp);
    void removeBreakPoint(unsigned id);
    static BreakPoints& getBreakPoints();  // 정적 컨테이너

    // 워치포인트 관리
    void setWatchPoint(const std::shared_ptr<WatchPoint>& watchPoint);
    void removeWatchPoint(std::shared_ptr<WatchPoint> watchPoint);
    const WatchPoints& getWatchPoints() const;

    // 디버그 조건 관리
    void setCondition(DebugCondition cond);
    void removeCondition(const DebugCondition& cond);
    static Conditions& getConditions();

    // 브레이크 상태
    static bool isBreaked();
    void doBreak();
    void doContinue();
    void doStep();

    // 브레이크포인트 검사 (CPU 루프에서 호출)
    static bool anyBreakPoints();
    bool checkBreakPoints(unsigned pc);

private:
    static inline BreakPoints breakPoints;  // 정렬되지 않음
    WatchPoints watchPoints;                // 생성 순서
    static inline Conditions conditions;    // 생성 순서
    static inline bool breaked = false;
};
```

**브레이크포인트 검사 흐름:**
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

## 3. 메모리 디버거 분석

### 3.1 Debuggable 인터페이스

```cpp
class Debuggable {
public:
    virtual unsigned getSize() const = 0;
    virtual std::string_view getDescription() const = 0;
    virtual uint8_t read(unsigned address) = 0;
    virtual void write(unsigned address, uint8_t value) = 0;

    // 블록 읽기 (기본 구현 제공)
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

### 3.2 SimpleDebuggable

Debuggable의 편의 구현체입니다.

```cpp
class SimpleDebuggable : public Debuggable {
public:
    unsigned getSize() const final;
    std::string_view getDescription() const final;

    uint8_t read(unsigned address) override;
    uint8_t read(unsigned address, EmuTime time);  // 시간 기반
    void write(unsigned address, uint8_t value) override;
    void write(unsigned address, uint8_t value, EmuTime time);

    const std::string& getName() const;

protected:
    SimpleDebuggable(MSXMotherBoard& motherBoard,
                     std::string name,
                     static_string_view description,
                     unsigned size);
    ~SimpleDebuggable();  // 자동 등록 해제
};
```

### 3.3 MSXCPUInterface 메모리 디버깅

```cpp
class MSXCPUInterface {
    // 메모리 읽기 (캐시 사용)
    uint8_t readMem(uint16_t address, EmuTime time) {
        if (disallowReadCache[address >> CacheLine::BITS]) {
            return readMemSlow(address, time);  // 워치포인트 등
        }
        return visibleDevices[address >> 14]->readMem(address, time);
    }

    // 피크 (부작용 없는 읽기)
    uint8_t peekMem(uint16_t address, EmuTime time) const;
    uint8_t peekSlottedMem(unsigned address, EmuTime time) const;

    // 슬롯 지정 읽기/쓰기
    uint8_t readSlottedMem(unsigned address, EmuTime time);
    void writeSlottedMem(unsigned address, uint8_t value, EmuTime time);

private:
    // 디버거블
    struct MemoryDebug final : SimpleDebuggable { /* ... */ };
    struct SlottedMemoryDebug final : SimpleDebuggable { /* ... */ };
    struct IODebug final : SimpleDebuggable { /* ... */ };

    // 워치포인트 비트셋
    std::array<std::bitset<CacheLine::SIZE>, CacheLine::NUM> readWatchSet;
    std::array<std::bitset<CacheLine::SIZE>, CacheLine::NUM> writeWatchSet;

    // 캐시 비활성화 플래그
    std::array<uint8_t, CacheLine::NUM> disallowReadCache;
    std::array<uint8_t, CacheLine::NUM> disallowWriteCache;
};
```

### 3.4 CheckedRam - 미초기화 메모리 탐지

```cpp
class CheckedRam final : private Observer<Setting> {
public:
    uint8_t read(size_t addr);   // 초기화 검사 포함
    uint8_t peek(size_t addr) const;  // 검사 없음
    void write(size_t addr, uint8_t value);

    const uint8_t* getReadCacheLine(size_t addr) const;
    uint8_t* getWriteCacheLine(size_t addr);

    Ram& getUncheckedRam();  // 검사 없는 직접 접근

private:
    std::vector<bool> completely_initialized_cacheline;
    std::vector<std::bitset<CacheLine::SIZE>> uninitialized;
    Ram ram;
    TclCallback umrCallback;  // 미초기화 읽기 콜백
};
```

**동작 원리:**
1. 쓰기 시 해당 바이트를 초기화됨으로 표시
2. 읽기 시 미초기화 바이트 접근하면 `umrCallback` 호출
3. 캐시라인 전체가 초기화되면 `completely_initialized_cacheline`로 빠른 검사

### 3.5 메모리 워치포인트 처리

```cpp
// MSXCPUInterface.cc
void MSXCPUInterface::executeMemWatch(WatchPoint::Type type,
                                       unsigned address,
                                       unsigned value) {
    if (fastForward) return;  // 빨리감기 중엔 무시

    for (auto& wp : watchPoints) {
        if (!wp->getBeginAddress()) continue;
        if (wp->getType() != type) continue;

        unsigned begin = *wp->getBeginAddress();
        unsigned end = wp->getEndAddress().value_or(begin);

        if (address >= begin && address <= end) {
            // Tcl에 $wp_last_address, $wp_last_value 설정
            if (wp->checkAndExecute(cliComm, interp)) {
                removeWatchPoint(wp);
            }
        }
    }
}
```

---

## 4. 디버거 UI 분석 (ImGui)

### 4.1 ImGuiDebugger - 메인 디버거 창

```cpp
class ImGuiDebugger final : public ImGuiPart {
public:
    void paint(MSXMotherBoard* motherBoard) override;
    void showMenu(MSXMotherBoard* motherBoard) override;

    // 이벤트
    void signalBreak();
    void signalContinue();
    void signalQuit();
    void setGotoTarget(uint16_t target);

private:
    void drawControl(MSXCPUInterface&, MSXMotherBoard&);  // 제어 버튼
    void drawSlots(MSXCPUInterface&, Debugger&);          // 슬롯 정보
    void drawStack(const CPURegs&, const MSXCPUInterface&, EmuTime);
    void drawRegisters(CPURegs&);
    void drawFlags(CPURegs&);

    // 디버그 동작
    void actionBreakContinue(MSXCPUInterface&);
    void actionStepIn(MSXCPUInterface&);
    void actionStepOver();
    void actionStepOut();
    void actionStepBack();

private:
    std::vector<std::unique_ptr<ImGuiDisassembly>> disassemblyViewers;
    std::vector<std::unique_ptr<DebuggableEditor>> hexEditors;

    bool showControl = false;
    bool showSlots = false;
    bool showStack = false;
    bool showRegisters = false;
    bool showFlags = false;
};
```

### 4.2 ImGuiBreakPoints - 브레이크포인트 관리 UI

```cpp
class ImGuiBreakPoints final : public ImGuiPart {
public:
    void paint(MSXMotherBoard* motherBoard) override;
    void refreshSymbols();
    void paintBpTab(MSXCPUInterface& cpuInterface, uint16_t addr);

private:
    template<typename Type>
    void paintTab(MSXCPUInterface& cpuInterface,
                  std::optional<uint16_t> addr = {});

    template<typename Item>
    void drawRow(MSXCPUInterface& cpuInterface, int row, Item& item);

    std::string displayAddr(const TclObject& addr) const;
    void editRange(MSXCPUInterface&, std::shared_ptr<WatchPoint>&,
                   std::string& begin, std::string& end);
    bool editCondition(ParsedSlotCond& slot);
};
```

**슬롯 조건 파싱:**
```cpp
struct ParsedSlotCond {
    std::string rest;      // 나머지 조건
    int ps = 0;           // 프라이머리 슬롯
    int ss = 0;           // 세컨더리 슬롯
    uint8_t seg = 0;      // 세그먼트
    bool hasPs, hasSs, hasSeg;

    std::string toTclExpression(std::string_view checkCmd) const;
    std::string toDisplayString() const;
};
```

### 4.3 ImGuiDisassembly - 디스어셈블리 뷰

```cpp
class ImGuiDisassembly final : public ImGuiPart {
public:
    void paint(MSXMotherBoard* motherBoard) override;
    void signalBreak();
    void setGotoTarget(uint16_t target);
    void syncWithPC();

    void actionToggleBp(MSXMotherBoard& motherBoard);

private:
    unsigned disassemble(
        const MSXCPUInterface& cpuInterface,
        unsigned addr, unsigned pc, EmuTime time,
        std::span<uint8_t, 4> opcodes,
        std::string& mnemonic,
        std::optional<uint16_t>& mnemonicAddr,
        std::span<const Symbol* const>& mnemonicLabels);

    void disassembleToClipboard(/*...*/);

private:
    SymbolManager& symbolManager;
    std::optional<uint16_t> selectedAddr;
    std::string gotoAddr;
    std::string runToAddr;
    bool followPC = false;
    bool scrollToPcOnBreak = true;
};
```

### 4.4 DebuggableEditor - 헥스 에디터

```cpp
class DebuggableEditor final : public ImGuiPart {
public:
    explicit DebuggableEditor(ImGuiManager& manager,
                              std::string debuggableName,
                              size_t index);

    void makeSnapshot(MSXMotherBoard& motherBoard);
    void makeSnapshot(Debuggable& debuggable);
    void paint(MSXMotherBoard* motherBoard) override;

private:
    void drawContents(const Sizes& s, Debuggable& debuggable, unsigned memSize);
    void drawSearch(const Sizes& s, Debuggable& debuggable, unsigned memSize);
    void drawPreviewLine(const Sizes& s, Debuggable& debuggable, unsigned memSize);
    void drawExport(const Sizes& s, Debuggable& debuggable);

private:
    int columns = 16;
    bool showAscii = true;
    bool showSearch = false;
    bool showDataPreview = false;
    bool showSymbolInfo = false;
    bool greyOutZeroes = true;

    std::string searchString;
    std::vector<uint8_t> snapshot;  // 변경 감지용
};
```

**기능:**
- 16진수/ASCII 뷰
- 검색 (HEX/ASCII)
- 데이터 프리뷰 (다양한 타입)
- 심볼 정보 표시
- 내보내기 (클립보드/파일)
- 변경 하이라이트

---

## 5. 심볼 관리

### 5.1 SymbolManager (`src/debugger/SymbolManager.hh`)

```cpp
struct Symbol {
    std::string name;
    uint16_t value;
    std::optional<uint8_t> slot;
    std::optional<uint16_t> segment;
};

struct SymbolFile {
    enum class Type : uint8_t {
        AUTO_DETECT,
        ASMSX, GENERIC, HTC, LINKMAP, NOICE, VASM, WLALINK_NOGMB
    };

    std::string filename;
    std::vector<Symbol> symbols;
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
    std::optional<uint16_t> parseSymbolOrValue(std::string_view s) const;

private:
    std::vector<SymbolFile> files;
    hash_map<uint16_t, std::vector<const Symbol*>> lookupValueCache;
};
```

**지원 심볼 파일 형식:**
- ASMSX
- Generic (PASMO, SJASM, TNIASM)
- HTC
- LinkMap
- NoICE
- VASM
- WLALINK (no GMB)

---

## 6. Tcl 디버거 API

### 6.1 debug 명령어

```tcl
# 디버거블 관리
debug list                          # 등록된 디버거블 목록
debug desc <debuggable>             # 설명
debug size <debuggable>             # 크기
debug read <debuggable> <addr>      # 읽기
debug write <debuggable> <addr> <value>  # 쓰기
debug read_block <debuggable> <addr> <size>

# 디스어셈블
debug disasm <addr>                 # 단일 명령어
debug disasm_blob <binary>          # 바이너리 데이터

# 브레이크포인트
debug break                         # 즉시 중단
debug cont                          # 계속 실행
debug step                          # 한 단계 실행

debug set_bp <addr> [cond] [cmd]    # BP 설정
debug remove_bp <id>                # BP 제거
debug list_bp                       # BP 목록

debug breakpoint create -address <addr> [-condition <cond>] [-command <cmd>]
debug breakpoint remove <id>
debug breakpoint configure <id> [-address <addr>] [-enabled <bool>]

# 워치포인트
debug set_watchpoint <type> <addr> [cond] [cmd]
debug remove_watchpoint <id>
debug list_watchpoints

debug watchpoint create -type <type> -address <addr> [-condition <cond>]
debug watchpoint remove <id>

# 조건
debug set_condition <cond> [cmd]
debug remove_condition <id>
debug list_conditions

debug condition create -condition <cond> [-command <cmd>]
debug condition remove <id>

# 프로브
debug probe list                    # 프로브 목록
debug probe desc <probe>            # 프로브 설명
debug probe read <probe>            # 프로브 값 읽기
debug probe set_bp <probe> [cond] [cmd]  # 프로브 BP
debug probe remove_bp <id>

# 심볼
debug symbols types                 # 지원 파일 형식
debug symbols load <file> [type]    # 심볼 파일 로드
debug symbols remove <file>         # 심볼 파일 제거
debug symbols files                 # 로드된 파일 목록
debug symbols lookup <name|value>   # 심볼 검색
```

---

## 7. 디버거 아키텍처 다이어그램

```
┌─────────────────────────────────────────────────────────────────────┐
│                           UI Layer (ImGui)                          │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌─────────────┐ │
│  │ImGuiDebugger │ │ImGuiBreak    │ │ImGuiDisasm   │ │Debuggable   │ │
│  │(Control/Regs)│ │Points        │ │              │ │Editor       │ │
│  └──────┬───────┘ └──────┬───────┘ └──────┬───────┘ └──────┬──────┘ │
└─────────┼────────────────┼────────────────┼─────────────────┼───────┘
          │                │                │                 │
          ▼                ▼                ▼                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         Debugger Core                                │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │                    Debugger (Tcl Command)                      │ │
│  │  - Debuggable 등록/관리                                        │ │
│  │  - Probe 등록/관리                                             │ │
│  │  - ProbeBreakPoint 관리                                        │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                                                                      │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐   │
│  │   BreakPoint     │  │   WatchPoint     │  │  DebugCondition  │   │
│  │   (주소 기반)    │  │   (메모리/IO)    │  │   (조건만)       │   │
│  └────────┬─────────┘  └────────┬─────────┘  └────────┬─────────┘   │
│           │                     │                     │              │
│           └─────────────────────┼─────────────────────┘              │
│                                 ▼                                    │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │                     MSXCPUInterface                            │ │
│  │  - static BreakPoints                                          │ │
│  │  - WatchPoints                                                 │ │
│  │  - static Conditions                                           │ │
│  │  - checkBreakPoints(pc) - CPU 루프에서 호출                    │ │
│  │  - doBreak() / doContinue() / doStep()                        │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                     SymbolManager                             │   │
│  │  - 심볼 파일 로드/관리                                        │   │
│  │  - 주소 ↔ 심볼 검색                                          │   │
│  └──────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          CPU Layer                                   │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │                         MSXCPU                                 │ │
│  │  ┌─────────────────────┐  ┌─────────────────────┐              │ │
│  │  │  CPUCore<Z80TYPE>   │  │  CPUCore<R800TYPE>  │              │ │
│  │  │  - execute()        │  │  - execute()        │              │ │
│  │  │  - exitCPULoopSync  │  │  - exitCPULoopAsync │              │ │
│  │  └──────────┬──────────┘  └──────────┬──────────┘              │ │
│  │             │                        │                          │ │
│  │             └────────────┬───────────┘                          │ │
│  │                          ▼                                      │ │
│  │  ┌────────────────────────────────────────────────────────────┐│ │
│  │  │                      CPURegs                               ││ │
│  │  │  AF, BC, DE, HL, IX, IY, SP, PC                           ││ │
│  │  │  AF', BC', DE', HL', I, R, IM, IFF1, IFF2                 ││ │
│  │  └────────────────────────────────────────────────────────────┘│ │
│  └────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        Memory Layer                                  │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │                    Debuggable Interface                        │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                                                                      │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌─────────────┐ │
│  │ MemoryDebug  │ │SlottedMemory │ │   IODebug    │ │ CheckedRam  │ │
│  │ (64KB view)  │ │Debug(all slt)│ │  (256 ports) │ │ (UMR detect)│ │
│  └──────────────┘ └──────────────┘ └──────────────┘ └─────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 8. 주요 설계 패턴

### 8.1 CRTP (Curiously Recurring Template Pattern)
`BreakPointBase<Derived>`에서 사용:
- ID 생성 및 문자열화
- 조건 검사 및 명령 실행 공통 로직

### 8.2 Observer Pattern
- `MSXCPU` → `Setting` 변경 감시
- `CheckedRam` → 설정 변경 감시
- `SymbolObserver` → 심볼 변경 알림

### 8.3 Wrapper Pattern
`MSXWatchIODevice`가 실제 I/O 장치를 래핑하여 감시

### 8.4 Template Method
`SimpleDebuggable`의 시간 기반/비기반 read/write 변형

### 8.5 Singleton-like Static State
```cpp
// MSXCPUInterface
static inline BreakPoints breakPoints;
static inline Conditions conditions;
static inline bool breaked = false;
```
모든 MSX 머신 간 공유되는 디버거 상태

---

## 9. 성능 최적화

### 9.1 메모리 캐시 라인
```cpp
namespace CacheLine {
    static constexpr unsigned BITS = 8;
    static constexpr unsigned SIZE = 1 << BITS;  // 256 bytes
    static constexpr unsigned NUM = 0x10000 / SIZE;  // 256 lines
}
```

### 9.2 빠른 경로 / 느린 경로
```cpp
uint8_t readMem(uint16_t address, EmuTime time) {
    if (disallowReadCache[address >> BITS]) [[unlikely]] {
        return readMemSlow(address, time);  // 워치포인트 등
    }
    return visibleDevices[address >> 14]->readMem(address, time);
}
```

### 9.3 Fast-Forward 모드
```cpp
void setFastForward(bool fastForward_) { fastForward = fastForward_; }
// fastForward 중에는 브레이크포인트 무시
```

---

## 10. 결론

openMSX의 디버거 시스템은 다음과 같은 특징을 가집니다:

1. **계층적 설계**: UI → Core → CPU → Memory
2. **Tcl 스크립팅**: 모든 디버거 기능에 프로그래밍 가능한 인터페이스
3. **다양한 중단점**: 주소 BP, 메모리/IO WP, 조건부 중단
4. **심볼 지원**: 다양한 어셈블러 형식의 심볼 파일 지원
5. **성능 고려**: 캐시 라인, fast-forward 모드, 조건부 느린 경로
6. **확장성**: Debuggable 인터페이스로 새 디버깅 대상 추가 용이

이 디버거는 MSX 개발자가 어셈블리 레벨에서 프로그램을 분석하고 디버깅하는 데 필요한 모든 기능을 제공합니다.
