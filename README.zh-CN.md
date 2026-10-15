<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

<p align="center"><img src="artwork/ieum-icon-1024.png" alt="Ieum" width="112"></p>
<h1 align="center">多台电脑，一套键鼠。</h1>
<p align="center">用 Ieum 在 Windows、Mac 和 Linux 之间移动。</p>
<p align="center"><a href="https://github.com/Cherry-Company/ieum/releases"><strong>下载</strong></a> · <a href="#连接电脑">开始连接</a> · <a href="docs/dev/build.md">构建指南</a></p>
<p align="center"><a href="README.md">한국어</a> · <a href="README.en.md">English</a> · 简体中文</p>

## 鼠标移过去，接着工作。

将指针移过屏幕边缘，就能操作旁边的电脑。键盘输入也会一起切换。
在一台电脑上复制文字，在另一台上粘贴。每台电脑保留自己的屏幕和应用，Ieum 共享键鼠输入，不传输远程桌面画面。

| 想做什么 | Ieum 提供什么 |
| --- | --- |
| 用一套键鼠操作多台电脑 | 跨屏幕边缘共享输入 |
| 跨系统输入韩语和中日韩文字 | 输入语言同步与输入法处理 |
| 在电脑之间移动文字 | 剪贴板共享与 Unicode 规范化 |
| 连接不同网络的电脑 | 从 Tailscale 设备列表选择主机 |
| 在 Windows 与 Mac 之间传文件 | Pro Local 文件拖放传输 |

## 连接电脑

在所有电脑上安装 Ieum。连接着键盘和鼠标的电脑是**主机**，接收输入的电脑是**客户端**。
应用中的 **First connection** 可随时重新查看这些步骤及 Mac 权限说明。

1. 在主机上选择 **Share my keyboard and mouse**，点击 **Start**。
2. 在另一台电脑上选择 **Use a shared keyboard and mouse**。局域网连接时输入主机名称或 IP，
   Tailscale 连接时从列表选择主机。点击 **Connect**，核对并接受两台电脑上的指纹。
3. 在主机的 **Arrange computers** 中输入客户端的完整 **This device** 名称，选择方向，点击 **Add computer**。
   保存后，将鼠标移向对应的屏幕边缘即可。

使用 Tailscale 时，两台电脑都要登录同一个 tailnet，并在 Ieum 主界面选择 **Tailscale** 连接方式。
Tailscale 显示在线的电脑仍需在 Ieum 中启动主机共享。

要切换主机，在客户端的主机列表中选择另一台电脑即可。Ieum 会自动断开旧连接并建立新连接。
一个客户端同时只能连接一台主机。如果主机一直等待，请检查客户端选中的主机，尤其是它是否已经连接了别的电脑。

## 安装

从[发行页面](https://github.com/Cherry-Company/ieum/releases)选择对应的安装文件。

| 电脑 | 安装文件 |
| --- | --- |
| Windows 10/11，Intel 或 AMD | x64 MSI |
| Windows 10/11，ARM | ARM64 MSI |
| Apple Silicon Mac | macOS arm64 DMG；当前软件包面向 macOS 14 及以上 |
| Intel Mac | macOS x86_64 DMG；当前软件包面向 macOS 12 及以上 |
| Linux | 对应发行版的软件包；实验性支持 |

Mac 用户需将应用移到 Applications，并授予辅助功能权限。Windows 可能需要对应架构的 Visual C++ 运行库。
当前 Alpha 软件包尚未完成 Windows 代码签名和 Apple 签名、公证。
[详细安装与权限说明（韩语）](docs/user/installation.md)。

## 找到需要的设置

使用主界面的快捷入口，或在 **All settings** 中搜索。选择搜索结果即可跳转到对应设置。

| 要修改什么 | 位置 |
| --- | --- |
| 主机与连接方式 | 主界面 |
| 屏幕位置、切换方向、指针速度 | 主机的 **Arrange computers** |
| 单台电脑的名称、别名和按键映射 | **Arrange computers → Computer settings** |
| 输入语言与剪贴板文字 | **Input and clipboard** |
| 客户端滚动和重连方式 | 客户端的 **Receiving options** |
| 加密、证书、端口 | **Network settings → Connection & security** |
| 接收文件夹与 Pro Local 许可证 | **Files** |
| 登录时自动启动 | **All settings → General** |

键鼠共享、输入状态同步和剪贴板免费。Windows ↔ macOS 文件传输需要两台电脑都激活 Pro Local，并启用 TLS。
Pro Local Early Access 为一次性 USD 30，付款确认后通过邮件人工发放许可证。
[激活方法与功能范围](docs/dev/pro_local_file_transfer.md)。

## 无法连接时

- **主机一直等待：** 在主机启动共享，并在客户端选择这台主机。若客户端已连接其他主机，请在客户端切换。
- **Tailscale 显示在线却无法连接：** 在对方电脑上启动 Ieum 主机共享，并检查防火墙和 tailnet ACL 是否允许 TCP `24800`。
- **Mac 无法接收输入：** 检查 Applications 中当前 Ieum 应用的辅助功能权限。
- **报告问题：** 在 **Help → Report bug** 中查看诊断内容，点击 **Copy and open GitHub**，粘贴到 Issue 并描述当时的操作后提交。
  浏览器无法打开时，可使用显示的地址或保存的诊断文件。应用不会自动上传。

Ieum 仍处于 Alpha 阶段。实机稳定性、休眠恢复和操作系统重启仍需分平台测试。
[提交问题](https://github.com/Cherry-Company/ieum/issues)。

## 开发与支持

[构建测试](docs/dev/build.md) · [输入法指南](docs/user/ime.md) · [协议](docs/dev/protocol_reference.md) ·
[安全](docs/SECURITY.md) · [GitHub Sponsors](https://github.com/sponsors/victoriousian)

由 Cherry Company 维护。赞助用于签名、公证、实机测试与发行维护。
[赞助使用记录](docs/release/sponsorship-impact.md)。

Ieum 从 Deskflow `39bf4fb` 分叉，按 [GPL-2.0-only WITH LicenseRef-OpenSSL-Exception](LICENSE) 分发，并保留上游版权声明。
