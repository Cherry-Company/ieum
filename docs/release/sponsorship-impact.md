<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.29

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
- alpha.29는 첫 연결 안내, 연결 상태 설명, 실행 중 호스트 자동 전환, 검색 가능한 설정과 컴퓨터
  배치를 제공합니다. README는 설치와 사용 순서 중심으로 정리했습니다. / Alpha.29 adds first-connection
  guidance, clearer connection states, automatic host switching while running, searchable settings, and a
  simpler computer-layout editor. The README now focuses on installation and getting connected.
- 재부팅을 비정상 종료로 오인하는 진단을 줄이고, 오류 보고서 미리보기·복사·저장과 짧은 GitHub
  보고 링크를 제공합니다. 종료와 재시작은 코어 프로세스가 정리된 후 진행합니다. / Boot-session-aware
  diagnostics reduce false crash reports after restarting the OS. Reports can be reviewed, copied, saved,
  and opened through a short GitHub link. Core shutdown completes before a replacement process starts.
- macOS 입력 소스 조회의 null 접근, 클립보드 자원 정리, 이벤트 탭과 전원 감시 실행 루프의 수명을
  수정했습니다. 안정된 코드 서명이 없는 개발·ad-hoc 빌드는 Notification Center 호출을 생략합니다.
  / macOS fixes cover null input-source lists, clipboard cleanup, event-tap and power-monitor run-loop
  lifetimes. Development and ad-hoc builds without a stable signing identity skip Notification Center calls.
- 공통 타이머의 초 미만 정밀도를 복원하고, 이벤트 대기·IPC 대기열·화면 로그의 자원 사용을 줄였습니다.
  Linux의 제한된 비교 측정과 원본 결과는 `docs/dev/resource-comparison-2026-10-15.md`에 있습니다.
  모든 플랫폼의 CPU·RAM 개선율을 보장하는 수치는 아닙니다. / Shared timers regain subsecond precision;
  event waiting, queued IPC messages, and displayed logs use bounded resources. Scoped Linux measurements
  and raw results are documented in `docs/dev/resource-comparison-2026-10-15.md`; they do not establish a
  whole-application CPU or RAM improvement across platforms.
- 로컬 Linux 검증은 CTest 73개 대상, 기존 테스트 27개, CI 도구 테스트 26개와 실제 TLS 연결·키보드·
  포인터 입력·호스트 전환을 통과했습니다. Windows 실행 파일 버전은 `0.1.129.0`입니다. / Local Linux
  validation passed 73 CTest targets, 27 legacy tests, 26 CI-tool tests, and real TLS connection, keyboard,
  pointer, and host-switch checks. Windows executable versions increase to `0.1.129.0`.
- 사용자가 보고한 macOS 강제 종료 자체의 재현, Windows·macOS 재부팅/로그아웃, 실제 Tailscale 경로,
  절전 복귀·다중 모니터·장시간 연결은 실기 검증이 남아 있습니다. 상세 범위는
  `docs/user/reliability-validation.md`에 기록했습니다. / Reproducing the reported macOS crash, Windows
  and macOS reboot/logout behavior, a real Tailscale path, sleep/wake, multiple monitors, and long sessions
  still require physical acceptance testing. See `docs/user/reliability-validation.md` for the exact scope.
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
