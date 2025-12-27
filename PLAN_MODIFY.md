# openMSX HTTP 디버거 서버 개발 계획

## 1. 개요

### 1.1 목표
HTTP 클라이언트가 접속하여 openMSX 에뮬레이터의 디버깅 정보를 실시간으로 확인할 수 있는 기능을 추가합니다.

### 1.2 요구사항
- 설정 파일의 flag에 따라 활성화/비활성화
- 4개의 별도 포트로 소켓을 열고 각 유형별 정보 제공
- 외부 라이브러리 없이 내부 구현
- 라이선스 문제 없는 코드 사용

### 1.3 포트 정의
| 포트 | 정보 유형 | 설명 |
|------|-----------|------|
| 65501 | 머신 정보 | 드라이브, 슬롯, 카트리지 등 |
| 65502 | IO 정보 | I/O 포트 입출력 데이터 |
| 65503 | CPU 정보 | 레지스터, PC, 플래그 등 |
| 65504 | 메모리 정보 | 메모리 덤프, 슬롯 메모리 |

---

## 2. 기존 코드 분석

### 2.1 재사용 가능한 기존 코드

#### 2.1.1 소켓 인프라 (`src/events/Socket.hh/cc`)
```cpp
// 이미 플랫폼 독립적 소켓 API 제공
SOCKET, sock_recv(), sock_send(), sock_close(), sock_error()
SocketActivator - 소켓 서브시스템 초기화/정리
```

#### 2.1.2 서버 패턴 (`src/events/CliServer.hh/cc`)
```cpp
// 참조 가능한 TCP 서버 구현
- 별도 스레드에서 accept() 루프 실행
- Poller를 사용한 비동기 중단
- 클라이언트 연결 시 별도 핸들러 생성
```

#### 2.1.3 폴링 (`src/utils/Poller.hh`)
```cpp
// 스레드 안전한 비동기 폴링/중단
Poller::poll(fd), Poller::abort(), Poller::aborted()
```

#### 2.1.4 설정 시스템 (`src/settings/`)
```cpp
BooleanSetting - 활성화 플래그용
IntegerSetting - 포트 번호 설정용
```

### 2.2 정보 접근 경로

#### 2.2.1 머신 정보
```cpp
// MSXMotherBoard
- getMachineName(), getMachineType()
- getSlotManager() → CartridgeSlotManager
  - getNumberOfSlots(), getPsSs(), getConfigForSlot()
- getMediaProviders() → 드라이브/카트리지 미디어 정보
- getExtensions() → 확장 카트리지 목록
- getCassettePort() → 카세트 상태

// Reactor
- getDiskManipulator() → 디스크 정보
```

#### 2.2.2 IO 정보
```cpp
// MSXCPUInterface
- IO_In[256], IO_Out[256] → I/O 장치 배열
- readIO(), writeIO() → I/O 접근 (감시용)
- IODebug (SimpleDebuggable) → 256 바이트 I/O 공간
```

#### 2.2.3 CPU 정보
```cpp
// MSXCPU
- getRegisters() → CPURegs
- isR800Active()

// CPURegs
- getAF(), getBC(), getDE(), getHL(), getPC(), getSP()
- getIX(), getIY(), getAF2(), getBC2(), getDE2(), getHL2()
- getI(), getR(), getIM(), getIFF1(), getIFF2(), getHALT()
```

#### 2.2.4 메모리 정보
```cpp
// MSXCPUInterface
- peekMem(), peekSlottedMem() → 메모리 읽기 (부작용 없음)
- MemoryDebug → 64KB 메모리 공간 (현재 슬롯)
- SlottedMemoryDebug → 모든 슬롯 메모리

// Debugger
- getDebuggables() → 등록된 모든 Debuggable 객체
```

---

## 3. 아키텍처 설계

### 3.1 전체 구조

