<!-- SPDX-FileCopyrightText: (C) 2026 Cherry Inc. -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.16

- 이번 릴리스에 배분된 GitHub Sponsors 후원금 / Sponsor funds allocated to this release: **USD 0**
- 후원금으로 완료한 작업 / Work claimed as sponsor-funded: **없음 / None**
- 이유 / Reason: 이번 릴리스에 배정된 후원금 없이 독립적으로 개발했습니다. / This release was
  developed independently without sponsor funds allocated to it.
- 다음 우선순위 / Next funding priorities: Windows 정식 코드 서명, Apple Developer ID 서명·공증,
  Windows ARM64·Apple Silicon 실기 회귀 테스트. / Production Windows signing, Apple Developer ID
  signing and notarization, and physical Windows ARM64 and Apple Silicon regression testing.

## 빌드 및 실기 검증 범위 / Build and physical validation scope

- 사용자의 MacBook Pro는 다른 개인 저장소에서 `dev-macbook` self-hosted runner로 이미 정상
  운용 중입니다. 다만 이 릴리스 시점에는 Ieum 전용 `dev-macbook-ieum` 등록과 워크플로 라우팅이
  없었습니다. 따라서 macOS ARM64·Intel DMG는 MacBook Pro가 아니라 GitHub Actions의 임시 macOS
  runner(`macos-15`, `macos-15-intel`)에서 빌드하고 패키지 검사했습니다. 실제 MacBook Pro 수용
  테스트는 아직 완료하지 않았으므로 Apple 패키지는 **빌드·패키지 검증 완료, 실기 검증 대기**로
  표시합니다. / The user's MacBook Pro was already operating normally as a `dev-macbook` self-hosted
  runner for other personal repositories. However, this release did not yet have an Ieum-specific
  `dev-macbook-ieum` registration or workflow route. The macOS ARM64 and Intel DMGs were therefore built
  and package-tested on ephemeral GitHub-hosted macOS runners (`macos-15` and `macos-15-intel`), not on
  the MacBook Pro. Physical MacBook Pro acceptance testing is still pending, so the Apple packages are
  **build/package verified and awaiting physical validation**.
- 이번 릴리스 시점의 저장소 전용 self-hosted runner 등록 수는 0개입니다. Windows x64 소스 빌드,
  MSI 검증과 실행 중 교체 설치는 `PICOPULSE409`에서 별도로 검증했습니다. 태그의 교차 플랫폼
  패키징은 문서화된 예외로 GitHub-hosted runner를 사용했습니다. / At this release point, the
  repository has zero registered self-hosted runners. Windows x64 source, MSI, and live replacement
  testing was also completed on `PICOPULSE409`; tag packaging used GitHub-hosted runners as a documented
  exception.

후원금이 투입된 이후의 릴리스는 이 섹션에 배분 금액과 완료한 작업을 함께 공개합니다. Future releases
that use sponsorship funds disclose both the allocated amount and the completed work in this section.
