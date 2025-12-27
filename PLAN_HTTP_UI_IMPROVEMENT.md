# openMSX Debug HTTP Server UI 개선 계획

## 현재 상태 분석

### 현재 구현
- **응답 형식**: JSON만 지원
- **엔드포인트**: `/info`, `/stream`
- **스타일링**: 없음 (raw JSON)
- **실시간 업데이트**: SSE (Server-Sent Events)

### 문제점
1. 브라우저에서 JSON만 보임 - 시각적으로 불편
2. HTML 대시보드 없음
3. 색상 코딩된 상태 표시 없음
4. 자동 새로고침 HTML 페이지 없음

---

## 개선 목표

AppleWin의 디자인 패턴을 참고하여 openMSX에 맞는 HTML 대시보드 추가:

1. **듀얼 모드 응답**: JSON API + HTML 대시보드
2. **다크 테마 UI**: Catppuccin Mocha 스타일 적용
3. **실시간 업데이트**: Meta refresh 또는 JavaScript
4. **시맨틱 색상 코딩**: 상태별 색상 구분

---

## 색상 팔레트 (Catppuccin Mocha 기반)

```css
--bg-base:      #1e1e2e;   /* 배경 */
--bg-surface:   #313244;   /* 카드/박스 배경 */
--bg-overlay:   #45475a;   /* 호버/강조 배경 */
--text-primary: #cdd6f4;   /* 주요 텍스트 */
--text-muted:   #6c7086;   /* 보조 텍스트 */
--accent-blue:  #89b4fa;   /* 주소/링크 */
--accent-green: #a6e3a1;   /* 활성/ON */
--accent-yellow:#f9e2af;   /* 값/하이라이트 */
--accent-red:   #f38ba8;   /* 오류/경고 */
--accent-mauve: #cba6f7;   /* MSX 브랜드 색상 */
```

---

## 엔드포인트 구조 개선

### 포트 65501 (Machine Info)
| 경로 | 응답 | 설명 |
|------|------|------|
| `/` | HTML | 머신 정보 대시보드 |
| `/info` | JSON | 머신 정보 API |
| `/api/machine` | JSON | 머신 정보 API (별칭) |
| `/stream` | SSE | 실시간 스트리밍 |

### 포트 65502 (I/O Info)
| 경로 | 응답 | 설명 |
|------|------|------|
| `/` | HTML | I/O 상태 대시보드 |
| `/info` | JSON | I/O 정보 API |
| `/api/io` | JSON | I/O 정보 API (별칭) |
| `/stream` | SSE | 실시간 스트리밍 |

### 포트 65503 (CPU Info)
| 경로 | 응답 | 설명 |
|------|------|------|
| `/` | HTML | CPU 레지스터 대시보드 |
| `/info` | JSON | CPU 정보 API |
| `/api/registers` | JSON | 레지스터 API |
| `/api/flags` | JSON | 플래그 API |
| `/stream` | SSE | 실시간 스트리밍 |

### 포트 65504 (Memory Info)
| 경로 | 응답 | 설명 |
|------|------|------|
| `/` | HTML | 메모리 뷰어 대시보드 |
| `/info` | JSON | 메모리 덤프 API |
| `/api/memory` | JSON | 메모리 API (별칭) |
| `/stream` | SSE | 실시간 스트리밍 |

---

## HTML 템플릿 설계

### 공통 레이아웃 구조

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="1">
    <title>openMSX Debug - {PAGE_TITLE}</title>
    <style>{EMBEDDED_CSS}</style>
</head>
<body>
    <header>
        <h1>openMSX Debug Server</h1>
        <nav>
            <a href="http://127.0.0.1:65501/">Machine</a>
            <a href="http://127.0.0.1:65502/">I/O</a>
            <a href="http://127.0.0.1:65503/">CPU</a>
            <a href="http://127.0.0.1:65504/">Memory</a>
        </nav>
    </header>
    <main>
        {CONTENT}
    </main>
    <footer>
        <span class="timestamp">Updated: {TIMESTAMP}</span>
        <span class="status">{STATUS}</span>
    </footer>
