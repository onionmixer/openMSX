# openMSX HTTP 디버거 서버 개발 계획

**상태: 구현 완료**
**마지막 업데이트: 2025-12-27**
**Fork: https://github.com/onionmixer/openMSX**

---

## 1. 개요

### 1.1 목표
HTTP 클라이언트가 접속하여 openMSX 에뮬레이터의 디버깅 정보를 실시간으로 확인할 수 있는 기능을 추가합니다.

### 1.2 요구사항
- [x] 설정 파일의 flag에 따라 활성화/비활성화
- [x] 4개의 별도 포트로 소켓을 열고 각 유형별 정보 제공
- [x] 외부 라이브러리 없이 내부 구현
- [x] 라이선스 문제 없는 코드 사용
- [x] HTML 대시보드 지원 (Catppuccin Mocha 테마)
- [x] JSON API 지원
- [x] SSE (Server-Sent Events) 스트리밍 지원

### 1.3 포트 정의
| 포트 | 정보 유형 | 설명 | 상태 |
|------|-----------|------|------|
| 65501 | 머신 정보 | 드라이브, 슬롯, 카트리지 등 | 구현 완료 |
| 65502 | IO 정보 | I/O 포트 입출력 데이터 | 구현 완료 |
| 65503 | CPU 정보 | 레지스터, PC, 플래그 등 | 구현 완료 |
| 65504 | 메모리 정보 | 메모리 덤프, 슬롯 메모리 | 구현 완료 |

---

## 2. 구현 완료 내역

### 2.1 생성된 파일

| 파일 | 크기 | 설명 |
|------|------|------|
| `src/debugger/DebugHttpServer.hh` | 1.6KB | 메인 서버 클래스 헤더 |
| `src/debugger/DebugHttpServer.cc` | 3.0KB | 메인 서버 구현 |
| `src/debugger/DebugHttpServerPort.hh` | 2.2KB | 포트별 리스너 헤더 |
| `src/debugger/DebugHttpServerPort.cc` | 3.6KB | 포트별 리스너 구현 |
| `src/debugger/DebugHttpConnection.hh` | 2.4KB | 클라이언트 연결 헤더 |
| `src/debugger/DebugHttpConnection.cc` | 9.9KB | HTTP 요청/응답 처리 |
| `src/debugger/DebugInfoProvider.hh` | 1.7KB | 정보 수집 헤더 |
| `src/debugger/DebugInfoProvider.cc` | 11.5KB | JSON 데이터 생성 |
| `src/debugger/HtmlGenerator.hh` | 1.9KB | HTML 생성 헤더 |
| `src/debugger/HtmlGenerator.cc` | 26.5KB | HTML 대시보드 생성 |

**총 추가 코드:** ~64KB (10개 파일)

### 2.2 수정된 파일

| 파일 | 변경 내용 |
|------|-----------|
| `src/Reactor.hh` | DebugHttpServer 멤버 추가 |
| `src/Reactor.cc` | DebugHttpServer 초기화 |
| `share/settings.xml` | 기본 설정 추가 |

### 2.3 아키텍처