```
┌─────────────────────────────────────────────────────────────┐
│                      Reactor                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │               DebugHttpServer                         │  │
│  │  - 4개 포트 관리                                       │  │
│  │  - 설정 감시                                           │  │
│  │  ┌─────────┐┌─────────┐┌─────────┐┌─────────┐         │  │
│  │  │ Machine ││   IO    ││   CPU   ││ Memory  │         │  │
│  │  │ Server  ││ Server  ││ Server  ││ Server  │         │  │
│  │  │ :65501  ││ :65502  ││ :65503  ││ :65504  │         │  │
│  │  └────┬────┘└────┬────┘└────┬────┘└────┬────┘         │  │
│  └───────┼──────────┼──────────┼──────────┼──────────────┘  │
│          │          │          │          │                  │
│  ┌───────▼──────────▼──────────▼──────────▼──────────────┐  │
│  │                 DebugInfoProvider                      │  │
│  │  - 정보 수집 및 포맷팅                                  │  │
│  │  - JSON/Text 출력 형식                                  │  │
│  └───────────────────────────────────────────────────────┘  │
│          │                                                   │
│  ┌───────▼───────────────────────────────────────────────┐  │
│  │              MSXMotherBoard (Active)                   │  │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐      │  │
│  │  │   MSXCPU    │ │MSXCPUInterface│ │ Debugger   │      │  │
│  │  └─────────────┘ └─────────────┘ └─────────────┘      │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 클래스 다이어그램

```
┌──────────────────────────────────────────────────────────────┐
│ DebugHttpServer                                               │
├──────────────────────────────────────────────────────────────┤
│ - reactor: Reactor&                                          │
│ - enableSetting: BooleanSetting                              │
│ - machinePortSetting: IntegerSetting                         │
│ - ioPortSetting: IntegerSetting                              │
│ - cpuPortSetting: IntegerSetting                             │
│ - memoryPortSetting: IntegerSetting                          │
│ - servers: array<unique_ptr<DebugHttpServerPort>, 4>         │
├──────────────────────────────────────────────────────────────┤
│ + DebugHttpServer(Reactor&)                                  │
│ + ~DebugHttpServer()                                         │
│ - updateServers()                                            │
│ - update(const Setting&) override                            │
└──────────────────────────────────────────────────────────────┘
                               │
                               │ 1:4
                               ▼
┌──────────────────────────────────────────────────────────────┐
│ DebugHttpServerPort                                           │
├──────────────────────────────────────────────────────────────┤
│ - port: int                                                  │
│ - type: InfoType                                             │
│ - listenSocket: SOCKET                                       │
│ - thread: std::thread                                        │
│ - poller: Poller                                             │
│ - connections: vector<unique_ptr<DebugHttpConnection>>       │
│ - infoProvider: DebugInfoProvider&                           │
├──────────────────────────────────────────────────────────────┤
│ + start()                                                    │
│ + stop()                                                     │
│ - mainLoop()                                                 │
│ - handleConnection(SOCKET)                                   │
└──────────────────────────────────────────────────────────────┘
                               │
                               │ 1:*
                               ▼
┌──────────────────────────────────────────────────────────────┐
│ DebugHttpConnection                                           │
├──────────────────────────────────────────────────────────────┤
│ - socket: SOCKET                                             │
│ - type: InfoType                                             │
│ - infoProvider: DebugInfoProvider&                           │
│ - thread: std::thread                                        │
│ - poller: Poller                                             │
│ - updateInterval: int (ms)                                   │
├──────────────────────────────────────────────────────────────┤
│ + run()                                                      │
│ + close()                                                    │
│ - parseHttpRequest(string)                                   │
│ - sendHttpResponse(string)                                   │
│ - sendInfo()                                                 │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│ DebugInfoProvider                                             │
├──────────────────────────────────────────────────────────────┤
│ - reactor: Reactor&                                          │
├──────────────────────────────────────────────────────────────┤
│ + getMachineInfo(): string                                   │
│ + getIOInfo(): string                                        │
│ + getCPUInfo(): string                                       │
│ + getMemoryInfo(start, size): string                         │
│ + formatAsJson/Text(InfoType, data): string                  │
├──────────────────────────────────────────────────────────────┤
│ - getActiveMotherBoard(): MSXMotherBoard*                    │
└──────────────────────────────────────────────────────────────┘

enum class InfoType { MACHINE, IO, CPU, MEMORY }
```

---

## 4. 파일 구조

### 4.1 새로 생성할 파일

```
src/debugger/
├── DebugHttpServer.hh          # 메인 서버 클래스
├── DebugHttpServer.cc
├── DebugHttpServerPort.hh      # 개별 포트 서버
├── DebugHttpServerPort.cc
├── DebugHttpConnection.hh      # 클라이언트 연결 처리
├── DebugHttpConnection.cc
├── DebugInfoProvider.hh        # 정보 수집 및 포맷팅
├── DebugInfoProvider.cc
└── DebugInfoFormat.hh          # 출력 형식 정의
```

### 4.2 수정할 파일

```
src/Reactor.hh                  # DebugHttpServer 멤버 추가
src/Reactor.cc                  # DebugHttpServer 초기화
src/GlobalSettings.hh           # (선택) 전역 디버그 서버 설정
build/main.mk                   # 새 파일 빌드 등록
```

---

## 5. 출력 형식 정의

### 5.1 공통 HTTP 응답 헤더

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Access-Control-Allow-Origin: *
Connection: keep-alive
Cache-Control: no-cache

{data}
```

