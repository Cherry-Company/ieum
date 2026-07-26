<!-- SPDX-FileCopyrightText: (C) 2026 Ieum Developers -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

<p align="center">
  <img src="artwork/ieum-icon-1024.png" alt="Ieum icon" width="156">
</p>

<h1 align="center">Ieum</h1>

<p align="center"><strong>跨 Windows、macOS 和 Linux 的软件 KVM</strong></p>

<p align="center">
  <a href="README.md">한국어</a> · <a href="README.en.md">English</a> · 简体中文
</p>

<p align="center">
  <a href="https://github.com/victoriousian/ieum/releases/tag/v0.1.0-alpha.6"><strong>下载 Ieum</strong></a>
  · <a href="#支持开发"><strong>支持开发</strong></a>
</p>

<p align="center">
  <a href="https://github.com/victoriousian/ieum/actions/workflows/continuous-integration.yml"><img src="https://github.com/victoriousian/ieum/actions/workflows/continuous-integration.yml/badge.svg?branch=ieum%2Fmain" alt="CI"></a>
  <a href="https://github.com/victoriousian/ieum/releases"><img src="https://img.shields.io/github/v/release/victoriousian/ieum?include_prereleases&label=release" alt="Release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--2.0--only-blue" alt="GPL-2.0-only"></a>
</p>

Ieum 让一套键盘和鼠标可以在 Windows、macOS 与 Linux 电脑之间切换。它不仅让指针跨越屏幕边缘，
还试图把不同系统中的 **韩/英输入状态、输入法组合会话、物理按键位置和 Unicode 剪贴板**连接成
一致的输入链路。

> 当前版本为 `v0.1.0-alpha.6`。自动构建和单元测试已经通过，但 Windows/macOS 真机长时间输入矩阵
> 与正式代码签名尚未完成。

## 产品名称与界面

韩语界面显示 **이음 (Ieum)**，并突出稳定的韩语与 CJK 输入；其他界面语言显示 **Ieum**，介绍语为
“跨 Windows、macOS 和 Linux 的软件 KVM”。为保持兼容性，可执行文件名与设置路径继续使用
`Ieum`。

主窗口现已采用系统调色板材质、服务端/客户端分段控件、清晰的功能层级和稳定的响应式尺寸。
Qt 实现参考了 Apple WWDC26 Liquid Glass 对层级、标准控件、克制使用效果、可调整窗口和辅助功能
的要求，而不是逐像素模仿 Apple 界面。实现规则与官方资料见
[界面设计原则](docs/design/visual-system.md)。

## 支持开发

**如果 이음 (Ieum) 对你有帮助，请考虑支持项目开发。** 资金将用于 Windows 代码签名、Apple
Developer Program 与公证、真机测试设备、中继基础设施和持续维护。

`victoriousian` 的 GitHub Sponsors 收款资料目前正在准备中。在资料激活之前，项目不会把未获批准的
付款链接或个人银行账户作为官方赞助渠道。激活后，可从这里和代码库的 Sponsor 按钮进行单次或定期
赞助。在此之前，Star、真机测试结果、使用反馈和可复现的错误报告同样能直接帮助项目。

## 韩语输入不只是按键映射

传统软件 KVM 通常把物理按键转换成字符标识，再在远端合成按键。这个模型适用于静态键盘布局，
但韩语、中文和日语文字由 **输入法状态机和组合输入会话（preedit）**生成。缺少这些状态时，按键
即使成功到达，也可能无法得到预期文字。

| 现象 | 结构性原因 |
| --- | --- |
| Windows 韩/英切换键不能切换 Mac 输入源 | 韩/英切换是模式命令而不是字符，却被当作普通按键传输 |
| 服务端指示状态与远端输入框状态不一致 | 缺少把客户端实际输入源回传给服务端的通道 |
| `한` 偶尔变成分离的韩文字母 | 输入源切换、混合注入路径或事件乱序中断了组合会话 |
| 从 macOS 复制的韩语在 Windows 中分离 | macOS 分解形式文本没有在传输前进行 NFC 规范化 |