</body>
</html>
```

---

## 페이지별 디자인

### 1. Machine Info (65501)

```
+--------------------------------------------------+
| openMSX Debug Server     [Machine][I/O][CPU][Mem]|
+--------------------------------------------------+
|                                                  |
|  +-- System Info ------+  +-- CPU Type --------+ |
|  | Machine: MSX2+      |  |       [Z80]        | |
|  | Name: C-BIOS_MSX2+  |  |                    | |
|  | Status: ● Running   |  +--------------------+ |
|  +---------------------+                         |
|                                                  |
|  +-- Memory Slots ------------------------------------+
|  | Page | Address | Primary | Secondary | Device     |
|  |------|---------|---------|-----------|------------|
|  |  0   |  0000h  |    0    |    -1     | C-BIOS ROM |
|  |  1   |  4000h  |    0    |    -1     | C-BIOS ROM |
|  |  2   |  8000h  |    3    |     2     | Main RAM   |
|  |  3   |  C000h  |    3    |     2     | Main RAM   |
|  +----------------------------------------------------+
|                                                  |
|  +-- Extensions ---------+                       |
|  | (none)                |                       |
|  +-----------------------+                       |
+--------------------------------------------------+
```

### 2. I/O Info (65502)

```
+--------------------------------------------------+
| openMSX Debug Server     [Machine][I/O][CPU][Mem]|
+--------------------------------------------------+
|                                                  |
|  +-- Slot Selection (Port A8h) ------------------+
|  |                                               |
|  |  Page 0: [0]    Page 1: [0]                   |
|  |  Page 2: [3]    Page 3: [3]                   |
|  |                                               |
|  +-----------------------------------------------+
|                                                  |
|  +-- Secondary Slots ----------------------------+
|  |                                               |
|  |  Slot 0: Not Expanded                         |
|  |  Slot 1: Not Expanded                         |
|  |  Slot 2: Not Expanded                         |
|  |  Slot 3: Expanded   SS: [2][2][2][2]          |
|  |                                               |
|  +-----------------------------------------------+
|                                                  |
+--------------------------------------------------+
```

### 3. CPU Info (65503)

```
+--------------------------------------------------+
| openMSX Debug Server     [Machine][I/O][CPU][Mem]|
+--------------------------------------------------+
|                                                  |
|  +-- Registers (16-bit) -------------------------+
|  |   AF: FF6A     BC: 0004     DE: FBEC         |
|  |   HL: 27FB     IX: FBE1     IY: 0000         |
|  |   SP: F2E8     PC: 1A44                       |
|  +-----------------------------------------------+
|                                                  |
|  +-- Alternate Registers ------------------------+
|  |   AF': FF44    BC': 0000    DE': FFFF        |
|  |   HL': 26D2                                   |
|  +-----------------------------------------------+
|                                                  |
|  +-- Flags --------------------------------------+
|  |  [S]  [Z]  [ ]  [H]  [ ]  [P]  [N]  [C]       |
|  |   0    1    1    0    1    0    1    0        |
|  +-----------------------------------------------+
|                                                  |
|  +-- Interrupts ---------------------------------+
|  |  IFF1: ● ON    IFF2: ● ON    IM: 0           |
|  |  I: 00    R: 57    HALT: ○ No                |
|  +-----------------------------------------------+
|                                                  |
|  +-- CPU Type -----------------------------------+
|  |        [ Z80 ]  /  [ R800 ]                   |
|  +-----------------------------------------------+
+--------------------------------------------------+
```

### 4. Memory Info (65504)

```
+--------------------------------------------------+
| openMSX Debug Server     [Machine][I/O][CPU][Mem]|
+--------------------------------------------------+
|                                                  |
|  Address: [0000] Size: [256] [Refresh]           |
|                                                  |
|  +-- Memory Dump --------------------------------+
|  | Addr   00 01 02 03 04 05 06 07  ASCII        |
|  |--------|-------------------------|------------|
|  | 0000:  F3 C3 12 0D BF 1B 98 98  ó........    |
|  | 0008:  C3 92 11 00 C3 D2 23 00  Ã...Ã.#.     |
|  | 0010:  C3 A4 11 00 C3 13 24 00  Ã¤..Ã.$..    |
|  | ...                                           |
|  +-----------------------------------------------+
|                                                  |
|  +-- Slot Info ----------------------------------+
|  | Page 0 (0000-3FFF): Slot 0                   |
|  +-----------------------------------------------+
+--------------------------------------------------+
```

---

## 구현 계획

### Phase 1: HTML 생성기 추가

**파일**: `src/debugger/HtmlGenerator.hh`, `HtmlGenerator.cc`

```cpp
class HtmlGenerator {
public:
    // 공통 페이지 래퍼
    static std::string wrapPage(const std::string& title,
                                 const std::string& content,
                                 DebugInfoType activeTab);

