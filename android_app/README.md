# MSM 移动端 (Android / Jetpack Compose)

MSM（Minecraft Server Manager）的 Android 原生客户端，通过桌面端 **WebUI JSON API**
远程管理 Minecraft 服务器（手机不在本地起服务端）。

## 技术栈
- Kotlin + Jetpack Compose (Material 3)
- Retrofit2 + OkHttp3（自签 HTTPS 信任 + Bearer 鉴权）
- ML Kit Barcode Scanning（扫码 `msm://` 配对 URI）
- 协程 + ViewModel

## 目录
- `app/src/main/java/com/msm/app/data/` —— 模型、API 客户端、连接持久化
- `app/src/main/java/com/msm/app/ui/` —— Compose 页面（连接/配对、列表、详情、异常、扫码）

## 接入步骤
1. 用 Android Studio 打开 `android_app/` 目录（需联网下载 Gradle 8.9 + AGP 8.5.2 + SDK 34）。
2. `File → Sync Project with Gradle Files`。
3. 运行到设备或模拟器（minSdk 26）。

## 配对 / 连接
- **令牌模式**：手动填 host/port/token（桌面端 WebUI 设置中的 `webuiToken`）。
- **配对码模式**：桌面端「📱 移动端配对」生成一次性码（10 分钟有效），手机输入后
  `POST /api/pair` 换 token 自动连接；也可点「扫码」扫桌面端二维码。
- **URI 模式**：粘贴或扫码 `msm://token@host:port?t=...` 直接连接；App 已注册 `msm://` 深链。

## 注意
- 自签证书信任（`TrustAllDelegate` 等价实现）仅用于本机 LAN 管理面板，勿用于公网。
- 本工程在 Windows 环境编写，未经过 Android Studio 实机构建验证，首次 build 可能需微调 SDK 路径。