### 5.2 머신 정보 형식 (포트 65501)

**파일**: `src/debugger/formats/machine_info.json`

```json
{
  "timestamp": 1234567890,
  "machine": {
    "id": "machine1",
    "name": "Panasonic FS-A1GT",
    "type": "turboR",
    "powered": true,
    "active": true
  },
  "slots": [
    {
      "slot": 0,
      "ps": 0,
      "ss": -1,
      "type": "internal",
      "content": "Main ROM"
    },
    {
      "slot": 1,
      "ps": 1,
      "ss": 0,
      "type": "cartridge",
      "content": "Aleste 2",
      "file": "/path/to/aleste2.rom"
    }
  ],
  "drives": {
    "diska": {
      "inserted": true,
      "file": "/path/to/disk.dsk",
      "readonly": false
    },
    "diskb": {
      "inserted": false
    }
  },
  "cassette": {
    "inserted": false
  },
  "extensions": [
    {
      "name": "fmpac",
      "slot": 2
    }
  ]
}
```

### 5.3 IO 정보 형식 (포트 65502)

**파일**: `src/debugger/formats/io_info.json`

```json
{
  "timestamp": 1234567890,
  "ports": {
    "input": [
      {"port": "0x98", "value": "0x00", "device": "VDP data"},
      {"port": "0x99", "value": "0x1F", "device": "VDP status"},
      {"port": "0xA8", "value": "0x00", "device": "PPI slot select"}
    ],
    "output": [
      {"port": "0x98", "value": "0x00", "device": "VDP data"},
      {"port": "0x99", "value": "0x00", "device": "VDP address"},
      {"port": "0xA8", "value": "0x00", "device": "PPI slot select"}
    ]
  },
  "recent_activity": [
    {"time": "00:01:23.456", "type": "OUT", "port": "0x98", "value": "0x3F"},
    {"time": "00:01:23.457", "type": "IN",  "port": "0x99", "value": "0x1F"}
  ]
}
```

### 5.4 CPU 정보 형식 (포트 65503)

**파일**: `src/debugger/formats/cpu_info.json`

```json
{
  "timestamp": 1234567890,
  "cpu_type": "Z80",
  "frequency_hz": 3579545,
  "registers": {
    "main": {
      "AF": "0x0044", "A": "0x00", "F": "0x44",
      "BC": "0x1234", "B": "0x12", "C": "0x34",
      "DE": "0x5678", "D": "0x56", "E": "0x78",
      "HL": "0x9ABC", "H": "0x9A", "L": "0xBC"
    },
    "alt": {
      "AF'": "0x0000", "BC'": "0x0000",
      "DE'": "0x0000", "HL'": "0x0000"
    },
    "index": {
      "IX": "0x0000",
      "IY": "0x0000"
    },
    "special": {
      "PC": "0x4000",
      "SP": "0xF380",
      "I": "0x00",
      "R": "0x00",
      "IM": 1
    }
  },
  "flags": {
    "S": false, "Z": true, "H": false,
    "PV": false, "N": true, "C": false,
    "IFF1": true, "IFF2": true,
    "HALT": false
  },
  "current_instruction": {
    "address": "0x4000",
    "bytes": "21 00 C0",
    "mnemonic": "LD HL,0xC000"
  },
  "stack": [
    {"address": "0xF380", "value": "0x4010"},
    {"address": "0xF382", "value": "0x0038"}
  ]
}
```

### 5.5 메모리 정보 형식 (포트 65504)

**파일**: `src/debugger/formats/memory_info.json`

```json
{
  "timestamp": 1234567890,
  "request": {
    "start": "0x0000",
    "size": 256,
    "slot": "current"
  },
  "slot_config": {
    "page0": {"ps": 0, "ss": 0, "segment": null},
    "page1": {"ps": 3, "ss": 2, "segment": 0},
    "page2": {"ps": 3, "ss": 2, "segment": 1},
    "page3": {"ps": 3, "ss": 0, "segment": null}
  },
  "memory": {
    "hex": "F3 ED 56 ... (256 bytes)",
    "formatted": [
      {"address": "0x0000", "hex": "F3 ED 56 C3 4E 00 C3 51", "ascii": "..V.N..Q"},
      {"address": "0x0008", "hex": "00 C3 5D 00 C3 4C 01 C3", "ascii": "..]..L.."}
    ]
  },
  "symbols": [
    {"address": "0x0000", "name": "CHKRAM"},
    {"address": "0x0038", "name": "RST38"}
  ]
}
```

