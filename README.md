# MSM - Minecraft Server Manager

Minecraft 服务器可视化管理工具（Qt 6 C++ / Windows）。提供本地桌面端 + 内嵌 WebUI 远程面板 + QQ 机器人联动，覆盖服务端生命周期管理、自动化运维与公网暴露。

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

部署时需用 `windeployqt.exe MinecraftServerManager.exe --qmldir <源码 qml 目录>` 拉取 Qt DLL 与 QML 插件；HTTPS 依赖的 OpenSSL 3.x（`libcrypto-3-x64.dll` / `libssl-3-x64.dll`）需置于 exe 同目录。

---

## License

MIT
