<!-- SPDX-FileCopyrightText: (C) 2026 Cherry Inc. -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# 이음 유료화 로드맵 (제안)

작성일: 2026-07-28 · 상태: **제안이며 확정된 상품 계획이 아님**

기능 후보와 코드 검증 결과는 [유료 기능 계획](premium-features.md)에 있습니다. 이 문서는 그
결정을 전제로 **무엇을 어떤 순서로 내보내고 언제부터 파는가**를 정합니다. 구현 계획이 아니라
릴리스 계획입니다.

1차 유료 MVP는 **Instant Mesh + Carry**입니다. Carry(파일 전송)가 기존 기능 개선이 아니라 신규
구축임을 확인하고도 유지하기로 한 결정입니다. 따라서 로드맵의 성패는 *드래그 훅 없이 먼저 팔 수
있는가*에 달려 있고, 그 지점이 M3입니다.

## 로드맵을 지배하는 제약 3가지

- **아직 아무것도 팔 수 없는 상태입니다.** Windows 코드 서명과 Apple 공증이 미완이고 실기 입력
  매트릭스도 대기 중입니다. 서명 없는 바이너리로 유료 상품을 광고할 수 없습니다.
  → 첫 마일스톤은 기능이 아니라 **출시 자격**입니다.
- **후원금 배분액이 현재 USD 0입니다**([배분 내역](../release/sponsorship-impact.md)). 릴레이
  서버는 지속 비용이 발생하므로 수익 0인 상태에서 먼저 세우면 감당할 수 없습니다.
  → **서버 비용이 0인 상품을 먼저 팔고, 그 수익으로 릴레이를 세웁니다.** 이 순서가 뼈대입니다.
- **Carry의 전송 코어를 얹을 자리에 결함이 있습니다.** `ClientProxy1_5.cpp:36-47`의 스트림
  desync를 먼저 정리하지 않으면 새 프로토콜을 결함 위에 얹게 됩니다.

## 마일스톤

규모는 상대 표기(S/M/L/XL)입니다. 개발 인력이 사실상 1명이므로 캘린더 날짜 대신 순서와 규모로
표기하고, 병렬 가능한 트랙만 따로 둡니다.

### M0 · `alpha.16` 기반 수리 · 규모 M · 무료

유료 기능이 전부 이 위에 올라가므로 선행합니다. 5건을 각각 독립 커밋으로 처리합니다.

| 항목 | 위치 | 규모 |
| --- | --- | --- |
| `DFTR`/`DDRG` 스텁의 스트림 desync 수정 | `ClientProxy1_5.cpp:36-47` | S |
| `fakeAllButtonsUp()` 신설 (`KeyState::fakeAllKeysUp()`과 대칭) | `IPlatformScreen.h` → 플랫폼 4종, 호출 `Client.cpp:546-559`, `Server.cpp:2118-2155` | M |
| Tailscale 조회 비동기화 (GUI 최대 2초 블로킹 제거) | `TailscaleIntegration.cpp:134-166`, `MainWindow.cpp:1465,1586` | S |
| 클라이언트 측 네트워크 변경 반응 (현재 서버 전용) | `NetworkMonitor.cpp`, `ClientApp.cpp:180-226` | M |
| 재시도 카운터 리셋 비대칭 정리 | `ClientApp.cpp:226` vs `:171-178` | S |

**완료 기준**: 각 항목에 회귀 테스트가 붙고 `ctest` 통과. desync 회귀 테스트는
`ClipboardChunksTests.cpp`의 `MemoryStream` 페이크를 재사용합니다.

### M1 · `beta.1` 출시 자격 · 규모 M · **판매 개시 지점**

기능 개발이 아니라 신뢰 확보입니다. 인증서 발급과 Apple 계정 승인에 행정 리드타임이 있어
**코드 작업과 병렬로 지금 착수해야 하는 크리티컬 패스**입니다.

- Windows 코드 서명, Apple Developer ID 서명·공증
- 안정 업데이트 채널 ([업데이트 전달 기준](../dev/update-delivery.md)의 서명 검증·단계적
  교체·롤백 충족)
- Windows↔macOS 실기 입력 매트릭스 공개 ([Phase 0 보고서](../../phase0_report.md)의 미실행 셀)

여기서 **`Ieum Plus 평생`(일회성) 얼리버드 판매를 시작**합니다. 판매 대상은 서명 빌드와 업데이트
채널이며 Carry는 "포함 예정"으로 명시합니다. 선주문 성격이므로 **할인가로 팔고 미이행 시 환불
조건을 문서화**합니다. 서버 비용이 0인 상품이라 릴레이 없이 매출이 발생합니다.

**완료 기준**: 설치 시 경고창 없음. 실기 수용 기준(한/영 전환 20회 불일치 0건, 자소 분리 0건,
입력 지연 p95 증가 2ms 미만) 공개.

