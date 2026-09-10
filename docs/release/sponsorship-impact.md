<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

## 후원 투명성 / Sponsorship transparency

Release: v0.1.0-alpha.27

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
- alpha.27은 macOS 커서 숨김·표시 요청을 직렬화하고, 중복 숨김으로 Quartz의 숨김 횟수가 누적되지
  않도록 합니다. 실패한 표시 요청은 다음 복원 때 재시도합니다. / Alpha.27 serializes macOS cursor
  visibility changes, prevents repeated hides from accumulating Quartz hide counts, and retries a failed show
  on the next restore request.
- Mac 클라이언트에서 로컬 마우스 이동·드래그·클릭·스크롤을 감지하면 Ieum이 숨긴 커서를 복원합니다.
  Ieum이 생성한 원격 입력과 커서 대기 위치 이동은 복원을 유발하지 않습니다. / Local mouse movement,
  dragging, clicks, and scrolling restore the cursor hidden by Ieum on a Mac client. Synthetic remote input
  and cursor parking do not trigger this recovery.
- 회귀 테스트는 반복 화면 전환, 실패 후 재시도, 이벤트 탭과 화면 스레드의 동시 복원, 로컬·합성 입력
  구분을 검사합니다. 실제 커서를 숨기거나 움직이지 않고 실행합니다. / Focused regression tests cover
  repeated screen transitions, retries, concurrent event-tap and screen-thread changes, and local versus
  synthetic pointer events without hiding or moving the real cursor.
- alpha.26까지의 종료·보조키·파일 전송 수정이 포함됩니다. Windows 실행 파일 버전은 alpha.27에서
  `0.1.127.0`으로 증가합니다. / Shutdown, modifier, and file-transfer fixes through alpha.26 remain included.
  The Windows executable version increases to `0.1.127.0` for alpha.27.
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