---

## 6. 구현 상세

### 6.1 DebugHttpServer 클래스

```cpp
// src/debugger/DebugHttpServer.hh

#ifndef DEBUG_HTTP_SERVER_HH
#define DEBUG_HTTP_SERVER_HH

#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "Observer.hh"

#include <array>
#include <memory>

namespace openmsx {

class Reactor;
class DebugHttpServerPort;
class DebugInfoProvider;

class DebugHttpServer final : private Observer<Setting>
{
public:
    explicit DebugHttpServer(Reactor& reactor);
    ~DebugHttpServer();

    DebugHttpServer(const DebugHttpServer&) = delete;
    DebugHttpServer& operator=(const DebugHttpServer&) = delete;

private:
    void updateServers();
    void update(const Setting& setting) noexcept override;

private:
    Reactor& reactor;
    std::unique_ptr<DebugInfoProvider> infoProvider;

    // 설정
    BooleanSetting enableSetting;
    IntegerSetting machinePortSetting;
    IntegerSetting ioPortSetting;
    IntegerSetting cpuPortSetting;
    IntegerSetting memoryPortSetting;

    // 서버 인스턴스
    std::array<std::unique_ptr<DebugHttpServerPort>, 4> servers;
};

} // namespace openmsx

#endif
```

### 6.2 DebugHttpServerPort 클래스

```cpp
// src/debugger/DebugHttpServerPort.hh

#ifndef DEBUG_HTTP_SERVER_PORT_HH
#define DEBUG_HTTP_SERVER_PORT_HH

#include "Poller.hh"
#include "Socket.hh"

#include <mutex>
#include <thread>
#include <vector>
#include <memory>

namespace openmsx {

class DebugInfoProvider;
class DebugHttpConnection;

enum class InfoType { MACHINE, IO, CPU, MEMORY };

class DebugHttpServerPort final
{
public:
    DebugHttpServerPort(int port, InfoType type,
                        DebugInfoProvider& infoProvider);
    ~DebugHttpServerPort();

    DebugHttpServerPort(const DebugHttpServerPort&) = delete;
    DebugHttpServerPort& operator=(const DebugHttpServerPort&) = delete;

    void start();
    void stop();

private:
    void mainLoop();
    SOCKET createListenSocket();

private:
    int port;
    InfoType type;
    DebugInfoProvider& infoProvider;

    SOCKET listenSocket = OPENMSX_INVALID_SOCKET;
    std::thread thread;
    Poller poller;
    [[no_unique_address]] SocketActivator socketActivator;

    std::mutex connectionsMutex;
    std::vector<std::unique_ptr<DebugHttpConnection>> connections;
};

} // namespace openmsx

#endif
```

### 6.3 DebugHttpConnection 클래스

```cpp
// src/debugger/DebugHttpConnection.hh

#ifndef DEBUG_HTTP_CONNECTION_HH
#define DEBUG_HTTP_CONNECTION_HH

#include "DebugHttpServerPort.hh"
#include "Poller.hh"
#include "Socket.hh"

#include <atomic>
#include <string>
#include <thread>

namespace openmsx {

class DebugInfoProvider;

class DebugHttpConnection final
{
public:
    DebugHttpConnection(SOCKET socket, InfoType type,
                        DebugInfoProvider& infoProvider);
    ~DebugHttpConnection();

    void start();
    void stop();
    [[nodiscard]] bool isClosed() const;

private:
    void run();
    bool parseHttpRequest(const std::string& request);
    void sendHttpResponse(const std::string& body);
    void sendInfo();
    std::string generateInfo();

private:
    SOCKET socket;
    InfoType type;
    DebugInfoProvider& infoProvider;

    std::thread thread;
    Poller poller;
    std::atomic<bool> closed{false};

    // HTTP 요청 파라미터
    int refreshInterval = 100;  // ms (SSE 모드용)
    bool sseMode = false;       // Server-Sent Events 모드
    unsigned memoryStart = 0;
    unsigned memorySize = 256;
};

} // namespace openmsx

#endif
```

### 6.4 DebugInfoProvider 클래스

