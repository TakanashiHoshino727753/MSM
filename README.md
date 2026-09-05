# MSM - Minecraft Server Manager

Minecraft 服务器可视化管理工具（Qt 6 C++ / Windows）。提供本地桌面端 + 内嵌 WebUI 远程面板 + QQ 机器人联动 + 移动端远控，覆盖服务端生命周期管理、自动化运维与公网暴露。

---

## 项目结构

本仓库是**一个父项目 + 两个并列子项目（均在 `src/` 下）**的结构；两端均为 Qt 6 / C++ / QML：

```
MinecraftServerManager/        ← 父项目（CMake 聚合，不直接产出程序）
├── src/                       ← 两个端 + 共用源码统一目录
│   ├── desktop/               ← 子项目①：MSM 桌面端（Qt 6 / C++ / QML，服务端管理器）
│   ├── remote/                ← 子项目②：MSM 远控端（Qt 6 / C++ / QML，原 MSM_Remote）
│   └── shared/                ← 两端共用的功能模块（独立静态库）：统一日志 + 外观个性化（背景图/壁纸轮换/压暗）+ WebUI JSON API 契约
└── _legacy_remote_native/     ← 旧版原生 Android/iOS 远控端（已归档，备查）
```

- **桌面端 `src/desktop/`**：CMake 直接构建，产出主程序 `MinecraftServerManager`。
- **远控端 `src/remote/`**：Qt/C++ 客户端，经桌面端 WebUI JSON API（HTTPS）远程管理服务器，
  产出 `MSM_Remote`；旧版 Android/iOS 原生客户端已归档至仓库根 `_legacy_remote_native/`。
- **共用 `src/shared/`**：按功能拆为独立静态库模块，由两端各自链接（不再有单一 msm_shared 大库）：
  ① `logging/` —— 统一日志 `msm_logging`（Qt 宏 `QT_DEBUG` 区分 Debug/Release）；
  ② `appearance/` —— 外观个性化 `msm_appearance`（背景图/壁纸轮换/压暗/透明度，与桌面端壁纸逻辑共用同一套 QSettings 键）；
  ③ WebUI JSON API 契约见 `src/shared/README.md`。

构建桌面端：

```bash
cmake -S . -B build
cmake --build build --target MinecraftServerManager
```

---

## 功能清单

### 一、服务端核心管理

- **启动 / 停止 / 状态监控**：每台服务器独立进程管理，实时订阅控制台输出；崩溃自动重启（指数退避，可配置最大重试次数，EULA 未完成时不重试）。
- **配置可视化编辑**：服务端 `server.properties` 等核心配置通过界面可视化编辑，避免手工改文件出错。
- **一键更新核心**：检测并下载最新服务端核心（Paper 走 PaperMC API、Vanilla 走 Mojang launchermeta），替换前自动备份 `server.jar.bak`。
- **实时控制台视图**：内嵌日志区订阅 `consoleAppended`，支持即时查看与历史回溯。
- **资源监控**：每服展示 CPU 占用 / 内存 / TPS（解析 "Can't keep up!" 估算，60s 无过载回退满速 20）。

### 二、代理 / Velocity

- **代理崩溃自动拉起 + 退避重试**：`ProxyController` 区分用户停止与崩溃，崩溃后指数退避最多 N 次自动重启。
- **多代理支持 + 在线人数聚合**：`ProxyManager` 多实例（实例目录 `Velocity/instances/<id>`，设置键 `proxy/<id>/*`，`velocity.jar` 共享）；默认实例 = 索引 0，旧配置完全兼容；实例页签 + 后端「聚合」勾选 + 在线人数解析。
- **端口冲突自动检测**：代理 / 后端启动前检测监听端口占用，避免冲突导致启动失败。

### 三、后端自动化运维

- **定时备份 + 滚动保留**：`BackupController` 打包到 `AppData/MSM/backups`，按间隔自动备份 + 保留 N 份滚动删除；支持「启动时备份一次」与即时备份。
- **定时启停（cron 式）**：`SchedulerController` 每台服务器可配置 启动 / 停止 / 备份 + `HH:MM` 定时任务，每分钟检查。
- **公网暴露（实验）**：`PortMapper` 纯 Qt 实现 UPnP IGD（SSDP 发现 + SOAP `AddPortMapping`/`DeletePortMapping`/`GetExternalIPAddress`），无第三方 native 依赖；自动检测 CG-NAT 私网地址并提示改用 frp / ngrok。

### 四、Java 运行环境管理

- **按版本自动匹配 Java**：根据 Minecraft 版本返回所需 JDK 特性版本（8 / 17 / 21 …）。
- **自动安装 / 下载**：优先复用 PATH 上匹配版本；否则从 Adoptium（Temurin，TUNA 镜像）或 Oracle 公开直链下载并静默安装；Linux 等无安装器平台支持下载压缩包解压为可移植 JDK 并登记。
- **多架构适配**：自动识别主机 CPU 架构（x64 / aarch64 / arm / x86），避免在非 x86_64 机器下载到跑不起来的 JDK。
- **托管目录隔离**：Java 可随服务端核心安装到 `{服务器路径}/jvm`，自带、可单独删除；支持手动指定 `JAVA_HOME`、临时 JDK（用完即删，不留环境改动）。