### M2 · `beta.2` Instant Mesh · 규모 L · 무료(릴레이 제외)

"기기 목록"을 "이음 서버 목록"으로 바꾸고, 사람이 지문 hex를 눈으로 대조하지 않게 만듭니다.

- **서비스 발견**: Tailscale 피어 목록(`TailscaleIntegration.cpp:93-100`)에 24800 포트 프로브와
  capability 응답을 얹습니다. LAN은 UDP 브로드캐스트 발견을 신규 구현합니다(현재 UDP 소켓이
  전무하여 `arch/` 소켓 계층 확장 필요). mDNS는 의존성이 무거워 제외합니다.
- **짧은 코드 페어링**: 서버가 6자리 코드를 표시 → 클라이언트 입력 → 코드에서 파생한 값으로 지문
  상호 검증(SAS) → `FingerprintDatabase::addTrusted()` 기록. 재사용은
  `FingerprintDatabase.cpp:13-91`, `SecureUtils.cpp:66-113`이고 실패 시 기존 `FingerprintDialog`로
  폴백합니다. **`SecureSocket::verifyCertFingerprint`의 fail-closed 동작은 유지하며 TOFU 자동
  신뢰로 바꾸지 않습니다.**
- **경로 선택**: LAN 직결 → Tailscale. 릴레이 자리는 인터페이스만 잡고 비워 둡니다.
- **무중단 복구**: M0의 클라이언트 네트워크 반응 위에 세션 유지를 얹습니다.

**과금 경계**: 발견과 페어링은 **무료**입니다. 진입장벽 제거가 유입 경로이며 여기를 잠그면
Community의 가치가 훼손됩니다. 릴레이만 유료입니다.

**완료 기준**: Wi-Fi 전환·절전 복귀·Tailscale 재시작 각 20회에서 세션 유지, 잔류 modifier·버튼 0건.

### M3 · `beta.4` Carry 1단계(드래그 없음) · 규모 L · Plus 가치 완성

드래그 훅 없이 트레이 드롭존과 핫키로 출시합니다. 이것만으로 폴더·대용량·크로스 플랫폼·한글
파일명 우위가 성립합니다.

- 프로토콜 1.11에 새 메시지 세트(start/chunk/end/ack/resume). 폐기된 `DFTR`/`DDRG`는 재사용하지
  않습니다. 상대 경로를 실어 폴더를 지원하고 SHA-256 무결성과 이어받기를 넣습니다. 재사용:
  `StreamChunker.cpp:14`(512KiB 청크), `ClipboardChunk.cpp:79-162`(조립·검증 패턴),
  `ClientProxy1_9.cpp`(59줄, 버전별 메시지 추가 템플릿).
- 한글 파일명 NFC/NFD **양방향** 정규화. 텍스트 정규화의 아웃바운드 누락
  (`OSXClipboardAnyTextConverter.cpp:54-58`)도 함께 수정합니다. 재사용: `normalizeNfc()`.
- 클립보드 히스토리 + 초성 검색. 선행 수정: 클라이언트 수신 상한이 서버 협상값이 아니라 로컬
  설정에서 나오는 문제(`Client.cpp:63-65` vs `:366-370`).

여기서 **Plus 평생을 정가로 전환**합니다. M1 얼리버드 구매자에 대한 약속이 이행되는 지점입니다.

**완료 기준**: 1GB 폴더 전송 중 네트워크 전환 후 이어받기 성공, 한글 파일명 자소 분리 0건.

### M4 · `rc.1` Carry 2단계(드래그 훅) · 규모 XL · 후속 무료 업데이트

Windows `IDropTarget`/`IDropSource`, macOS `NSPasteboard` 드래그, X11 XDND. 세 플랫폼이 서로 다른
API라 사실상 세 번 만듭니다. **로드맵 최대 리스크 구간이지만 판매는 이미 M3에서 시작했으므로
여기서 지연되어도 매출이 막히지 않습니다.** "맥 폴더를 윈도우로 끌어 놓기" 시연은 이 시점부터
가능합니다.

### M5 · `v1.0` 정식 · 규모 S

수용 기준 확정, 문서 정리, Plus 정가 판매 안정화.

### M6+ · 이후

- **Input Memory (앱별 한/영 기억)** · 규모 M · 이음의 유일한 구조적 차별점.
  `MSWindowsImeController.cpp:80`이 이미 `GetForegroundWindow()`를 호출하며 PID를 `nullptr`로
  버리고 있어 앱 식별은 약 10줄입니다. macOS는 `OSXInputSourceController.mm:94-97`의 기존 TIS
  옵저버 옆에 `NSWorkspace` 알림을 추가합니다. **Linux는 IME 컨트롤러가 없어 Windows·macOS
  전용입니다.**
