<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.30

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
- alpha.30은 macOS 로그인 자동 실행을 앱 로그인 항목으로 전환하고 이전 LaunchAgent를 정리합니다.
  이전 항목이 새 앱의 실행 제약과 맞지 않아 발생하는 `CODESIGNING / Launch Constraint Violation`
  보고 경로를 수정합니다. 실제 재부팅 확인은 별도 수용 테스트입니다. / Alpha.30 switches macOS
  startup to an application login item and retires the previous LaunchAgent. Native reboot acceptance
  remains separate from CI compilation and package checks.
- 숨겨진 로그 대기열과 표시 문서의 전체 문자 수를 제한하고 최신 오류 내용을 남깁니다.
  Mac·Linux에서 파일 감시가 동작하면 보조 확인 주기를 2초에서 30초로 늘립니다. Windows는
  기존 2초 복구 주기를 유지합니다. / Hidden and rendered logs retain recent context within character
  budgets. Healthy Mac/Linux file watches use a 30-second safety poll; Windows retains 2-second recovery.
- 같은 Linux Debug 조건의 로그 폭주 시험 3회 중앙값은 RSS 48.39 → 25.65 MiB였습니다.
  일반 유휴 CPU나 Mac 전체 앱의 개선율을 뜻하지 않습니다. 조건과 원본 수치는
  `docs/dev/reboot-resource-followup-2026-10-25.md`에 있습니다. / Median probe RSS in three matching
  Linux Debug log-burst runs fell from 48.39 to 25.65 MiB. General idle CPU and native Mac savings
  are not established; see the follow-up record for raw data and limitations.
- Debug 테스트가 누락한 코어 IPC 싱글턴을 초기화하도록 두 픽스처를 수정했습니다.
  전체 Linux Debug CTest 74/74를 통과했습니다. Windows 실행 파일 버전은 `0.1.130.0`입니다. /
  Two Debug fixtures now construct the core IPC singleton; all 74 Linux Debug CTest programs passed.
  Windows executable versions increase to `0.1.130.0`.
- Windows·macOS 재부팅/로그아웃, 실제 Tailscale 경로, 절전 복귀·다중 모니터·장시간 연결은
  실기 검증이 남아 있습니다. / Reboot/logout, actual Tailscale paths, sleep/wake, multiple monitors and
  long sessions remain physical acceptance tests.
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
