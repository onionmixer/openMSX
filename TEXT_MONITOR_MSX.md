# MSX Screen Mode 별 Text Screen 모니터링 가능 여부

| Screen Mode       | Display Mode | 텍스트 추출 가능 여부 | 이유                                              |
|-------------------|--------------|-----------------------|----------------------------------------------------------------|
| Screen 0 (40열)   | TEXT1        | ✅ 가능               | nameTable에 문자 코드(0-255) 직접 저장                         |
| Screen 0 (80열)   | TEXT2        | ✅ 가능               | nameTable에 문자 코드 직접 저장 (MSX2+)                        |
| Screen 1          | GRAPHIC1     | ⚠️ 부분적             | 패턴 번호가 ASCII에 매핑될 수 있으나 보장 안됨                 |
| Screen 2          | GRAPHIC2     | ❌ 불가능             | 패턴 테이블이 완전 프로그래머블 (고정 문자셋 없음)             |
| Screen 3          | MULTICOLOR   | ❌ 불가능             | 색상 블록 모드, 문자 데이터 없음                               |
| Screen 4          | GRAPHIC3     | ❌ 불가능             | Screen 2와 동일                                                |
| Screen 5-8, 10-12 | GRAPHIC4-7   | ❌ 불가능             | 비트맵 모드 - 순수 픽셀 데이터, 문자 코드 자체가 존재하지 않음 |
