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

## CI 실행 경로 / CI execution route

- 이번 릴리스 시점에는 저장소 전용 `dev-picopulse`·`dev-macbook` runner가 등록되어 있지 않아 태그의
  교차 플랫폼 패키징은 임시 GitHub-hosted runner를 사용합니다. Windows x64 소스 빌드, MSI 검증과
  실행 중 교체 설치는 `PICOPULSE409`에서 별도로 검증했습니다. Apple 패키지는 `dev-macbook` 경로가
  복구되어 실기 수용 테스트를 마치기 전까지 빌드 검증으로만 표시합니다. / No repository-scoped
  `dev-picopulse` or `dev-macbook` runner is registered at this release point, so tag packaging temporarily
  uses ephemeral GitHub-hosted runners. Windows x64 source, MSI, and live replacement testing was also completed
  on `PICOPULSE409`; Apple packages remain build-verified only until the `dev-macbook` physical acceptance lane
  is restored.

후원금이 투입된 이후의 릴리스는 이 섹션에 배분 금액과 완료한 작업을 함께 공개합니다. Future releases
that use sponsorship funds disclose both the allocated amount and the completed work in this section.