### 五、下载中心

- **分类浏览与搜索**：Java / 服务端 / Modrinth 模组分类；支持关键词搜索、自定义保存目录。
- **统一下载管理**：并行下载、进度、暂停 / 继续 / 取消 / 重试、打开文件；多镜像源容错（国内 MCIM 优先，回退官方），90s 超时保护。
- **模组服（多加载器）打包**：选择 MC 版本 + 加载器（Forge / Fabric / NeoForge），下载并打包为可直接运行的服务端（复用「创建服务器」引擎，自动准备 Java + 运行安装器）。
- **Java 临时目录清理**：下载中心自动检测并清理未清理的临时 Java 目录。

### 六、服务端创建与整合包导入

- **创建服务器向导**：选择核心类型（Paper / Vanilla / Forge / Fabric / NeoForge 等）与版本，自动解析下载链接、准备 Java、运行安装器、写入 EULA 并加入服务器列表。
- **整合包导入**：导入 CurseForge / Modrinth 整合包 `.zip`，自动解压 → 识别类型 → 解析所需加载器与游戏版本 → 拉取服务端核心 → 写入 EULA 并加入列表；全程进度与状态可视化。

### 七、WebUI 远程控制面板

- **内嵌 SPA**：C++ 内嵌单页应用（编译进二进制），无需外部服务器；提供下载中心、服务器管理、代理、设置等远程操作界面。
- **服务端打包下载**：WebUI 支持选择 MC 版本 + 加载器，提交「下载并打包」任务，实时轮询多任务进度（含 Java 下载、安装器运行等分阶段进度）。
- **自签证书**：WebUI HTTPS 使用 Windows CryptoAPI 生成自签证书。

### 八、QQ 机器人联动（需 NapCat / NoneBot 环境）

- **管理指令**：`/白名单 add|remove`、`/广播`、`/在线`、`/重启` 等命令。
- **状态推送**：代理在线人数、各后端状态（CPU / 内存 / 平均 TPS / 运行数）推送群聊；服务器 / 代理启停、崩溃事件推送到群或私信管理员。

### 九、通知与告警

- **Webhook 通知**：`Notifier` 向 Discord / 企业微信 / 通用 Webhook POST JSON，覆盖崩溃 / 启停 / 玩家进服等事件，可独立开关。

### 十、易用性与界面

- **系统托盘常驻**：关闭窗口收起到托盘（仅当系统托盘可用时），单击切换显隐。
- **明暗主题切换**：设置页「深色模式」一键切换。
- **多语言**：界面支持简体中文 / English，状态栏实时重译。
- **诊断模式**：`--console / -c / -diag` 启动诊断，输出 TLS 后端检测、环境信息等。
- **自定义背景图 + 壁纸轮换**：设置页可指定任意图片作为所有窗口的底层背景（图片置于窗口圆角裁剪层之下，操作控件浮于其上并可通过透明度滑块调节），图片不显示时回退主题底色。壁纸支持「选择文件夹」（文件夹内多张图片）或「添加图片」手动收集；可在列表内用 ↑/↓ 直接调整播放顺序、✕ 删除；支持「顺序播放 / 随机播放」两种模式与切换间隔（分钟），列表≥2 张且已开启时自动轮换。
- **统一界面透明度控制**：设置页「界面透明度」滑块按等比例映射控制所有半透明控件（侧边栏背景、主区域底色、服务器卡片、页签条等）从各自最小值到完全不透明的过渡；标题栏保持不透明。背景图压暗遮罩强度可独立调节以保证文字可读性。
- **可拖动无边框窗口**：自绘标题栏 + 圆角窗口 + 边缘拖拽改大小；最大化时圆角归零。

---

## 环境

- Qt 6.11（MinGW-w64 13.10）
- CMake
- Windows 10+

> Linux 部署：核心管理、代理、下载、Java 多架构逻辑已具备 Linux 适配基础，但本工具主要面向 Windows 验证环境。

---

## 构建与部署

```powershell
# 临时加入工具链 PATH（按需调整 Qt 安装路径）
$env:PATH = "D:\Developer\Qt\Tools\CMake_64\bin;D:\Developer\Qt\Tools\mingw1310_64\bin;" + $env:PATH

# 配置并构建（未指定构建类型时默认 Release：开启编译器优化并关闭 DEBUG 控制台日志）
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MinecraftServerManager -j 8

# 部署（复制 exe + qml / i18n / qqbot 源码目录，并执行 windeployqt 部署 Qt 运行时与 QML 插件）
```

部署时需用 `windeployqt.exe MinecraftServerManager.exe --qmldir src/desktop/qml` 拉取 Qt DLL 与 QML 插件；HTTPS 依赖的 OpenSSL 3.x（`libcrypto-3-x64.dll` / `libssl-3-x64.dll`）需置于 exe 同目录。

---

## 未完成功能

