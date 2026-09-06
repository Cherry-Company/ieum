<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.26

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
- alpha.26은 Windows가 종료를 확정할 때 트레이 실행 중에도 세션 흔적을 정리하고, macOS에서는 이전
  부팅의 흔적만으로 앱 충돌이라고 판정하지 않습니다. 종료 시 코어의 순간 상태가 다음 자동 시작 설정을
  덮어쓰지 않도록 했습니다. / Alpha.26 handles confirmed Windows shutdown even during tray-only startup,
  distinguishes a previous macOS boot from an interrupted app session, and preserves the user's automatic-start
  intent when the core stops before the GUI during shutdown.
- Windows는 양쪽 보조키의 개별 누름·해제를 전달하고, macOS는 오른쪽 Command·Option·Control·Shift를
  클릭 상태에 반영합니다. 한쪽 키를 떼어도 반대쪽이 눌려 있으면 보조키가 유지됩니다. / Windows preserves
  separate left/right modifier transitions, and macOS tracks right Command, Option, Control, and Shift for mouse
  events, retaining the modifier until both sides are released.
- 회귀 테스트는 종료 확정·취소, 부팅 식별자 변경과 양쪽 보조키 상태를 검사합니다. 실제 재부팅과 USB
  키보드 분리·재연결에 대한 실기 검증은 별도 수용 테스트입니다. / Regression tests cover confirmed and
  cancelled shutdown, boot identity changes, and sided modifier state. Physical reboot and USB keyboard reconnect
  validation remain separate acceptance tests.
- Windows 실행 파일 버전은 MSI ProductVersion과 함께 릴리스마다 증가하며 alpha.26은 `0.1.126.0`입니다.
  설치기는 기존 제품 제거와 두 번째 파일 교체 단계를 모두 끝낸 뒤에만 Ieum 서비스를 다시 시작합니다. /
  Windows executable versions now increase with every MSI release (`0.1.126.0` for alpha.26), and the installer
  restarts the Ieum service only after old-product removal and the second file-replacement pass both finish.
- alpha.25까지의 파일 전송·설치·재시도 수정과 Pro Local의 라이선스·무덮어쓰기·SHA-256 검증 경계는
  그대로 포함됩니다. / Transfer, installation, and retry fixes through alpha.25 remain included, along with
  Pro Local's license, no-overwrite, and SHA-256 validation boundaries.
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