Ieum 将这些问题视为协议级的 **输入状态同步问题**，而不是缺少某个特殊键映射。

## Ieum 的核心改动

### 1. 输入源控制通道

协议 1.9 增加 `DILC` 与 `CILS`：服务端请求切换输入源，客户端再回报操作系统实际选中的输入源。
客户端的真实状态而不是服务端猜测值，才是唯一可信来源。

### 2. 输入法原生按键链路

输入法启用时，Ieum 可以绕过字符 `KeyID` 翻译，以 PC Set-1 原始扫描码传输物理按键位置。文字
组合完全交给远端系统原生输入法，避免布局翻译干扰 CJK 输入。

### 3. 一致的 macOS 事件合成

输入法路径使用同一个持久 `CGEventSource` 和 FIFO 队列，并明确设置时间戳、键盘类型、修饰键和
重复状态，避免每个按键在不同事件注入层之间切换。

### 4. Unicode 剪贴板规范化

从 macOS 发出的 UTF-8 文本会转换为 NFC，减少 Windows 应用与文件名中的韩文音节拆分。

```mermaid
flowchart LR
    A["物理键盘"] --> B["Ieum Server"]
    B -->|"按键与扫描码"| C["加密 KVM 通道"]
    B -->|"DILC 输入源请求"| C
    C --> D["Ieum Client"]
    D --> E["Windows IMM32 / macOS TIS"]
    E -->|"CILS 实际状态"| D
    D -->|"状态回报"| B
```

## 项目状态

| 范围 | 状态 |
| --- | --- |
| Ieum 品牌、图标、应用与安装程序名称 | 已完成 |
| Windows x64/ARM64、macOS Intel/Apple Silicon 安装包 | CI 构建与打包检查通过 |
| `DILC`/`CILS`、原始扫描码、macOS 事件源、NFC 链路 | 已实现并包含自动测试 |
| Linux 与 Flatpak 安装包 | 实验性提供 |
| Windows ↔ macOS 十分钟真机输入矩阵 | **待完成** |
| Windows 签名、Apple 签名与公证 | **待完成** |
| iPadOS | 公共 API 不提供系统级输入注入，因此暂不支持 |

真机验收目标包括：连续切换输入源 20 次不失步、每个应用连续输入十分钟无韩文字母分离、输入
延迟增加低于 p95 2ms。在真机矩阵完成之前，本 Alpha 版本不会宣称已经达到这些结果。

## 下载

