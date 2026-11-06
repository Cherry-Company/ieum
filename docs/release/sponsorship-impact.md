<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.32

- 이번 릴리스에 배분된 GitHub Sponsors 후원금 / Sponsor funds allocated to this release: **USD 0**
- 후원금으로 완료한 작업 / Work claimed as sponsor-funded: **없음 / None**
- 이번 릴리스에 배정된 후원금 없이 독립적으로 개발했습니다. / Developed independently without
  sponsor funds allocated to this release. Advertised prices are not counted as received revenue.
- 다음 우선순위 / Next priorities: 네이티브 기능 실기기 검수, Paddle 운영 검수, Windows 정식 코드 서명,
  Apple Developer ID 서명·공증과 실기기 회귀 검사. / Native hardware acceptance, Paddle production
  acceptance, Windows signing, Apple Developer ID/notarization and physical regression testing.

## 변경 내용 / Changes

- Free / Professional Local / Professional / Professional Max 플랜과 가격 미리보기를 추가했습니다.
  일반 구매 경로는 Paddle 구독으로 준비하며, 실제 결제 서비스가 검증된 빌드에서만 구매 버튼이 열립니다.
  현재 구독은 판매하지 않습니다. / Adds four-tier plan and price previews. Paddle checkout is enabled
  only in a build connected to a verified service. Subscriptions are not on sale yet.
- 기존 Local 라이선스의 유효 기간과 영구 파일 전송 권한을 보존합니다. 이벤트·소유자 라이선스와
  구독 권한은 별도로 저장하며 구독 종료가 기존 유효 키를 지우지 않습니다. / Preserves valid legacy
  Local and event licenses independently of subscription expiry.
- 서명된 구독 권한을 매시간 갱신하고 만료·취소·통신 장애를 구분합니다. 구독 전용 검증 키와
  이벤트 키의 권한을 분리했습니다. 서비스 장애 시 기존 서명 만료까지 최대 7일 유지합니다.
  / Adds hourly signed entitlement refresh with separate subscription and event authorities.
  Existing signed access survives a service outage until its expiry, at most seven days.
- 결제 페이지에서 받은 복원 코드 또는 이음 활성화 링크로 구독을 복원하도록 준비했습니다.
  링크는 선택한 플랜과 마스킹된 코드를 채우며 사용자가 복원을 선택해 완료합니다. Paddle 영수증은
  결제 관리용이고 복원 코드는 이음 결제 페이지에서 저장합니다. / Adds recovery-code and activation-link
  handling. Receipt links manage billing; the Ieum checkout page provides the recovery code.
- **권한 정책 변경:** 기존 이미지 클립보드와 키보드 동시 전송은 이번 빌드부터 Max 권한을 확인합니다.
  무료 클립보드는 일반 텍스트를 유지하고 이미지를 포함할 수 있는 HTML은 전송하지 않습니다.
  / **Entitlement policy change:** inherited image clipboard and keyboard broadcast now require Max.
  Free clipboard sharing retains plain text and excludes HTML that can embed images.
- 방송 중 키를 누른 대상에 맞춰 반복/해제를 처리하고 모드 변경·권한 만료·연결 해제 시 눌린 키 처리를
  보완했습니다. 잘못된 클립보드 헤더의 읽기 경계 오류도 수정했습니다. / Tracks broadcast key recipients
  through mode changes, entitlement expiry and disconnects, and fixes malformed clipboard header reads.

## 추가 공유 기능 미리보기 / Native sharing preview

시스템 오디오 캡처·재생, 주 화면 전송, 양쪽 물리 키보드·마우스 제어, 잠금·깨우기,
마우스를 포함한 동시 입력을 구현했습니다. 공유 패널에서 상대의 기능별 허용을 받고,
저장된 허용 취소·트레이 중지·Ctrl+Alt+Esc·연결 종료 시 입력 해제를 지원합니다.
기존 TLS 연결의 프로토콜 1.15로 전달하며 별도 수신 포트를 열지 않습니다.

Implements system audio, primary-screen viewing, bidirectional physical input,
lock/wake and simultaneous keyboard/mouse input. The sharing panel adds receiver
consent, saved-permission revocation, tray stop and emergency release. Sessions
use protocol 1.15 on the existing authenticated TLS connection.

화면은 최대 720p·5fps, macOS 시스템 오디오는 13 이상입니다. Linux의 새 화면·입력 기능은
X11을 사용하며, 새 기능의 Wayland·Flatpak 지원은 포함하지 않습니다. 깨우기는 로그인을
해제하지 않으며 절전 해제는 하드웨어와 LAN 조건이 필요합니다. Windows↔macOS 실기기
검증 전인 미리보기이며 상용 구독 판매 완료 상태가 아닙니다.

Screen viewing is capped at 720p/5fps. macOS audio needs 13+. New Linux screen/input
features require X11; new Wayland/Flatpak support is not included. Wake never
unlocks a login and sleep wake depends on the hardware/LAN. Windows↔macOS hardware
acceptance is pending; this is a preview, with subscription sales disabled.

## 빌드 및 검증 범위 / Build and validation scope

- Linux GUI/core 빌드, CTest 80개 대상과 레거시 검사 27개가 통과했습니다. 가상 X11/PulseAudio에서
  네이티브 입력·키 해제·루프 차단·시스템 소리 캡처·재생도 확인했습니다. Windows 소스는 교차 문법
  검사를 통과했고 macOS 빌드와 실기기 검수는 남았습니다. / Linux GUI/core builds, 80 CTest targets
  and 27 legacy cases passed, along with isolated native X11/PulseAudio checks. Windows syntax was
  cross-checked; macOS compilation and physical acceptance remain outstanding.
- 별도 비공개 구독 서비스의 모의 데이터 테스트 43개와 로컬 Workers/D1 검사 6개, Chromium 결제/복원 흐름, 세 플랜의
  서명·만료 C++ 검증 6개가 통과했습니다. 실제 결제 검증은 아닙니다. / The separate service passed
  43 fixture tests and six local Workers/D1 tests, a Chromium checkout/recovery flow and six C++ signature/expiry checks;
  no real payment has been verified.
- 새 소스의 Windows x64/ARM64, Apple Silicon/Intel, Linux 패키지는 해당 태그 CI와 패키지 검사가
  통과한 파일만 게시합니다. 소유자 마스터키젠과 개인 서명 키는 공개 패키지에 넣지 않습니다.
  / Only assets passing this tag's cross-platform build and package gates are published.
  Owner key-generation tools and private signing keys are excluded from public assets.
- Windows 실행 파일 버전은 `0.1.132.0`입니다. / Windows executable version: `0.1.132.0`.
- Windows 정식 코드 서명 및 Apple Developer ID/공증은 아직 구성되지 않았습니다. Mac 패키지는
  ad-hoc 서명 프리릴리스이며 수동 실행 승인이 필요할 수 있습니다. / Windows production signing
  and Apple Developer ID/notarization are not configured; macOS uses ad-hoc signing.
- 설치·재부팅·절전 복귀·실제 네트워크와 장시간 입력 검사는 Windows/macOS 실기기에서 확인해야 합니다.
  Paddle 계정·정산·도메인·운영 서버와 실결제·갱신·해지·환불 검수도 남아 있습니다. / Physical installation,
  reboot/sleep, network and long-running input acceptance remain, as do Paddle onboarding, hosting
  and live purchase/renewal/cancellation/refund validation.

[플랜·가격 근거와 출시 조건 / Plan evidence and release conditions](https://github.com/Cherry-Company/ieum/blob/v0.1.0-alpha.32/docs/product/professional-plans.md)
