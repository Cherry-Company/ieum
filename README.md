<!-- SPDX-FileCopyrightText: (C) 2026 Ieum Developers -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

<p align="center">
  <img src="artwork/ieum-icon-1024.png" alt="Ieum icon" width="152">
</p>

# 이음 (Ieum)

**이음**은 여러 컴퓨터를 한 대의 키보드와 마우스로 제어하는 IME 네이티브 소프트 KVM입니다.
Windows와 macOS를 오갈 때 한/영 상태, 조합 중인 입력, 원시 스캔코드와 유니코드 클립보드를
일관되게 전달하는 데 초점을 둡니다.

## 다운로드

[Ieum v0.1.0-alpha.2 설치 파일](https://github.com/victoriousian/ieum/releases/tag/v0.1.0-alpha.2)

| 운영체제 | 파일 |
| --- | --- |
| Apple Silicon Mac | `Ieum-0.1.0-alpha.2-macos-arm64.dmg` |
| Intel Mac | `Ieum-0.1.0-alpha.2-macos-x86_64.dmg` |
| Intel/AMD 64비트 Windows | `Ieum-0.1.0-alpha.2-win-x64.msi` |
| ARM64 Windows | `Ieum-0.1.0-alpha.2-win-arm64.msi` |

Windows는 설치 없이 실행할 수 있는 `portable.7z` 패키지도 함께 제공합니다. 다운로드한 파일은
릴리스의 `SHA256SUMS.txt`로 검증할 수 있습니다.

현재 알파 패키지는 개발자 인증서로 서명하거나 Apple에 공증하지 않았습니다. macOS에서 차단되면
앱을 `/Applications`로 옮긴 뒤 다음 명령을 실행하고, 시스템 설정에서 접근성 및 입력 모니터링
권한을 허용하세요.

```sh
xattr -dr com.apple.quarantine /Applications/Ieum.app
```

Windows에서는 SmartScreen 경고가 표시될 수 있습니다.

## 주요 기능

- 프로토콜 1.9 `DILC`/`CILS` 채널을 통한 원격 입력 소스 제어 및 상태 확인
- Windows IMM32와 macOS TIS 기반 한/영 및 입력 소스 동기화
- IME 활성 중 KeyID 변환을 우회하는 PC Set-1 원시 스캔코드 경로
- macOS 합성 이벤트의 동일 `CGEventSource` 및 타임스탬프 유지
- macOS에서 공유하는 UTF-8 텍스트의 NFC 정규화
- 입력 상태 오버레이와 `Preferences > Advanced` 동기화 정책
- TLS 암호화, 화면 전환, 클립보드 공유와 기존 KVM 프로토콜 호환

## 빠른 시작

1. 제어할 컴퓨터 모두에 Ieum을 설치합니다.
2. 키보드와 마우스가 연결된 컴퓨터에서 `Server`를 선택합니다.
3. 나머지 컴퓨터에서 `Client`를 선택하고 서버 주소를 입력합니다.
4. 서버의 화면 배치에서 각 컴퓨터 위치를 지정한 뒤 시작합니다.

IME 동작과 운영체제별 권한은 [IME 사용 안내](docs/user/ime.md)를 확인하세요.

## 지원 범위

- Windows 10/11 x64 및 Windows 11 ARM64
- macOS 12 이상 Intel, macOS 14 이상 Apple Silicon
- Linux는 소스 빌드와 CI 패키지를 제공하지만 IME 동기화의 주 대상은 Windows와 macOS입니다.

iPadOS 설치 파일은 제공하지 않습니다. 소프트 KVM 클라이언트에는 다른 앱까지 제어하는 전역 입력
주입 권한이 필요하며, 일반 iPadOS 앱에는 해당 기능을 구현할 공개 API가 없습니다.

## 빌드와 테스트

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

자세한 환경별 절차는 [빌드 문서](docs/dev/build.md)를 참고하세요. GitHub Actions는 Windows x64/ARM64,
macOS Intel/Apple Silicon, Linux 배포판, Flatpak과 FreeBSD 빌드를 검증합니다.

## 계보와 라이선스

Ieum은 `deskflow/deskflow`의 `39bf4fb`에서 포크해 개발하고 있습니다. 프로토콜 호환성과 기존 macOS
권한 승계를 위해 일부 내부 바이너리명 및 `org.deskflow.deskflow` 식별자는 유지합니다. 사용자에게
표시되는 제품명, 앱 아이콘, 설치 프로그램과 릴리스는 Ieum으로 관리합니다.

코드는 `GPL-2.0-only WITH LicenseRef-OpenSSL-Exception`에 따라 배포합니다. 원저작자 고지와 라이선스
파일은 그대로 유지합니다.