```cpp
// src/debugger/DebugInfoProvider.hh

#ifndef DEBUG_INFO_PROVIDER_HH
#define DEBUG_INFO_PROVIDER_HH

#include <cstdint>
#include <string>
#include <mutex>

namespace openmsx {

class Reactor;
class MSXMotherBoard;

class DebugInfoProvider final
{
public:
    explicit DebugInfoProvider(Reactor& reactor);

    // 정보 수집 (스레드 안전)
    [[nodiscard]] std::string getMachineInfo();
    [[nodiscard]] std::string getIOInfo();
    [[nodiscard]] std::string getCPUInfo();
    [[nodiscard]] std::string getMemoryInfo(unsigned start, unsigned size);

private:
    [[nodiscard]] MSXMotherBoard* getActiveMotherBoard();

    // JSON 헬퍼
    static std::string jsonEscape(const std::string& s);
    static std::string hexString(uint8_t value);
    static std::string hexString(uint16_t value);

private:
    Reactor& reactor;
    std::mutex accessMutex;  // MotherBoard 접근 동기화
};

} // namespace openmsx

#endif
```

---

## 7. HTTP 프로토콜 구현

### 7.1 지원 엔드포인트

| 엔드포인트 | 메서드 | 설명 |
|------------|--------|------|
| `/` | GET | 정보 JSON 반환 (기본) |
| `/stream` | GET | SSE (Server-Sent Events) 스트림 |
| `/raw` | GET | 단순 텍스트 형식 |

### 7.2 쿼리 파라미터

| 파라미터 | 적용 포트 | 설명 |
|----------|-----------|------|
| `interval=<ms>` | 모든 포트 | SSE 갱신 주기 (기본: 100ms) |
| `start=<addr>` | 65504 | 메모리 시작 주소 (기본: 0x0000) |
| `size=<bytes>` | 65504 | 메모리 크기 (기본: 256) |
| `slot=<ps>-<ss>` | 65504 | 특정 슬롯 지정 |

### 7.3 간단한 HTTP 파서 구현

```cpp
// 내부 구현 - 외부 라이브러리 없음
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> queryParams;
    std::map<std::string, std::string> headers;
};

HttpRequest parseHttpRequest(const std::string& raw) {
    HttpRequest req;
    // 첫 줄 파싱: GET /path?query HTTP/1.1
    // 헤더 파싱
    // 쿼리 스트링 파싱
    return req;
}
```

### 7.4 SSE (Server-Sent Events) 구현

```cpp
// SSE 응답 헤더
void sendSSEHeader() {
    std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    sock_send(socket, header.data(), header.size());
}

// SSE 이벤트 전송
void sendSSEEvent(const std::string& data) {
    std::string event = "data: " + data + "\n\n";
    sock_send(socket, event.data(), event.size());
}
```

---

## 8. 스레드 안전성

### 8.1 동기화 지점

```cpp
// 1. MotherBoard 접근
std::mutex DebugInfoProvider::accessMutex;

// 2. 연결 목록 관리
std::mutex DebugHttpServerPort::connectionsMutex;

// 3. Reactor의 기존 뮤텍스 활용
Reactor::mbMutex  // activeBoard 접근 시
```

### 8.2 스레드 모델

```
Main Thread          Server Thread (x4)      Connection Thread (x N)
     │                     │                        │
     │  ◄──설정 변경──     │                        │
     │                     │                        │
     │                accept()                      │
     │                     │──────────────────────► │
     │                     │                   처리 시작
     │                     │                        │
     │  ◄─────────────── 정보 요청 ───────────────  │
     │                     │                        │
     │  ──────────────── 정보 반환 ───────────────► │
     │                     │                        │
```

---

## 9. 설정 파일

### 9.1 설정 키

```tcl
# settings.xml 또는 Tcl 콘솔에서 설정

set debug_http_enable true
set debug_http_machine_port 65501
set debug_http_io_port 65502
set debug_http_cpu_port 65503
set debug_http_memory_port 65504
```

### 9.2 기본값

| 설정 | 기본값 | 범위 |
|------|--------|------|
| `debug_http_enable` | false | true/false |
| `debug_http_machine_port` | 65501 | 1024-65535 |
| `debug_http_io_port` | 65502 | 1024-65535 |
| `debug_http_cpu_port` | 65503 | 1024-65535 |
| `debug_http_memory_port` | 65504 | 1024-65535 |

---

## 10. 구현 단계

