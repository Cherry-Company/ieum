<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.32

- 이번 릴리스에 배분된 GitHub Sponsors 후원금 / Sponsor funds allocated to this release: **USD 0**
- 후원금으로 완료한 작업 / Work claimed as sponsor-funded: **없음 / None**
- 이번 릴리스에 배정된 후원금 없이 독립적으로 개발했습니다. / Developed independently without
  sponsor funds allocated to this release. Advertised prices are not counted as received revenue.
- 다음 우선순위 / Next priorities: 남은 네이티브 기능 구현, Paddle 운영 검수, Windows 정식 코드 서명,
  Apple Developer ID 서명·공증과 실기기 회귀 검사. / Remaining native features, Paddle production
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

## 아직 포함되지 않은 기능 / Features not yet complete

오디오 캡처·재생, 원격 화면 전송, 양쪽 기기의 물리 키보드·마우스 제어, 실제 잠금·깨우기,
마우스 동시 입력과 주 컴퓨터 입력 주입은 아직 완성되지 않았습니다. 플랜 화면의 추가 기능은
출시 예정 구성입니다. 이번 버전을 여섯 기능 전체가 완성된 상용 구독판으로 안내하지 않습니다.

Audio capture/playback, remote screen transport, input from both computers' physical devices,
coordinated lock/wake, mouse broadcast and primary-host input injection remain incomplete.
The additional plan features describe intended packaging, not completed capabilities.

## 빌드 및 검증 범위 / Build and validation scope

- 로컬 Linux Qt GUI/core 빌드와 네이티브 테스트 131개가 통과했습니다. Windows 관련 진단 검사
  1개는 Linux에서 건너뛰었습니다. / Local Linux GUI/core builds and 131 native test cases passed;
  one Windows-specific diagnostic test was skipped on Linux.
- 별도 비공개 구독 서비스의 모의 데이터 테스트 39개, Chromium 결제/복원 흐름, 세 플랜의
  서명·만료 C++ 검증 6개가 통과했습니다. 실제 결제 검증은 아닙니다. / The separate service passed
  39 fixture tests, a Chromium checkout/recovery flow and six C++ signature/expiry checks;
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