- **远控端（远程控制客户端）**：现统一为 Qt/C++/QML 客户端（`src/remote/`，自 MSM_Remote 合并而来），
  经桌面端 WebUI JSON API 远程管理服务器：连接/配对、服务器列表与详情、启停与指令、控制台、
  异常纠错、看门狗、代理、优化模组、扫码接入。旧版 Android/iOS 原生客户端已归档至 `_legacy_remote_native/`。
  远控端自身功能已可在 Windows 上构建运行；配套反向连接隧道、端到端加密会话仍未实现。
- **远端二维码生成**：已实现。桌面端内置 `QrImageProvider`（C++ 用内嵌 qrcode 库渲染位图），
  QML 通过 `image://qr/<uri>` 显示；WebUI 侧通过 `/qrcode.js` + `/api/paircode` 展示配对二维码。

## 待修复 Bug

- ~~**窗口圆角丢失**：开启自定义背景图后，部分窗口四角变方。~~ **已修复（2026-09-05）**：改用 `Qt5Compat.GraphicalEffects` 的 `OpacityMask` 作为各窗口 `frame` 的 `layer.effect`，把整个窗口内容（含方角子项）统一按圆角矩形遮罩裁剪，并同步让 `BackgroundLayer` 的图片也经 `OpacityMask` 圆角化，彻底解决四角方角问题。
- ~~**WebUI 打不开**：内嵌 WebUI 远程面板无法访问（监听正常但页面无响应）。~~
  **已修复（2026-09-05）**：根因是构造函数创建的是普通 `QTcpServer`，而真正接管连接的
  `WebTcpServer`（重写 `incomingConnection`）因 `if (!m_server)` 判断**从未被创建**，
  于是所有连接都走 `newConnection` → 空的 `onNewConnection()`，请求无人处理。
  改为构造 `WebTcpServer`；同时修复 HTTPS 分支在 `encrypted()` 之后才连接 `readyRead`
  导致握手完成瞬间到达的请求被挂起的问题（改为握手前连接），并补充监听失败日志。
- ~~**远端二维码无法生成**：依赖 WebUI 运行状态，始终无法生成。~~
  **已修复（2026-09-05）**：该问题由「WebUI 打不开」连带导致（服务未运行时 `pairUri()` 无意义），
  WebUI 修复后配对二维码可正常生成；二维码渲染链路本身已完整实现。

## 更新日志（2026-08-29）

- 新增「自定义背景图」：所有窗口底层显示用户指定图片，图片不显示时回退主题底色。
- 新增「统一界面透明度控制」：设置页「界面透明度」滑块按等比例映射控制侧边栏背景、主区域底色、服务器卡片、页签条等半透明控件的透明度（标题栏保持不透明）；背景图压暗遮罩强度可独立调节。

## 更新日志（2026-09-05）

- **壁纸轮换**：背景图支持文件夹多图与手动添加；设置页列表内可 ↑/↓ 调整播放顺序、✕ 删除；支持「顺序播放 / 随机播放」与切换间隔（分钟），≥2 张且开启时自动轮换。底层由 `AppController` 定时器驱动，`SettingsController` 持久化播放列表/模式/间隔/顺序。
- **修复窗口圆角丢失**：采用 `Qt5Compat.GraphicalEffects` 的 `OpacityMask` 作为各窗口 `frame` 的 `layer.effect`，将窗口全部内容（含方角子项）按圆角矩形遮罩裁剪；`BackgroundLayer` 图片同样经 `OpacityMask` 圆角，彻底解决开启背景图后四角变方的问题。
- **项目结构重构**：拆为「父项目 + 两个并列子项目」——桌面端源码整体迁入 `desktop/`，
  远控端（`android_app/` + `ios_app/`）迁入 `remote/android`、`remote/ios`；
  新增根 `CMakeLists.txt` 聚合两端，桌面端产物与构建方式不变。
- **抽取共用契约 `shared/`**：两端唯一共用物是桌面端 WebUI JSON API，
  已在 `shared/README.md` 完整描述（鉴权、配对流程、`msm://` URI、全部端点与错误码约定）。
- **修复 WebUI 打不开**（详见「待修复 Bug」）：根因为服务器连接从未被接管处理。

## 更新日志（2026-09-05 · 架构再整合）

- **源码统一到 `src/`**：桌面端、远控端、共用模块分别归入 `src/desktop/`、`src/remote/`、`src/shared/`；
  旧版 Android/iOS 原生远控端归档至仓库根 `_legacy_remote_native/`。
- **远控端落地为 Qt 客户端**：`src/remote/` 即原 `MSM_Remote`（Qt/C++/QML），与桌面端共用
  `src/shared/msm_logging.cpp` 统一日志；壁纸相关能力（播放列表/轮换/压暗/透明度）已同步到远控端。
- **统一日志（Qt 宏）**：`src/shared/msm_logging.cpp` 用 `QT_DEBUG` 区分——Debug 构建回显控制台并写
  `*-debug.log`，Release 仅写常规 `logs/*.log`；桌面端 `msm.log`、远控端 `msm-remote.log`。

---

## License

MIT