    // 각 페이지 생성
    static std::string generateMachinePage(const MachineInfo& info);
    static std::string generateIOPage(const IOInfo& info);
    static std::string generateCPUPage(const CPUInfo& info);
    static std::string generateMemoryPage(const MemoryInfo& info);

private:
    static std::string getCSS();
    static std::string getNavigation(DebugInfoType activeTab);
    static std::string escapeHtml(const std::string& s);
};
```

### Phase 2: DebugHttpConnection 수정

**파일**: `src/debugger/DebugHttpConnection.cc`

1. Accept 헤더 파싱 추가
2. HTML 응답 경로 추가
3. Content-Type에 따른 분기 처리

```cpp
void DebugHttpConnection::handleInfoRequest(const HttpRequest& request) {
    // Accept 헤더 확인
    auto acceptIt = request.headers.find("accept");
    bool wantsHtml = (acceptIt != request.headers.end() &&
                      acceptIt->second.find("text/html") != std::string::npos);

    if (wantsHtml || request.path == "/") {
        // HTML 대시보드 반환
        std::string html = generateHtmlPage();
        sendHttpResponse(200, "text/html; charset=utf-8", html);
    } else {
        // JSON API 반환
        std::string json = generateInfo();
        sendHttpResponse(200, "application/json", json);
    }
}
```

### Phase 3: CSS 스타일 임베딩

**내장 CSS** (약 3KB):

```cpp
static const char* EMBEDDED_CSS = R"CSS(
:root {
    --bg-base: #1e1e2e;
    --bg-surface: #313244;
    --bg-overlay: #45475a;
    --text-primary: #cdd6f4;
    --text-muted: #6c7086;
    --accent-blue: #89b4fa;
    --accent-green: #a6e3a1;
    --accent-yellow: #f9e2af;
    --accent-red: #f38ba8;
    --accent-mauve: #cba6f7;
}

* { margin: 0; padding: 0; box-sizing: border-box; }

body {
    font-family: 'Courier New', monospace;
    background: var(--bg-base);
    color: var(--text-primary);
    line-height: 1.6;
}

/* ... 전체 스타일 ... */
)CSS";
```

---

## 구현 우선순위

| 순서 | 작업 | 복잡도 | 설명 |
|------|------|--------|------|
| 1 | HtmlGenerator 클래스 생성 | 중 | CSS 및 HTML 템플릿 |
| 2 | CPU 페이지 구현 | 중 | 레지스터, 플래그 표시 |
| 3 | Machine 페이지 구현 | 저 | 슬롯, 확장 정보 |
| 4 | I/O 페이지 구현 | 저 | 슬롯 선택 레지스터 |
| 5 | Memory 페이지 구현 | 고 | 헥스 덤프 포맷팅 |
| 6 | 네비게이션 통합 | 저 | 포트 간 링크 |
| 7 | 자동 새로고침 | 저 | meta refresh |

---

## 파일 변경 목록

### 새 파일
- `src/debugger/HtmlGenerator.hh`
- `src/debugger/HtmlGenerator.cc`

### 수정 파일
- `src/debugger/DebugHttpConnection.hh` - HTML 생성 메서드 추가
- `src/debugger/DebugHttpConnection.cc` - HTML 응답 처리 추가
- `src/debugger/DebugInfoProvider.hh` - 구조체 정의 추가 (optional)
- `src/debugger/node.mk` - 새 파일 빌드 등록

---

## 예상 결과

### Before (현재)
```json
{"timestamp":1234567890,"status":"running","registers":{"af":"FF6A",...}}
```

### After (개선 후)
브라우저에서 시각적인 대시보드가 표시됨:
- 다크 테마 UI
- 색상 코딩된 레지스터 값
- ON/OFF 상태 표시 (플래그, 인터럽트)
- 1초마다 자동 새로고침
- 포트 간 네비게이션 링크

---

## 테스트 계획

1. **단위 테스트**: HTML 이스케이프, CSS 생성
2. **통합 테스트**: 각 포트별 HTML 응답 확인
3. **브라우저 테스트**: Chrome, Firefox에서 렌더링 확인
4. **자동 새로고침 테스트**: 1초 간격 업데이트 확인
5. **JSON API 호환성**: 기존 JSON 응답 유지 확인

---

## 향후 확장 가능성

1. **JavaScript 기반 실시간 업데이트**: SSE 활용
2. **메모리 편집 기능**: POST 요청 처리
3. **디스어셈블리 뷰**: PC 주변 명령어 표시
4. **브레이크포인트 관리**: 설정/해제 UI
5. **VDP 레지스터 표시**: 비디오 상태 모니터링
