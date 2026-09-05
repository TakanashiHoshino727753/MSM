# MSM 移动端 (iOS / SwiftUI)

本目录是 MSM（Minecraft Server Manager）的 iOS 原生客户端骨架（SwiftUI）。
它通过 MSM 桌面端的 **WebUI JSON API** 远程管理服务器（手机不在本地起服务端）。

## 文件结构
- `MSM/Models.swift` —— 数据模型（对应 WebUI 的 JSON 字段）
- `MSM/Network.swift` —— `MsmClient`（自签 HTTPS 信任 + Bearer 鉴权 + 配对端点）
- `MSM/AppViewModel.swift` —— 连接 / 配对 / 服务器 / 异常 的状态与动作
- `MSM/Views.swift` —— SwiftUI 页面（连接、列表、详情、异常纠错）
- `MSM/MSMApp.swift` —— 程序入口

## 接入步骤（在 macOS + Xcode 中）
1. 新建 iOS App 工程（Interface: SwiftUI，Language: Swift），Bundle ID 如 `com.msm.app`。
2. 把 `MSM/` 下的 .swift 文件拖入工程（勾选 target membership）。
3. `Info.plist` 添加：
   - `NSAppTransportSecurity` → `NSAllowsArbitraryLoads = YES`（允许自签 HTTPS / 明文 LAN）
   - `CFBundleURLTypes` → scheme `msm`（用于扫码深链 `msm://...`）
4. 需联网：`com.apple.security.network.client`（App Sandbox 若开启）。
5. 扫码：集成 `CodeScanner`（Swift Package）或 AVFoundation，扫描 `msm://` 二维码后调用
   `vm.connectWithUri(result)`。

## 配对流程
- 桌面端 WebUI「📱 移动端配对」页生成一次性配对码（10 分钟有效）。
- 手机端「配对码」模式输入码 → `POST /api/pair` 换取 `token` 后自动连接。
- 或直接粘贴 `msm://token@host:port?t=...` URI / 扫码连接。

## 注意
- 本骨架在 Windows 环境下编写，**未经过 Xcode 编译验证**，需在 Mac 上首次 build 微调。
- 自签证书信任（`TrustAllDelegate`）仅用于本机 LAN 管理面板，勿用于公网。
