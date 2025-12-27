# openMSX 빌드 가이드

openMSX를 소스로부터 컴파일하는 방법을 설명합니다.

## 목차

1. [빌드 요구사항](#빌드-요구사항)
2. [소스 코드 받기](#소스-코드-받기)
3. [로컬 시스템용 빌드](#로컬-시스템용-빌드)
4. [독립 실행형 바이너리 빌드](#독립-실행형-바이너리-빌드)
5. [빌드 옵션](#빌드-옵션)
6. [플랫폼별 참고사항](#플랫폼별-참고사항)
7. [문제 해결](#문제-해결)
8. [Debug HTTP Server](#debug-http-server)

---

## 빌드 요구사항

### 필수 빌드 도구

| 도구 | 최소 버전 | 설명 |
|------|-----------|------|
| Python | 3.x | 빌드 스크립트 실행 |
| GNU Make | 3.80+ | 빌드 시스템 |
| GCC (g++) | 12+ | C++23 지원 필요 |
| clang++ | 17+ | GCC 대안 (macOS 기본) |

### 필수 라이브러리

| 라이브러리 | 설명 |
|------------|------|
| SDL2 | 크로스플랫폼 미디어 레이어 |
| SDL2_ttf | TrueType 폰트 지원 (FreeType 포함) |
| libpng | PNG 이미지 처리 |
| zlib | 압축 라이브러리 |
| Tcl | 8.6+ 스크립팅 언어 |
| OpenGL | 그래픽 가속 |
| GLEW | OpenGL 확장 |

### 선택 라이브러리

| 라이브러리 | 용도 |
|------------|------|
| libogg | Laserdisc 에뮬레이션 |
| libvorbis | Laserdisc 오디오 |
| libtheora | Laserdisc 비디오 |
| ALSA | Linux MIDI 지원 |

---

## 소스 코드 받기

### Git Clone (권장)

```bash
git clone https://github.com/openMSX/openMSX.git openMSX
cd openMSX
```

### 릴리스 버전

[GitHub Releases](https://github.com/openMSX/openMSX/releases/)에서 다운로드:

```bash
tar xzvf openmsx-VERSION.tar.gz
cd openmsx-VERSION
```

---

## 로컬 시스템용 빌드

시스템에 설치된 라이브러리를 사용하여 빌드합니다.

### 1. 의존성 설치

#### Debian/Ubuntu

```bash
# 모든 빌드 의존성 설치 (권장)
sudo apt-get build-dep openmsx

# 또는 개별 패키지 설치
sudo apt-get install \
    build-essential \
    python3 \
    libsdl2-dev \
    libsdl2-ttf-dev \
    libpng-dev \
    zlib1g-dev \
    tcl-dev \
    libglew-dev \
    libogg-dev \
    libvorbis-dev \
    libtheora-dev \
    libasound2-dev
```

#### Fedora/RHEL

```bash
sudo dnf install \
    gcc-c++ \
    make \
    python3 \
    SDL2-devel \
    SDL2_ttf-devel \
    libpng-devel \
    zlib-devel \
    tcl-devel \
    glew-devel \
    libogg-devel \
    libvorbis-devel \
    libtheora-devel \
    alsa-lib-devel
```

#### Arch Linux

```bash
sudo pacman -S \
    base-devel \
    python \
    sdl2 \
    sdl2_ttf \
    libpng \
    zlib \
    tcl \
    glew \
    libogg \
    libvorbis \
    libtheora \
    alsa-lib
```

### 2. 환경 검사

```bash
./configure
```

이 스크립트는 필요한 라이브러리와 헤더가 설치되어 있는지 확인합니다.
문제가 있으면 `derived/<cpu>-<os>-<flavour>/config/probe.log`를 확인하세요.

### 3. 컴파일

```bash
make
```

빌드 진행 상황을 상세히 보려면:

```bash
make V=1
```

### 4. 설치

```bash
sudo make install
```

기본 설치 경로: `/opt/openMSX`

설치 경로 변경은 `build/custom.mk`에서 `INSTALL_BASE` 수정:

```makefile
INSTALL_BASE:=/usr/local/openMSX
```

### 5. 실행

```bash
openmsx
```

---

## 독립 실행형 바이너리 빌드

외부 라이브러리에 의존하지 않는 독립 실행형 바이너리를 만듭니다.
Windows와 macOS에서 권장됩니다.

### 빌드 명령

```bash
make staticbindist
```

이 명령은:
1. 필요한 3rd party 라이브러리 소스를 자동으로 다운로드
2. 해당 라이브러리들을 정적으로 빌드
3. openMSX와 정적 링크하여 독립 실행형 바이너리 생성

### 출력 위치

```
derived/<cpu>-<os>-<flavour>-3rd/bindist/
```

### 3rd Party 라이브러리만 빌드

```bash
make 3rdparty
```

---

## 빌드 옵션

### 빌드 플레이버 (OPENMSX_FLAVOUR)

| 플레이버 | 설명 |
|----------|------|
| `opt` | 일반 최적화 (기본값) |
| `devel` | 개발용 (assert 활성화, 디버그 심볼 포함) |
| `debug` | 디버그용 (최적화 없음) |
| `lto` | 링크 타임 최적화 |
| `i686` | i686 CPU 최적화 (x86) |

플레이버 설정:

```bash
export OPENMSX_FLAVOUR=devel
make
```

### 컴파일러 선택 (CXX)

```bash
# GCC 사용
export CXX=g++

# 특정 버전 GCC
export CXX=g++-13

# Clang 사용
export CXX=clang++
```

### 크로스 컴파일

타겟 CPU와 OS 지정:

```bash
export OPENMSX_TARGET_CPU=x86_64
export OPENMSX_TARGET_OS=linux
make
```

지원 OS:
- `linux`, `darwin`, `freebsd`, `netbsd`, `openbsd`
- `mingw-w64`, `mingw32` (Windows)
- `android`

### 바이너리 스트립

```bash
export OPENMSX_STRIP=true
make
```

### 상세 빌드 출력 (Verbosity)

```bash
make V=0  # 요약만 (기본)
make V=1  # 명령어만
make V=2  # 요약 + 명령어
```

---

## 플랫폼별 참고사항

### Linux

```bash
# 전체 빌드 과정
./configure
make
sudo make install
```

### macOS

Xcode 16.4 이상 필요.

```bash
# 독립 실행형 앱 번들 생성 (권장)
make staticbindist

# 결과: derived/<cpu>-darwin-<flavour>-3rd/bindist/openMSX.app
```

앱 실행:

```bash
open derived/<cpu>-darwin-<flavour>-3rd/bindist/openMSX.app
```

### Windows (Visual C++)

Visual Studio 2022 필요.

```powershell
# 1. 3rd party 라이브러리 다운로드
python build\thirdparty_download.py windows

# 2. 3rd party 라이브러리 빌드
msbuild -p:Configuration=Release;Platform=x64 build\3rdparty\3rdparty.sln

# 3. openMSX 빌드
msbuild -p:Configuration=Release;Platform=x64 build\msvc\openmsx.sln
```

결과 위치: `derived\x64-VC-Release\install`

### Windows (MinGW)

```bash
make staticbindist
```

---

## 문제 해결

### configure 실패

`probe.log` 파일 확인:

```bash
cat derived/<cpu>-<os>-<flavour>/config/probe.log
```

### 컴파일 오류

1. 컴파일러 버전 확인 (GCC 12+, clang 17+ 필요):

```bash
g++ --version
clang++ --version
```

2. 상세 빌드 출력으로 오류 확인:

```bash
make V=1
```

### 라이브러리를 찾을 수 없음

개발 패키지 설치 확인 (예: `libsdl2-dev`, `libpng-dev` 등).

### 빌드 정리

```bash
# 현재 플레이버의 빌드 결과물 삭제
make clean

# 전체 derived 디렉토리 삭제
rm -rf derived/
```

### 설정 확인

```bash
make config
```

출력 예시:
```
Build configuration:
  Platform: x86_64-linux
  Flavour:  opt
  Compiler: g++
  Subset:   full build
```

---

## 빌드 시스템 구조

```
openMSX/
├── build/
│   ├── main.mk              # 메인 빌드 스크립트
│   ├── custom.mk             # 사용자 설정
│   ├── 3rdparty.mk           # 3rd party 라이브러리 빌드
│   ├── flavour-*.mk          # 빌드 플레이버 설정
│   ├── platform-*.mk         # 플랫폼별 설정
│   ├── probe.py              # 환경 검사 스크립트
│   └── components.py         # 컴포넌트 의존성 정의
├── configure                 # 환경 검사 래퍼 스크립트
├── src/                      # 소스 코드
└── derived/                  # 빌드 결과물 (자동 생성)
```

---

## Debug HTTP Server

openMSX에는 실시간 디버깅 정보를 HTTP로 제공하는 Debug HTTP Server가 포함되어 있습니다.
브라우저에서 시각적인 대시보드를 보거나, API를 통해 JSON 데이터를 받을 수 있습니다.

### 활성화

openMSX 실행 후 TCL 콘솔에서:

```tcl
set debug_http_enable on
```

### 브라우저 접속 (HTML 대시보드)

서버가 활성화되면 웹 브라우저에서 다음 주소로 접속할 수 있습니다:

| 포트 | URL | 설명 |
|------|-----|------|
| 65501 | http://127.0.0.1:65501/ | 머신 정보 대시보드 |
| 65502 | http://127.0.0.1:65502/ | I/O 상태 대시보드 |
| 65503 | http://127.0.0.1:65503/ | CPU 레지스터 대시보드 |
| 65504 | http://127.0.0.1:65504/ | 메모리 뷰어 대시보드 |

**대시보드 특징:**
- Catppuccin Mocha 다크 테마
- 1초마다 자동 새로고침
- 색상 코딩된 레지스터/플래그 표시
- 포트 간 네비게이션 링크

### HTTP 엔드포인트

| 경로 | 응답 형식 | 설명 |
|------|-----------|------|
| `/` | HTML | 시각적 대시보드 (브라우저용) |
| `/info` | HTML/JSON | Accept 헤더에 따라 결정 |
| `/api` | JSON | API 엔드포인트 (항상 JSON) |
| `/stream` | SSE | Server-Sent Events 스트리밍 |

### API 사용 예시

```bash
# JSON API로 CPU 레지스터 조회
curl http://127.0.0.1:65503/api

# 메모리 덤프 (0x0000부터 256바이트)
curl "http://127.0.0.1:65504/api?start=0&size=256"

# 실시간 스트리밍 (500ms 간격)
curl "http://127.0.0.1:65503/stream?interval=500"

# Accept 헤더로 JSON 명시적 요청
curl -H "Accept: application/json" http://127.0.0.1:65503/info
```

### 쿼리 파라미터

| 파라미터 | 적용 포트 | 설명 |
|----------|-----------|------|
| `start` | 65504 | 메모리 시작 주소 (예: `0x0000`, `0`, `0xC000`) |
| `size` | 65504 | 읽을 바이트 수 (기본: 256, 최대: 65536) |
| `interval` | 모든 포트 | 스트리밍 간격 ms (기본: 100, 범위: 10-10000) |

### 설정 옵션

| 설정 | 기본값 | 설명 |
|------|--------|------|
| `debug_http_enable` | off | HTTP 서버 활성화 |
| `debug_http_machine_port` | 65501 | 머신 정보 포트 |
| `debug_http_io_port` | 65502 | I/O 정보 포트 |
| `debug_http_cpu_port` | 65503 | CPU 정보 포트 |
| `debug_http_memory_port` | 65504 | 메모리 정보 포트 |

### 보안 참고사항

- 서버는 `127.0.0.1` (localhost)에만 바인딩됩니다
- 외부 네트워크에서는 접근할 수 없습니다
- 인증 없이 에뮬레이터 상태를 조회할 수 있으므로 로컬 환경에서만 사용하세요

---

## 추가 리소스

- [openMSX 공식 사이트](https://openmsx.org/)
- [GitHub 저장소](https://github.com/openMSX/openMSX)
- [공식 컴파일 가이드](doc/manual/compile.html) (HTML)
- [IRC 채널: #openMSX on libera.chat](ircs://irc.libera.chat:6697/#openMSX)