```
┌─────────────────────────────────────────────────────────────────┐
│                         Reactor                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                   DebugHttpServer                         │  │
│  │  - 4개 포트 관리 (65501-65504)                            │  │
│  │  - TCL 설정 연동 (debug_http_enable)                      │  │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐         │  │
│  │  │ Machine ││   IO    ││   CPU   ││ Memory  │         │  │
│  │  │ :65501  ││ :65502  ││ :65503  ││ :65504  │         │  │
│  │  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘         │  │
│  └───────┼──────────┼──────────┼──────────┼──────────────┘  │
│          │          │          │          │                  │
│  ┌───────▼──────────▼──────────▼──────────▼──────────────┐  │
│  │               DebugHttpServerPort (×4)                 │  │
│  │  - 소켓 리스닝                                          │  │
│  │  - 클라이언트 연결 수락                                  │  │
│  │  - 연결별 스레드 생성                                    │  │
│  └───────────────────────────────────────────────────────┘  │
│          │                                                   │
│  ┌───────▼───────────────────────────────────────────────┐  │
│  │                  DebugHttpConnection                   │  │
│  │  - HTTP 요청 파싱                                       │  │
│  │  - 응답 생성 (HTML/JSON/SSE)                           │  │
│  └───────────────────────────────────────────────────────┘  │
│          │                                                   │
│  ┌───────▼──────────┐  ┌─────────────────────────────────┐  │
│  │  HtmlGenerator   │  │       DebugInfoProvider          │  │
│  │  - HTML 대시보드 │  │  - JSON 데이터 생성              │  │
│  │  - CSS 스타일링  │  │  - 에뮬레이터 상태 수집          │  │
│  │  - 네비게이션    │  │  - 스레드 안전 (mutex)           │  │
│  └──────────────────┘  └─────────────────────────────────┘  │
│          │                          │                        │
│  ┌───────▼──────────────────────────▼────────────────────┐  │
│  │                MSXMotherBoard (Active)                 │  │
│  │  ┌─────────────┐ ┌───────────────┐ ┌─────────────┐    │  │
│  │  │   MSXCPU    │ │MSXCPUInterface│ │  Debugger   │    │  │
│  │  └─────────────┘ └───────────────┘ └─────────────┘    │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. HTTP 엔드포인트

### 3.1 구현된 엔드포인트

| 경로 | 메서드 | 응답 형식 | 설명 |
|------|--------|-----------|------|
| `/` | GET | HTML | 시각적 대시보드 (브라우저용) |
| `/info` | GET | HTML/JSON | Accept 헤더에 따라 결정 |
| `/api` | GET | JSON | API 엔드포인트 (항상 JSON) |
| `/stream` | GET | SSE | Server-Sent Events 스트리밍 |

### 3.2 쿼리 파라미터

| 파라미터 | 적용 포트 | 설명 |
|----------|-----------|------|
| `start` | 65504 | 메모리 시작 주소 (예: `0x0000`, `0xC000`) |
| `size` | 65504 | 읽을 바이트 수 (기본: 256, 최대: 65536) |
| `interval` | 모든 포트 | 스트리밍 간격 ms (기본: 100, 범위: 10-10000) |

---

## 4. HTML 대시보드

### 4.1 디자인 특징

- **테마:** Catppuccin Mocha 다크 테마
- **자동 새로고침:** 1초 간격 (meta refresh)
- **반응형:** CSS Grid 기반 레이아웃
- **네비게이션:** 포트 간 이동 링크

### 4.2 색상 팔레트

```css
:root {
    --bg-base: #1e1e2e;      /* 배경 */
    --bg-surface: #313244;   /* 카드 배경 */
    --bg-overlay: #45475a;   /* 오버레이 */
    --text-main: #cdd6f4;    /* 메인 텍스트 */
    --text-sub: #a6adc8;     /* 서브 텍스트 */
    --accent-blue: #89b4fa;  /* 강조 (파랑) */
    --accent-green: #a6e3a1; /* 강조 (초록) */
    --accent-yellow: #f9e2af;/* 강조 (노랑) */
    --accent-red: #f38ba8;   /* 강조 (빨강) */
    --accent-mauve: #cba6f7; /* 강조 (보라) */
}
```

### 4.3 페이지별 기능

| 포트 | 페이지 | 표시 정보 |
|------|--------|-----------|
| 65501 | Machine | 머신 이름, 타입, 슬롯 구성, 드라이브 상태 |
| 65502 | I/O | I/O 포트 상태, 최근 I/O 활동 |
| 65503 | CPU | 레지스터, 플래그, PC, 현재 명령어 |
| 65504 | Memory | 메모리 덤프 (16진수 + ASCII) |

---

## 5. JSON API 형식

### 5.1 머신 정보 (65501/api)

```json
{
  "machine": {
    "id": "machine1",
    "name": "Panasonic FS-A1GT",
    "type": "turboR",
    "powered": true
  },
  "slots": [...],
  "drives": {...},
  "extensions": [...]
}
```

### 5.2 CPU 정보 (65503/api)

```json
{
  "cpu_type": "Z80",
  "registers": {
    "AF": "0x0044", "BC": "0x1234",
    "DE": "0x5678", "HL": "0x9ABC",
    "IX": "0x0000", "IY": "0x0000",
    "PC": "0x4000", "SP": "0xF380"
  },
  "flags": {
    "S": false, "Z": true, "H": false,
    "PV": false, "N": true, "C": false,
    "IFF1": true, "IFF2": true
  }
}
```

### 5.3 메모리 정보 (65504/api)

```json
{
  "start": "0x0000",
  "size": 256,
  "data": "F3ED56C34E00...",
  "formatted": [
    {"address": "0x0000", "hex": "F3 ED 56 C3...", "ascii": "..V."}
  ]
}
```

---

## 6. 설정

### 6.1 TCL 명령어

```tcl
# 활성화
set debug_http_enable on

# 비활성화
set debug_http_enable off

