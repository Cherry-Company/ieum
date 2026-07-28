<!-- SPDX-FileCopyrightText: (C) 2026 Cherry Inc. -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# 이음 유료 기능 계획 (코드 검증본)

작성일: 2026-07-28 · 상태: **제안이며 확정된 상품 계획이 아님**

이 문서는 유료 기능 후보를 **코드베이스에 대고 검증한 결과**로 갱신한 것입니다. 초판(같은 날
작성)에는 검증 없이 쓴 오류가 있었으므로 아래 §1에 정정 내용을 남깁니다.

## 0. 과금 경계 원칙

코어는 `GPL-2.0-only`입니다. 실행 파일 안에 기능을 넣고 잠그는 모델은 README의 공개 방침과
충돌하고, 누구나 재빌드하면 뚫리므로 방어되지 않습니다.

> **서버 비용이 드는 것과 안 드는 것을 나눈다.** 전자는 구독, 후자는 일회성으로 판다.

같은 기능에 두 가격표를 붙이는 것이 아니라 항목 자체를 분리합니다. 상세는 §4.

---

## 1. 초판 정정

| 초판 서술 | 사실 |
| --- | --- |
| "드래그 전송 배관은 이미 있다 (`ClientProxy1_5.h:26`)" | **틀림.** 헤더 선언만 남았고 구현은 빈 스텁입니다. `ClientProxy1_5.cpp:26-34`와 `:49-57`이 전부 `// do nothing`이고, 프로토콜 상수는 `ProtocolTypes.h:1030,1061`에서 `@deprecated`로 표시되어 있습니다. 파일 전송은 **개선이 아니라 신규 구축**입니다. |
| "조합 이월(preedit carry-over)을 만들 수 있다" | **불가능.** `CILS`의 `composing` 필드는 예약 상태로 **항상 0**입니다(`ProtocolTypes.h:1175-1179`). 타 프로세스의 조합 상태를 읽는 공개 API가 Windows·macOS 모두 없기 때문입니다. 로드맵에서 내립니다. |

## 2. 현재 코드 기준 사실 확인

유료 기능을 설계하기 전에 확인한 실제 상태입니다.

| 영역 | 상태 | 근거 |
| --- | --- | --- |
| 3대 이상 메시 | **이미 동작** (5×3 그리드, N 클라이언트) | `Constants.h.in:30-31`, `ServerConfig.cpp:65-74` |
| 파일 전송·드래그 | **전 플랫폼 부재** | `MSWindowsScreen.h:29` 전방 선언만(클래스 없음), `OSXScreen.h:321-322` 죽은 멤버, X11에 XDND 0건 |
| 장치 발견 | **없음.** UDP 소켓 자체가 코드베이스에 0개 | mDNS/broadcast/zeroconf 검색 결과 없음 |
| Tailscale 연동 | 피어 목록은 **기기 목록이지 이음 서버 목록이 아님.** 포트 프로브·capability 교환 없음 | `TailscaleIntegration.cpp:168-231`, `MainWindow.cpp:1469-1553` |
| 페어링 | 양쪽에서 사람이 SHA-256 hex/랜덤아트를 **눈으로 대조** | `FingerprintDialog.cpp:16-132`, `MainWindow.cpp:1044-1087` |
| 재접속 시 입력 정리 | 키는 해제됨. **마우스 버튼 해제 경로 없음** | `KeyState.cpp:944-958`, `ServerProxy.cpp:982-988` / `fakeAllButtonsUp` 0건 |
| 클립보드 | 청크·크기 상한 포함 정상 동작. 히스토리 없음 | `StreamChunker.cpp:14`, `ClipboardChunk.cpp:79-162` |
| NFC 정규화 | macOS **인바운드 전용, 기본 꺼짐**. 파일명에는 미적용 | `OSXClipboardAnyTextConverter.cpp:16-41,54-58,64` |
| 포그라운드 앱 감지 | **없음.** 단 Windows는 이미 `GetForegroundWindow()`를 호출하며 PID를 버리는 중 | `MSWindowsImeController.cpp:80-81` |
| Linux IME 제어 | **없음** (ibus/fcitx 연동 전무) | `IPlatformScreen.h:142,148` 기본 no-op |
| DDC/CI, 오디오 | **전무** | 플랫폼 CMake에 관련 의존성 없음 |

### 지금 고쳐야 할 결함 3건

1. **스트림 desync**: `ClientProxy1_5.cpp:36-47`이 `DFTR`/`DDRG`를 매칭하고 **페이로드를 한 바이트도
   읽지 않은 채 `true`를 반환**합니다. 남은 페이로드가 메시지 코드로 재해석되어
   `ClientProxy1_0.cpp:130-132`의 스트림 전체 드레인을 유발하고, 큐에 있던 정상 메시지가 버려집니다.
   이음은 프로토콜 1.10을 광고하므로 레거시 Synergy/Barrier 클라이언트가 트리거할 수 있습니다.
