<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.28

- 이번 릴리스에 배분된 GitHub Sponsors 후원금 / Sponsor funds allocated to this release: **USD 0**
- 후원금으로 완료한 작업 / Work claimed as sponsor-funded: **없음 / None**
- 이유 / Reason: 이번 릴리스에 배정된 후원금 없이 독립적으로 개발했습니다. / This release was
  developed independently without sponsor funds allocated to it.
- Early Access의 USD 30 가격은 실제 결제를 확인하기 전까지 이 금액에 포함하지 않습니다. / The USD 30
  Early Access offer is not counted here until a payment is actually received and verified.
- 다음 우선순위 / Next funding priorities: Windows 정식 코드 서명, Apple Developer ID 서명·공증,
  Windows ARM64·Apple Silicon 실기 회귀 테스트. / Production Windows signing, Apple Developer ID
  signing and notarization, and physical Windows ARM64 and Apple Silicon regression testing.

## 빌드 및 실기 검증 범위 / Build and physical validation scope

- 공개 저장소의 Apple Silicon 패키지는 임시 GitHub-hosted `macos-15-arm64` 환경에서 네이티브로
  빌드하고 Intel DMG는 `macos-15-intel`에서 빌드합니다. 실기 수용 테스트는 릴리스 패키징 경로와
  분리합니다. / Apple Silicon packages for this public repository are built natively on the ephemeral
  GitHub-hosted `macos-15-arm64` environment, while Intel DMGs use `macos-15-intel`. Hardware acceptance
  testing remains separate from the release-packaging lane.
- 릴리스 CI는 ARM64 컴파일과 CTest, DMG 생성·마운트, `codesign --verify --deep --strict`, ARM64 Mach-O
  확인과 아티팩트 업로드를 모두 통과한 경우에만 태그 자산을 게시합니다. Windows x64/ARM64도 빌드,
  MSI 구조, 교체 설치와 서비스 공존 검사를 통과해야 합니다. / Release CI publishes tagged assets only
  after ARM64 compilation and CTest, DMG creation and mounting, `codesign --verify --deep --strict`, ARM64 Mach-O
  inspection, and artifact upload all pass. Windows x64 and ARM64 packages must also pass build, MSI structure,
  replacement-install, and service-coexistence checks.
- alpha.28은 Windows 절대 마우스 입력을 각 픽셀의 중앙으로 변환하고, 모니터 사이 빈 공간에 좌표가
  누적되지 않도록 물리 화면 영역으로 제한합니다. / Alpha.28 maps Windows absolute mouse input to pixel
  centers and clamps coordinates to physical displays to prevent offsets from accumulating in monitor gaps.
- 전체화면·전체 창 모드 상태를 마우스가 멈춰 있어도 갱신하고, 잠금 중 예약된 컴퓨터 전환을 취소합니다.
  Windows에서는 게임 모니터 안에 커서를 제한하며 Alt+Tab·게임 종료·이음 중지 시 복원합니다. 게임
  자체의 커서 제한은 보존합니다. / Fullscreen and borderless-window state updates continue while the mouse
  is idle, and an active lock cancels pending computer switches. On Windows, Ieum confines the cursor to the
  game's monitor and restores its restriction after Alt+Tab, game exit, or stopping Ieum while preserving the
  game's own capture.
- 회귀 테스트는 음수 원점·고해상도 픽셀 왕복, 모니터 빈 공간과 역방향 이동, 유휴 상태 보고와 연결
  종료 시 타이머 정리, 지연된 화면 전환 취소, 게임 자체 캡처·복원 실패·해상도 변경을 검사합니다.
  실제 게임별 커서 제한과 Windows↔Mac 장시간 이동은 실기 수용 테스트 대상입니다. / Regressions cover
  negative origins and high-resolution pixel round trips, monitor gaps and reverse movement, idle-state reports
  and disconnect cleanup, canceled delayed switches, game-owned capture, failed restoration, and resolution
  changes. Per-game cursor confinement and long-running Windows↔Mac movement remain physical acceptance tests.
- alpha.27의 macOS 커서 복원과 이전 종료·보조키·파일 전송 수정이 포함됩니다. Windows 실행 파일
  버전은 alpha.28에서 `0.1.128.0`으로 증가합니다. / Alpha.27's macOS cursor recovery and earlier shutdown,
  modifier, and file-transfer fixes remain included. Windows executable versions increase to `0.1.128.0` for alpha.28.
- 저장소에 Apple Developer ID 및 공증 자격 증명이 아직 구성되지 않아 macOS 패키지는 ad-hoc 서명
  상태이며 Apple 공증을 받지 않았습니다. 따라서 이 버전은 프리릴리스이고 Gatekeeper 수동 승인이
  필요할 수 있습니다. 실제 Windows 서버와 Mac 클라이언트 사이의 한/영 입력, 장시간 커서 이동과
  재연결 동작은 계속 실기 수용 테스트 대상입니다. / Apple Developer ID and notarization credentials
  are not yet configured for the repository, so macOS packages are ad-hoc signed and not notarized. This is
  therefore a prerelease and may require manual Gatekeeper approval. Korean input switching, long-running
  pointer behavior, and reconnect recovery between a physical Windows server and Mac client remain physical
  acceptance-test items.

후원금이 투입된 이후의 릴리스는 이 섹션에 배분 금액과 완료한 작업을 함께 공개합니다. Future releases
that use sponsorship funds disclose both the allocated amount and the completed work in this section.
