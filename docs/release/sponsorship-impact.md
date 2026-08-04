<!-- SPDX-FileCopyrightText: (C) 2026 Cherry Inc. -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.17

- 이번 릴리스에 배분된 GitHub Sponsors 후원금 / Sponsor funds allocated to this release: **USD 0**
- 후원금으로 완료한 작업 / Work claimed as sponsor-funded: **없음 / None**
- 이유 / Reason: 이번 릴리스에 배정된 후원금 없이 독립적으로 개발했습니다. / This release was
  developed independently without sponsor funds allocated to it.
- 다음 우선순위 / Next funding priorities: Windows 정식 코드 서명, Apple Developer ID 서명·공증,
  Windows ARM64·Apple Silicon 실기 회귀 테스트. / Production Windows signing, Apple Developer ID
  signing and notarization, and physical Windows ARM64 and Apple Silicon regression testing.

## 빌드 및 실기 검증 범위 / Build and physical validation scope

- Apple Silicon 패키지는 공유 Apple 기본 경로 `yijios-apple-primary`를 통해 MacBook Pro의
  `yijios-apple-primary-macbook`에서 네이티브 ARM64로 빌드합니다. 외부 공개 포크 PR만 격리를 위해
  `macos-15`를 사용하며 Intel DMG는 `macos-15-intel`에서 빌드합니다. / Apple Silicon packages are
  built natively on the MacBook Pro runner `yijios-apple-primary-macbook` through the shared Apple primary
  lane `yijios-apple-primary`. Only public fork pull requests use `macos-15` for isolation, while Intel DMGs
  use `macos-15-intel`.
- 릴리스 전 최종 커밋은 MacBook Pro에서 ARM64 컴파일, 37개 CTest 대상, DMG 생성·마운트,
  `codesign --verify --deep --strict`, ARM64 Mach-O 확인과 아티팩트 업로드를 통과했습니다. Windows
  x64/ARM64 설치 파일도 빌드, MSI 구조, 교체 설치와 서비스 공존 검사를 통과했습니다. / The final
  pre-release commit passed native ARM64 compilation, all 37 CTest targets, DMG creation and mounting,
  `codesign --verify --deep --strict`, ARM64 Mach-O inspection, and artifact upload on the MacBook Pro.
  Windows x64 and ARM64 packages passed build, MSI structure, replacement-install, and service-coexistence
  checks.
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