# 포트 변경 (재시작 필요)
set debug_http_machine_port 65501
set debug_http_io_port 65502
set debug_http_cpu_port 65503
set debug_http_memory_port 65504
```

### 6.2 기본값

| 설정 | 기본값 | 범위 |
|------|--------|------|
| `debug_http_enable` | off | on/off |
| `debug_http_machine_port` | 65501 | 1024-65535 |
| `debug_http_io_port` | 65502 | 1024-65535 |
| `debug_http_cpu_port` | 65503 | 1024-65535 |
| `debug_http_memory_port` | 65504 | 1024-65535 |

---

## 7. 사용 방법

### 7.1 브라우저 접속

1. openMSX 실행
2. TCL 콘솔 열기 (F10)
3. 명령어 입력: `set debug_http_enable on`
4. 브라우저에서 접속:
   - http://127.0.0.1:65501/ (Machine)
   - http://127.0.0.1:65502/ (I/O)
   - http://127.0.0.1:65503/ (CPU)
   - http://127.0.0.1:65504/ (Memory)

### 7.2 curl 사용

```bash
# JSON API로 CPU 레지스터 조회
curl http://127.0.0.1:65503/api

# 메모리 덤프 (0x4000부터 512바이트)
curl "http://127.0.0.1:65504/api?start=0x4000&size=512"

# 실시간 스트리밍 (500ms 간격)
curl "http://127.0.0.1:65503/stream?interval=500"
```

### 7.3 JavaScript (SSE)

```javascript
const eventSource = new EventSource('http://127.0.0.1:65503/stream');

eventSource.onmessage = (event) => {
    const cpuInfo = JSON.parse(event.data);
    console.log('PC:', cpuInfo.registers.PC);
};
```

---

## 8. 구현 단계 (완료)

### Phase 1: 기반 구조 [완료]
- [x] `DebugHttpServer` 클래스 생성 및 설정 시스템 연동
- [x] `DebugHttpServerPort` 클래스 - TCP 리스닝 구현
- [x] `DebugHttpConnection` 클래스 - 기본 HTTP 요청/응답
- [x] 빌드 시스템 수정 (자동 감지)

### Phase 2: 정보 수집 [완료]
- [x] `DebugInfoProvider` 클래스 구현
- [x] 머신 정보 수집 구현
- [x] CPU 정보 수집 구현
- [x] 메모리 정보 수집 구현
- [x] IO 정보 수집 구현

### Phase 3: JSON 포맷팅 [완료]
- [x] JSON 출력 형식 구현
- [x] 각 정보 유형별 포맷터 구현

### Phase 4: HTML 대시보드 [완료]
- [x] `HtmlGenerator` 클래스 구현
- [x] Catppuccin Mocha 테마 적용
- [x] 네비게이션 구현
- [x] 자동 새로고침 구현

### Phase 5: 고급 기능 [완료]
- [x] SSE 스트리밍 구현
- [x] 쿼리 파라미터 처리
- [x] Accept 헤더 기반 콘텐츠 협상

---

## 9. 보안

### 9.1 구현된 보안 조치

- **로컬호스트 전용:** 127.0.0.1에만 바인딩
- **읽기 전용:** 쓰기 API 없음
- **포트 범위 제한:** 1024-65535

### 9.2 주의사항

- 인증 없이 에뮬레이터 상태 조회 가능
- 로컬 환경에서만 사용 권장

---

## 10. 향후 개선 가능 사항

### 10.1 기능 개선

- [ ] 디스어셈블리 뷰 추가
- [ ] 브레이크포인트 관리 UI
- [ ] 메모리 검색 기능
- [ ] 스프라이트/타일 뷰어
- [ ] VDP 레지스터 뷰

### 10.2 성능 개선

- [ ] WebSocket 지원 (SSE 대체)
- [ ] 압축 지원 (gzip)
- [ ] 연결 풀링

### 10.3 보안 개선

- [ ] 기본 인증 (선택적)
- [ ] HTTPS 지원 (선택적)
- [ ] 원격 접근 설정

---

## 11. 참고 자료

### 11.1 관련 프로젝트

- AppleWin HTTP Debug Server (참고 구현)
- openMSX CliServer (기존 소켓 서버)

### 11.2 사양

- RFC 7230-7235: HTTP/1.1
- W3C: Server-Sent Events
- Catppuccin Color Palette

---

## 12. 라이선스

모든 코드는 openMSX 프로젝트의 GPL-2.0 라이선스를 따릅니다.
외부 라이브러리 사용 없이 구현되었습니다.
