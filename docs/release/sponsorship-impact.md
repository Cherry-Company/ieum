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

- MacBook Pro의 공유 Apple 기본 실행 경로 `yijios-apple-primary`와 runner
  `yijios-apple-primary-macbook`는 이 릴리스 당시에도 등록되어 있었습니다. Ieum 전용 runner 복제본은
  필요하지 않습니다. 이전 문구는 "Ieum 전용 runner가 없다"는 사실을 "사용 가능한 MacBook runner가
  없다"는 의미로 잘못 설명했습니다. / The shared Apple primary lane `yijios-apple-primary` and runner
  `yijios-apple-primary-macbook` on the MacBook Pro were already registered when this release was
  published. Ieum does not require a repository-specific runner replica. The previous wording incorrectly
  conflated the absence of an Ieum-specific replica with the absence of an available MacBook runner.
- 공개된 v0.1.0-alpha.16 ARM64 DMG는 기존 워크플로가 `runs-on: macos-15`로 고정되어 있었기 때문에
  GitHub-hosted runner에서 빌드됐습니다. 이는 runner 등록 문제가 아니라 워크플로 라우팅 문제였습니다.
  현재 신뢰된 브랜치·태그·수동 ARM64 작업은 `yijios-apple-primary`를 사용하고, 외부 공개 포크 PR만
  격리를 위해 `macos-15`를 사용합니다. Intel DMG는 `macos-15-intel`에서 빌드합니다. / The published
  v0.1.0-alpha.16 ARM64 DMG was built on a GitHub-hosted runner because the old workflow hard-coded
  `runs-on: macos-15`. This was a workflow-routing defect, not a runner-registration problem. Trusted
  branch, tag, and manual ARM64 jobs now use `yijios-apple-primary`; only public fork pull requests use
  `macos-15` for isolation. Intel DMGs continue to use `macos-15-intel`.
- Windows x64 소스 빌드, MSI 검증과 실행 중 교체 설치는 `PICOPULSE409`에서 별도로 검증했습니다.
  alpha.16 태그의 Windows 및 Apple 패키지는 당시 워크플로 설정에 따라 GitHub-hosted runner에서
  생성됐습니다. / Windows x64 source, MSI, and live replacement testing was also completed on
  `PICOPULSE409`. The alpha.16 tag's Windows and Apple packages were produced on GitHub-hosted runners
  under the workflow configuration in effect at that time.

후원금이 투입된 이후의 릴리스는 이 섹션에 배분 금액과 완료한 작업을 함께 공개합니다. Future releases
that use sponsorship funds disclose both the allocated amount and the completed work in this section.