### Phase 1: 기반 구조 (1단계)
1. `DebugHttpServer` 클래스 생성 및 설정 시스템 연동
2. `DebugHttpServerPort` 클래스 - TCP 리스닝 구현
3. `DebugHttpConnection` 클래스 - 기본 HTTP 요청/응답
4. 빌드 시스템 수정

### Phase 2: 정보 수집 (2단계)
1. `DebugInfoProvider` 클래스 구현
2. 머신 정보 수집 구현
3. CPU 정보 수집 구현
4. 메모리 정보 수집 구현
5. IO 정보 수집 구현

### Phase 3: JSON 포맷팅 (3단계)
1. JSON 출력 형식 구현
2. 각 정보 유형별 포맷터 구현
3. 텍스트 출력 형식 구현

### Phase 4: 고급 기능 (4단계)
1. SSE 스트리밍 구현
2. 쿼리 파라미터 처리
3. 에러 처리 및 예외 안전성
4. 성능 최적화

### Phase 5: 테스트 및 문서화 (5단계)
1. 단위 테스트 작성
2. 통합 테스트
3. 사용자 문서 작성
4. API 문서 작성

---

## 11. 빌드 시스템 수정

### 11.1 meson.build 수정

```meson
# src/debugger/meson.build에 추가

debugger_sources = files(
    # 기존 파일들...
    'DebugHttpServer.cc',
    'DebugHttpServerPort.cc',
    'DebugHttpConnection.cc',
    'DebugInfoProvider.cc',
)
```

### 11.2 main.mk 수정 (대안)

```makefile
# build/main.mk에 추가

SOURCES += \
    src/debugger/DebugHttpServer.cc \
    src/debugger/DebugHttpServerPort.cc \
    src/debugger/DebugHttpConnection.cc \
    src/debugger/DebugInfoProvider.cc
```

---

## 12. 사용 예시

### 12.1 curl을 이용한 접근

```bash
# 머신 정보 조회
curl http://localhost:65501/

# CPU 정보 조회
curl http://localhost:65503/

# 메모리 덤프 (0x4000부터 512바이트)
curl "http://localhost:65504/?start=0x4000&size=512"

# SSE 스트림 구독
curl -N http://localhost:65503/stream
```

### 12.2 웹 브라우저 JavaScript

```javascript
// SSE 스트림 수신
const eventSource = new EventSource('http://localhost:65503/stream');

eventSource.onmessage = (event) => {
    const cpuInfo = JSON.parse(event.data);
    console.log('PC:', cpuInfo.registers.special.PC);
    updateDisplay(cpuInfo);
};

// 일반 폴링
async function fetchMachineInfo() {
    const response = await fetch('http://localhost:65501/');
    const data = await response.json();
    return data;
}
```

---

## 13. 보안 고려사항

### 13.1 보안 조치

1. **로컬호스트 바인딩**: 기본적으로 127.0.0.1에만 바인딩
2. **포트 범위 제한**: 1024-65535 범위만 허용
3. **읽기 전용**: 현재 구현은 읽기 전용 (쓰기 API 없음)
4. **리소스 제한**: 최대 연결 수 제한

### 13.2 향후 고려사항

- 인증 메커니즘 (선택적)
- HTTPS 지원 (선택적)
- 원격 접근 허용 설정

---

## 14. 라이선스

### 14.1 사용 코드 라이선스

- 모든 코드는 openMSX 프로젝트의 GPL-2.0 라이선스를 따름
- 외부 라이브러리 사용 없음
- 기존 openMSX 소켓 코드 재사용 (Socket.hh/cc)

### 14.2 참고 자료

- RFC 7230-7235: HTTP/1.1 사양
- RFC 9110: HTTP Semantics
- W3C: Server-Sent Events

---

## 15. 예상 코드 규모

| 파일 | 예상 라인 수 |
|------|-------------|
| DebugHttpServer.hh/cc | ~200 |
| DebugHttpServerPort.hh/cc | ~250 |
| DebugHttpConnection.hh/cc | ~400 |
| DebugInfoProvider.hh/cc | ~600 |
| DebugInfoFormat.hh | ~100 |
| **총계** | **~1,550** |

---

## 16. 테스트 계획

### 16.1 단위 테스트

- HTTP 파싱 테스트
- JSON 생성 테스트
- 정보 수집 테스트

### 16.2 통합 테스트

- 서버 시작/종료
- 다중 클라이언트 연결
- SSE 스트리밍
- 설정 변경 시 동작

### 16.3 부하 테스트

- 다중 연결 시 성능
- 메모리 누수 확인
- CPU 사용률 확인