- **Spaces (장소별 프로필)** · 규모 M. 이음매는 `ServerConfig::commit()`/`recall()`의
  `internalConfig` 그룹명 매개변수화(`ServerConfig.cpp:76-139`)입니다. 새 최상위 그룹은
  `Settings::removeUnknownScreens()`(`Settings.cpp:428-436`)가 삭제하므로 `m_validGroup` 등록이
  필요합니다. GUI→코어 런타임 명령 채널이 없어(IPC는 코어→GUI 단방향) 전환은 설정 재작성 + 코어
  재시작으로 구현됩니다.
- **Guardian (진단)** · 규모 M. 기존 신호 4개(AX 권한, Tailscale 상태, 설정 파일 가독성, TLS 지문)를
  묶는 집계 계층입니다. `deskflow-gui.cpp:261-330`의 권한 게이트 루프가 UX 템플릿입니다. 신규로
  필요한 것은 macOS Input Monitoring 확인(현재 Accessibility만 확인), 방화벽 확인, Windows 서비스
  헬스입니다.
- **Audio Follow (실험)** · 규모 XL · 사실상 별도 제품. Windows 서명 드라이버와 macOS
  `AudioServerPlugIn` + 공증이 필요하고 오디오 서브시스템이 전무합니다. M4 이후에만 착수합니다.

## 병렬 트랙

**트랙 A — 서명·공증 (지금 착수)**: 행정 리드타임이 있어 코드와 무관하게 즉시 시작합니다.
M1의 크리티컬 패스입니다.

**트랙 B — 릴레이 인프라 (M3의 매출 확인 후 착수)**: `Ieum Anywhere` 구독의 실체입니다.
**M2에 넣지 않습니다** — 발견·페어링과 릴레이를 같은 마일스톤에 묶으면 규모가 XL로 부풀고,
수익 0인 상태에서 서버 비용이 먼저 발생합니다.

```
Plus 평생 판매(서버비 0) → 매출 발생 → 릴레이 구축 → Anywhere 구독 개시
```

## 요금제와 개시 시점

| 상품 | 형태 | 포함 | 개시 시점 |
| --- | --- | --- | --- |
| Community | 무료 | 로컬 KVM, IME 동기화, 클립보드, M0 수리, M2의 발견·페어링 | 즉시 |
| Ieum Plus 평생 | 일회성 | 서명·공증 빌드, 안정 업데이트, 로컬 Carry(폴더·이어받기·정규화), 로컬 클립보드 히스토리 | M1 얼리버드 → M3 정가 |
| Ieum Anywhere | 구독 | 계정 기반 기기 발견, 암호화 릴레이, 클라우드 동기화, 게스트 연결 | 트랙 B 완료 후 |
| Teams | 구독(사용자당) | 중앙 배포, SSO, 장치 정책, 감사 기록, 지원 | M5 이후 |

일회성과 구독은 **항목이 겹치지 않게** 나눕니다. 평생 상품에 릴레이를 넣으면 매출은 1회인데
비용은 영구가 되므로 그 경계는 흐리지 않습니다.

## 리스크

| 리스크 | 영향 | 대응 |
| --- | --- | --- |
| M4 드래그 훅이 3플랫폼 신규 구축 | 일정 최대 변수 | M3에서 이미 판매 개시. M4 지연이 매출을 막지 않도록 순서 고정 |
| M1 얼리버드가 선주문 성격 | 신뢰 손상 | 할인가 + 환불 조건 문서화 + 미이행 항목 명시 |
| 릴레이 비용이 수익보다 먼저 발생 | 자금 고갈 | 트랙 B를 M3 이후로 고정 |
| 서명 인증서 리드타임 | M1 전체 지연 | 트랙 A를 지금 착수 |
| 개발 인력 1명 | 전 구간 | 마일스톤당 항목 수를 줄이고 M0을 독립 커밋으로 분할 |
| Linux는 IME·드래그 모두 미지원 | 기능 비대칭 | Linux는 Community 중심으로 포지셔닝하고 유료 기능 범위를 판매 페이지에 명시 |

## 검증

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

마일스톤별 완료 기준은 위 각 절에 있습니다. 자동 테스트로 대체할 수 없는 항목(실기 매트릭스,
네트워크 전환, 전송 이어받기)은 **두 대의 실기에서 확인하기 전까지 유료 상품으로 광고하지
않습니다.**

## 비목표

- GPL 코어 안에 결제 잠금을 넣지 않습니다.
- iPadOS 전체 시스템 KVM은 공개 API가 없어 약속하지 않습니다.
- DDC 모니터 입력 전환은 이번 로드맵에서 제외합니다 (Apple Silicon 공개 API 부재).
- 연결 안정성, IME, 기본 클립보드, 보안 업데이트, 기기 발견·페어링은 영구 무료로 유지합니다.

---

이 문서는 제품 방향 제안이며 법률 자문이 아닙니다. 실제 유료 배포 전에는 저작권·상표·서비스
경계를 별도로 검토해야 합니다.
