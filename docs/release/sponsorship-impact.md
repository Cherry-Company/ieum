<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.23

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
- alpha.23은 최종 핸드셰이크 옵션보다 먼저 마우스 이동 프레임이 도착해 `DMMV` 오류로 끊기던 경쟁을
  제거하고, 시작 중·재시도 중에도 중지 버튼이 비활성화되지 않게 합니다. Pro Local의 라이선스·무덮어쓰기·
  SHA-256 검증 경계는 그대로 유지합니다. / Alpha.23 removes the race that could deliver a `DMMV` mouse frame
  before final handshake options and keeps Stop available while the core is starting or retrying. Pro Local's
  license, no-overwrite, and SHA-256 validation boundaries remain unchanged.
- macOS에서는 빨간 닫기 버튼이 앱을 메뉴바에 남기고, 재시작·로그아웃은 정상 종료로 기록합니다. 손쉬운
  사용 안내는 Applications에서 현재 앱 보기, 필요 시 이전 승인 초기화, 현재 앱 등록, 설정 열기, 권한
  확인 순으로 고정하며 자동 등록 실패 시 `+`에서 `/Applications/Ieum.app`을 직접 선택할 수 있습니다. /
  On macOS, the red close button leaves Ieum in the menu bar, while restart and logout are recorded as clean exits.
  The Accessibility guide uses a fixed sequence and exposes `/Applications/Ieum.app` for manual `+` selection when
  automatic registration fails.
- Windows 업데이트 보조 실행은 기존 GUI가 응답했다는 확인만 받지 않고 단일 인스턴스 자원이 실제로
  해제될 때까지 기다립니다. 설치기는 서비스 중지와 제한 시간 후 Ieum GUI·코어 종료 안전장치를 계속
  유지합니다. / The Windows update helper waits for the previous GUI's singleton resource to be released instead
  of returning on acknowledgement alone. The installer retains service shutdown and bounded GUI/core termination
  fallbacks.
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
