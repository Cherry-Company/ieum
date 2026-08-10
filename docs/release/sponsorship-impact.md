<!-- SPDX-FileCopyrightText: (C) 2026 Cherry Inc. -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.19

- 이번 릴리스에 배분된 GitHub Sponsors 후원금 / Sponsor funds allocated to this release: **USD 0**
- 후원금으로 완료한 작업 / Work claimed as sponsor-funded: **없음 / None**
- 이유 / Reason: 이번 릴리스에 배정된 후원금 없이 독립적으로 개발했습니다. / This release was
  developed independently without sponsor funds allocated to it.
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
- alpha.19은 혼합 DPI 화면에서 경계 비율을 두 번 적용하던 좌표 경로를 누적 물리 경계 영역 매핑으로
  교체했고 Per-Monitor V2 DPI 인식을 사용합니다. Windows 네이티브 빌드와 좌표·업데이트·진단 회귀
  테스트, x64 글로벌·한국어 MSI 구조 검증을 로컬에서 통과했습니다. 다만 실제 게임별 재시험과
  Windows↔macOS 장시간 좌표 왕복이 끝날 때까지 해결 완료로 분류하지 않습니다. / Alpha.19
  replaces the double-applied mixed-DPI edge ratio with cumulative physical-edge span mapping and enables
  Per-Monitor V2 DPI awareness. Native Windows builds, coordinate/update/diagnostic regression tests, and x64
  global/Korean MSI structure validation pass locally. Per-game retesting and sustained Windows-to-macOS
  coordinate traversal remain physical acceptance items.
- Windows 설치 경로는 실행 중인 새 GUI에 정상 종료를 요청한 뒤 파일을 교체하고, 이 명령을 모르는
  alpha.18 이하 프로세스만 기존 MSI 강제 종료 규칙으로 정리합니다. 다운로드 중에는 KVM이 계속 동작하지만
  바이너리 교체 순간의 짧은 재시작은 필요합니다. / The Windows installer asks a current GUI to shut down
  cleanly before replacement and retains the MSI termination fallback only for alpha.18-and-older processes
  that do not understand the handoff. KVM remains available during download, but a short restart at replacement
  time is still required.
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
