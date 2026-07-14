<!-- SPDX-FileCopyrightText: (C) 2026 Ieum Developers -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Ieum

**이음(Ieum)**은 Deskflow를 기반으로 한 IME 네이티브 소프트 KVM입니다. 한 대의 키보드와 마우스로
Windows, macOS, Linux 컴퓨터를 제어하면서 Windows와 macOS 사이의 한/영 상태, 한글 조합,
클립보드 유니코드 정규화를 일관되게 유지하는 것을 목표로 합니다.

## 핵심 기능

- 프로토콜 1.9 `DILC`/`CILS` 채널로 원격 운영체제의 실제 입력 소스를 제어하고 확인
- Windows 서버가 원격 제어 중 한/영·한자 키를 로컬 IME에 적용하지 않도록 차단
- macOS TIS 입력 소스 선택과 변경 알림, Windows IMM32 상태 판독 및 `SendInput` 토글
- IME 활성 시 KeyID 번역을 우회하는 PC Set-1 원시 스캔코드 경로
- macOS 합성 키를 영속 `CGEventSource`와 단조 타임스탬프로 단일 주입
- macOS에서 공유되는 UTF-8 텍스트를 NFC로 정규화
- 입력 상태 트레이 표시와 `Preferences > Advanced` 설정

## 빌드

업스트림과 동일한 CMake/Qt 6 빌드 절차를 사용합니다. 자세한 환경별 명령은
[빌드 문서](docs/dev/build.md)를 참고하세요.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

macOS 클라이언트에서는 접근성 및 입력 모니터링 권한이 필요합니다. 한글 입력 설정과 호환성 옵션은
[IME 사용 안내](docs/user/ime.md)에 정리되어 있습니다.

## 기반과 라이선스

이 저장소는 `deskflow/deskflow`의 `39bf4fb`에서 포크되었습니다. 코드는
`GPL-2.0-only WITH LicenseRef-OpenSSL-Exception`에 따라 배포됩니다. 원본 저작권 고지와
라이선스 파일은 그대로 유지합니다.
