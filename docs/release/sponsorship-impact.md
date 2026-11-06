<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.31

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
- alpha.31은 내 책상을 기본 화면으로 바꾸고, 기기를 선택했을 때 정보 패널을 엽니다. 역할·주소는
  접을 수 있는 연결 설정에 두며 배치 편집은 저장·취소를 지원합니다. 외부 파일과 다른 창에서 바뀐
  배치를 보호하고, 좁은 창과 밝음/어두움에 대응합니다. / Alpha.31 introduces a desk-first workspace,
  contextual computer details and transactional layout editing, with external-file and concurrent-edit protection.
- Mac 상단 조작부는 시스템 반투명 재질을 사용하고, 투명도 줄이기·고대비에서는 불투명하게
  표시합니다. 최신 Liquid Glass와 동일한 광학 표현을 보장하지 않습니다. / The Mac toolbar uses a
  native system material with opaque accessibility fallbacks; it does not claim identical latest Liquid Glass optics.
- 이전 로그인 항목 정리를 손쉬운 사용 권한 안내보다 먼저 실행합니다. 권한 안내를 닫아도 정리
  시도를 건너뛰지 않습니다. 실제 `Launch Constraint Violation` 재발 여부는 설치된 Mac의 재부팅으로
  확인해야 합니다. / Legacy login-item cleanup now runs before Accessibility approval, so closing the guide
  cannot skip migration. Installed-Mac reboot acceptance is still required.
- Linux Debug에서 관련 GUI CTest 프로그램 25개와 밝음/어두움·작은 창 렌더링을 확인했습니다.
  새 UI에 반복 애니메이션이나 주기적인 타이머를 추가하지 않았지만, 새 CPU/RAM 절감률을 측정한 것은
  아닙니다. / All 25 related Linux Debug GUI CTest programs passed. No new resource-saving percentage is claimed.
- Windows 실행 파일 버전은 `0.1.131.0`입니다. 기존 입력 공유·호스트 전환·지문 승인·로그 제한을
  유지합니다. / Windows executable versions increase to `0.1.131.0`; existing sharing, host switching,
  fingerprint approval and bounded logging behavior remain available.
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