2. **마우스 버튼 잔류**: 하드 절단 시 클라이언트는 `leave`를 받지 못하고, `Screen::disable`의 강제
   `leave()`는 `fakeAllKeysUp`만 수행합니다. 눌린 버튼은 그대로 남습니다.
3. **GUI 프리즈**: `TailscaleIntegration::query()`가 `waitForFinished`로 **GUI 스레드를 최대 2초
   블로킹**합니다(`TailscaleIntegration.cpp:134-166`, 호출부 `MainWindow.cpp:1465,1586`).

---

## 3. 단계별 계획

### Phase 0 — 기반 수리 (무료, 유료 기능의 전제조건)

| 작업 | 위치 |
| --- | --- |
| `DFTR`/`DDRG` 스텁의 스트림 desync 수정 | `ClientProxy1_5.cpp:36-47` |
| `fakeAllButtonsUp()` 신설 — `KeyState::fakeAllKeysUp()`과 대칭 | `IPlatformScreen.h` → 플랫폼 4종, 호출은 `Client.cpp:546-559`, `Server.cpp:2118-2155` |
| Tailscale 조회 비동기화 | `TailscaleIntegration.cpp:134-166`, `MainWindow.cpp:1459-1467,1580-1615` |
| 클라이언트 측 네트워크 변경 반응 (현재 서버 전용) | `NetworkMonitor.cpp`, `ClientApp.cpp:180-226` |
| 재시도 카운터 리셋 비대칭 정리 | `ClientApp.cpp:226` vs `:171-178` |

### Phase 1 — Instant Mesh (첫 유료 상품)

핵심은 **기기 목록을 이음 서버 목록으로 바꾸고, 사람이 hex를 대조하지 않게 만드는 것**입니다.

- **서비스 발견**: Tailscale 피어(`TailscaleIntegration.cpp:93-100`)에 24800 포트 프로브와
  capability 응답을 얹어 실제 이음 서버만 남깁니다. LAN은 UDP 브로드캐스트 발견을 신규 구현합니다
  (현재 UDP 소켓이 전무하므로 `arch/` 소켓 계층 확장 필요). mDNS는 의존성이 무거워 1차 제외.
- **짧은 코드 페어링**: 서버가 6자리 코드를 표시하고 클라이언트가 입력하면, 그 코드에서 파생한
  값으로 지문을 상호 검증(SAS)한 뒤 `FingerprintDatabase::addTrusted()`로 기록합니다. 재사용은
  `FingerprintDatabase.cpp:13-91`, `SecureUtils.cpp:66-113`이고, 실패 시 기존 `FingerprintDialog`로
  폴백합니다. **`SecureSocket::verifyCertFingerprint`의 fail-closed 동작(`SecureSocket.cpp:619-657`)은
  유지하며 TOFU 자동 신뢰로 바꾸지 않습니다.**
- **경로 선택**: LAN 직결 → Tailscale → (구독 전용) 릴레이.
- **무중단 복구**: Phase 0의 클라이언트 네트워크 반응 위에 세션 유지를 얹습니다.

### Phase 2 — Carry (신규 구축, 단계적)

드래그 훅이 가장 비싸므로 **드래그 없이 먼저 출시**합니다.

- **2a. 전송 코어**: 프로토콜 1.11에 새 메시지 세트(start/chunk/end/ack/resume)를 추가합니다.
  폐기된 `DFTR`/`DDRG`는 재사용하지 않습니다. 상대 경로를 실어 폴더를 지원하고, SHA-256 무결성
  검사와 이어받기를 넣습니다. 재사용: `StreamChunker.cpp:14`(512KiB 청크),
  `ClipboardChunk.cpp:79-162`(조립·검증 패턴), `ClientProxy1_9.cpp`(59줄, 버전별 메시지 추가 템플릿).
  진입점은 트레이 드롭존과 핫키입니다.
- **2b. 한글 파일명 정규화**: 파일명에 NFC/NFD를 **양방향** 적용하고, 텍스트 정규화의 아웃바운드
  누락(`OSXClipboardAnyTextConverter.cpp:54-58`)도 함께 고칩니다. 재사용: `normalizeNfc()`.
- **2c. 클립보드 히스토리 + 초성 검색**: 기존 파이프라인 위에 얹되, 클라이언트 수신 상한이 서버
  협상값이 아니라 자기 로컬 설정에서 나오는 문제(`Client.cpp:63-65` vs `:366-370`)를 먼저 고칩니다.
- **2d. 플랫폼 드래그 훅**: Windows `IDropTarget`/`IDropSource`, macOS `NSPasteboard` 드래그,
  X11 XDND. 전부 신규이며 가장 마지막에 착수합니다.

