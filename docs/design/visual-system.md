<!-- SPDX-FileCopyrightText: (C) 2026 Ieum Developers -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# 이음 인터페이스 원칙

## 제품 표기

| UI 언어 | 표시 이름 | 소개 문구 |
| --- | --- | --- |
| 한국어 | 이음 (Ieum) | 한글과 CJK 입력을 안정적으로 잇는 소프트웨어 KVM |
| 그 외 | Ieum | Software KVM across Windows, macOS, and Linux |

실행 파일, 설정 디렉터리, 번들 내부 이름과 프로토콜 식별자는 호환성을 위해 `Ieum`을 유지합니다.
사용자에게 보이는 창 제목, 제품 헤더, 정보 화면과 운영체제 메타데이터만 현재 UI 언어에 맞춰
표시합니다.

Windows 릴리스는 글로벌 MSI와 `ko-KR` MSI를 별도로 생성합니다. macOS는 한국어
`InfoPlist.strings`를 번들에 포함하고, Linux는 데스크톱 항목과 AppStream 메타데이터를 현지화합니다.

## WWDC26 기반 원칙

이음은 Apple UI를 픽셀 단위로 복제하지 않습니다. Qt가 Windows, macOS, Linux에서 제공하는
표준 컨트롤을 유지하면서 다음 공식 원칙을 제품 구조에 적용합니다.

- 컨트롤과 내비게이션을 콘텐츠 위의 명확한 기능 레이어로 분리합니다.
- Liquid Glass 효과는 핵심 기능 레이어에만 제한하고 중첩하거나 장식으로 남용하지 않습니다.
- 시스템 색상, 밝은/어두운 모드, 대비 설정과 투명도 감소 환경에서도 읽을 수 있어야 합니다.
- 창 크기 변화에 대응하고, 텍스트나 동적 상태가 버튼과 레이아웃 크기를 흔들지 않게 합니다.
- 표준 버튼, 입력 필드, 메뉴, 도구 아이콘을 우선하고 커스텀 배경은 필요한 표면에만 사용합니다.

근거 자료:

- [Apple: Adopting Liquid Glass](https://developer.apple.com/documentation/technologyoverviews/adopting-liquid-glass)
- [Apple: Liquid Glass](https://developer.apple.com/documentation/TechnologyOverviews/liquid-glass)
- [Apple: What's new in SwiftUI, WWDC26](https://developer.apple.com/videos/play/wwdc2026/269/)
- [Apple: WWDC26 Design guide](https://developer.apple.com/wwdc26/guides/design/)

## Qt 구현 규칙

- `QPalette`의 창, 기본 표면, 텍스트, 강조, 자리표시자 색을 의미 기반 토큰으로 사용합니다.
- 반투명 표면은 시스템 팔레트 위에 얹고, 실제 배경 블러를 지원하지 않는 플랫폼에서 가짜 블러나
  그라데이션을 만들지 않습니다.
- 메인 창은 제품 헤더, 기기 식별 영역, 서버/클라이언트 세그먼트, 모드별 작업 영역, 상태 표시줄의
  순서로 구성합니다.
- 모서리 반경은 최대 8px, 아이콘 버튼은 34~36px, 핵심 실행 버튼은 최소 112px 너비를 유지합니다.
- 서버/클라이언트 선택은 배타적 `QRadioButton`을 세그먼트처럼 표시해 키보드 탐색과 접근성 의미를
  보존합니다.
- 운영체제 테마가 바뀌면 시스템 팔레트에서 색을 다시 계산하며, 제품 아이콘도 현재 아이콘 테마에서
  다시 읽습니다.

## 검증 체크리스트

- 한국어 UI에서 제품명과 소개 문구가 한국어 정책과 일치하는가
- 영어 및 다른 언어에서 글로벌 제품명과 소개 문구가 표시되는가
- 700px 너비와 긴 번역 문자열에서 컨트롤이 겹치거나 잘리지 않는가
- 밝은 모드, 어두운 모드, 고대비 모드에서 텍스트와 선택 상태를 구분할 수 있는가
- 서버/클라이언트 전환, 시작/중지, 로그 확장, 설정과 정보 창 동작이 기존과 동일한가
- Windows x64/ARM64, macOS Intel/Apple Silicon, Linux 패키지에서 리소스와 번역이 포함되는가