[Ieum v0.1.0-alpha.6 发布页](https://github.com/victoriousian/ieum/releases/tag/v0.1.0-alpha.6)

| 操作系统 | 安装文件 |
| --- | --- |
| Apple Silicon Mac | `Ieum-0.1.0-alpha.6-macos-arm64.dmg` |
| Intel Mac | `Ieum-0.1.0-alpha.6-macos-x86_64.dmg` |
| Intel/AMD 64 位 Windows | `Ieum-0.1.0-alpha.6-win-x64.msi` |
| Intel/AMD 64 位 Windows，韩文安装界面 | `Ieum-0.1.0-alpha.6-win-x64-ko-KR.msi` |
| ARM64 Windows | `Ieum-0.1.0-alpha.6-win-arm64.msi` |
| ARM64 Windows，韩文安装界面 | `Ieum-0.1.0-alpha.6-win-arm64-ko-KR.msi` |

发布页还提供 Windows 便携版与实验性 Linux 安装包。请使用随附的 `SHA256SUMS.txt` 校验文件。

`alpha.5` 的 Mac DMG 在签名后又修改了应用包，因此代码签名封印已损坏。请删除该版本并下载
`alpha.6`。`alpha.6` 最终应用已通过严格的代码签名验证，但尚未使用 Developer ID 证书签名，
也未经过 Apple 公证。请将应用移到 `/Applications` 并尝试打开一次；若 macOS 阻止运行，请前往
**系统设置 → 隐私与安全性 → 仍要打开**。应用还需要“辅助功能”和“输入监控”权限。

Windows `alpha.6` 使用 MSI 版本 `0.1.106`，可以替换 `alpha.4` 和 `alpha.5`。全局安装包在“已安装
的应用”和开始菜单中显示为 **Ieum**，韩文安装包显示为 **이음 (Ieum)**。安装目录为
`C:\Program Files\Ieum`，Windows 服务名为 `Ieum`，开始菜单中还会创建卸载快捷方式。Windows
安装包尚未进行代码签名，因此 SmartScreen 仍可能显示警告。

## 快速开始

1. 在所有需要控制的电脑上安装 Ieum。
2. 在连接物理键盘和鼠标的电脑上选择 `Server`。
3. 在其他电脑上选择 `Client`，并输入服务端地址。
4. 在服务端排列各屏幕位置，然后启动 Ieum。

输入法行为和各平台权限请参阅 [IME 使用说明](docs/user/ime.md)。

## 开源与付费产品方向

本地 KVM 核心和当前发行代码使用
`GPL-2.0-only WITH LicenseRef-OpenSSL-Exception`。Ieum 不会在同一个 GPL 可执行文件中把核心功能
改成闭源模块。

以下是**规划中的商业方向**，目前并不是已经销售的产品。

| 范围 | 开源/付费方向 |
| --- | --- |
| Community | 局域网 KVM、输入法同步、剪贴板和协议核心继续以 GPL 开源 |
| 官方发行 | 签名与公证安装包、稳定更新通道、简化安装和经过验证的二进制可以作为付费便利服务 |
| Teams/Cloud | 托管中继与设备发现、团队策略、SSO、审计记录和管理控制台可作为独立服务 |
| 技术支持 | 优先支持、部署协助、企业集成和维护合同可以收费 |

GPL 二进制可以收费发行，但接收者仍然有权获得对应源代码并重新分发。因此，可持续的付费价值应
来自 **官方可信发行、运维便利、托管服务和技术支持**，而不是限制源代码。任何专有组件都必须
保持清晰的独立进程或网络服务边界，并在发布前完成许可证审查。相关规则请参阅
[GNU GPL v2 FAQ](https://www.gnu.org/licenses/old-licenses/gpl-2.0-faq.en.html)。这只是项目方向而非法律
意见；正式商业发行前仍需单独审查版权、商标和服务边界。

## 路线图

- [x] 完成分支初始化与 Ieum 品牌替换
- [x] 输入源控制协议与各平台控制器
- [x] 输入法原始扫描码、macOS 事件链路和 NFC 规范化
- [x] Windows、macOS 与 Linux 自动打包
- [ ] 发布 Windows ↔ macOS 真机输入矩阵
- [ ] 代码签名、Apple 公证与稳定更新通道
- [ ] 定义托管中继与设备发现的产品边界
- [ ] 确定 `v1.0.0` 验收标准

## 构建与测试

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

更多信息请参阅[构建文档](docs/dev/build.md)、[协议参考](docs/dev/protocol_reference.md)和
[Phase 0 报告](phase0_report.md)。GitHub Actions 会验证 Windows x64/ARM64、macOS Intel/Apple
Silicon、多种 Linux 发行版、Flatpak 与 FreeBSD。

## 项目来源与许可证

Ieum 基于 `deskflow/deskflow` 的 `39bf4fb` 提交开发。为保持协议兼容性和已有 macOS 权限连续性，
部分内部二进制名称和 `org.deskflow.deskflow` 标识仍然保留。面向用户的产品名称、图标、安装程序和
发布版本均使用 Ieum。

项目按照 `GPL-2.0-only WITH LicenseRef-OpenSSL-Exception` 发布，并保留上游版权与许可证声明。