### Phase 3 — 이후

- **Input Memory (앱별 한/영 기억)**: 착수 비용은 낮습니다. `MSWindowsImeController.cpp:80`이 이미
  `GetForegroundWindow()`를 호출하면서 PID를 `nullptr`로 버리고 있어 앱 식별은 ~10줄이고, macOS는
  `OSXInputSourceController.mm:94-97`의 기존 TIS 옵저버 옆에 `NSWorkspace` 알림을 추가하면 됩니다.
  **Linux는 IME 컨트롤러 자체가 없어 Windows·macOS 전용입니다.**
- **Spaces (장소별 프로필)**: `ServerConfig::commit()`/`recall()`의 `internalConfig` 그룹명을
  매개변수화하는 것이 가장 자연스러운 이음매입니다(`ServerConfig.cpp:76-139`). 주의: 새 최상위
  그룹은 `Settings::removeUnknownScreens()`(`Settings.cpp:428-436`)가 삭제하므로 `m_validGroup`에
  등록해야 합니다. 또한 GUI→코어 런타임 명령 채널이 없어(IPC는 코어→GUI 단방향) 프로필 전환은
  설정 파일 재작성 + 코어 재시작으로 구현됩니다.
- **Audio Follow (실험)**: 가상 오디오 드라이버가 필요합니다. Windows는 서명 드라이버, macOS는
  `AudioServerPlugIn`과 공증이 필요한데 **이 프로젝트는 아직 코드 서명조차 완료 전**입니다. 사실상
  별도 제품 규모이므로 Phase 1~2 완료 전에는 착수하지 않습니다.

---

## 4. 요금제

| 상품 | 형태 | 포함 | 서버 비용 |
| --- | --- | --- | --- |
| Community | 무료 | 로컬 KVM, IME 동기화, 클립보드, Phase 0 수리 전부 | 없음 |
| Ieum Plus 평생 | **일회성** | 서명·공증된 공식 빌드, 안정 업데이트 채널, 로컬 Carry(폴더·이어받기·정규화), 로컬 클립보드 히스토리, 기기 P2P 직결 | 거의 0 |
| Ieum Anywhere | **구독** | 계정 기반 기기 발견, 암호화 릴레이, 설정·히스토리 클라우드 동기화, 임시 게스트 연결 | 지속 발생 |
| Teams | 구독(사용자당) | 중앙 배포, SSO, 장치 정책, 감사 기록, 지원 | 지속 발생 |

일회성과 구독을 함께 제공하되 항목이 겹치지 않게 합니다. 평생 상품에 릴레이를 넣으면 매출은
1회인데 비용은 영구가 되므로, 위 표는 릴레이를 의도적으로 제외했습니다.

**정직하게 명시할 것**: Plus 평생에 포함된 로컬 기능은 GPL이므로 구매자가 소스를 받아 재배포할 수
있습니다. 이를 막으려 해서는 안 되고 막을 수도 없습니다. 실제 판매 대상은 *공식 서명 빌드와
업데이트·지원 번들*이며, 릴레이·동기화는 계정 없이 동작하지 않으므로 구독으로 방어됩니다.

## 5. 수용 기준

자동 테스트로 대체할 수 없는 항목은 실기로 확인하며, 결과가 나오기 전에는 유료 상품으로
광고하지 않습니다.

- Instant Mesh: Wi-Fi 전환·절전 복귀·Tailscale 재시작 각 20회에서 세션 유지, 잔류 modifier·버튼 0건
- Carry: 1GB 폴더 전송 중 네트워크 전환 후 이어받기 성공, 한글 파일명 자소 분리 0건
- 공통: 입력 지연 p95 증가 2ms 미만 (`phase0_report.md` 기준과 동일)

추가할 단위 테스트는 `ClipboardChunksTests.cpp`의 `MemoryStream` 페이크를 재사용합니다: `DFTR`
수신 후 후속 메시지 파싱(desync 회귀), 파일 청커 분할·조립·해시 불일치 거부·이어받기, 파일명
NFC/NFD 왕복, `fakeAllButtonsUp` 이후 버튼 상태, 클립보드 수신 상한의 협상값 반영.

## 6. 비목표

- GPL 코어 안에 결제 잠금을 넣지 않습니다.
- iPadOS 전체 시스템 KVM은 공개 API가 없어 약속하지 않습니다.
- DDC 모니터 입력 전환은 이번 로드맵에서 제외합니다 (Apple Silicon에 공개 API 없음).
- 연결 안정성, IME, 기본 클립보드, 보안 업데이트는 영구 무료로 유지합니다.

---

이 문서는 제품 방향 제안이며 법률 자문이 아닙니다. 실제 유료 배포 전에는 저작권·상표·서비스
경계를 별도로 검토해야 합니다.
