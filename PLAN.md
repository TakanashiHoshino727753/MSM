# MSM 功能扩展计划（本地计划表 / 上下文记录）

> 用途：本文件是「大任务」的本地事实来源（source of truth）。
> 跨会话 / 跨上下文时，先读本文件了解进度，再继续未完成项。
> 状态图例：⛔ 不做 │ ⬜ 未开始 │ 🔧 进行中 │ ✅ 已完成 │ ✔ 已存在(无需做)

最后更新：2026-07-29（里程碑 1 已完成并部署）

---

## 一、代理 / Velocity

| # | 功能 | 状态 | 说明 |
|---|------|------|------|
| P1 | 代理崩溃自动拉起 + 退避重试 | ✅ | ProxyController：区分用户停止 vs 崩溃；崩溃后指数退避最多 N 次自动重启 |
| P2 | 多代理支持 + 后端在线人数聚合 | ⬜ | 需 ProxyManager 列表模型；仪表盘聚合各代理在线人数 |
| P3 | 代理/后端 端口冲突自动检测 | ✅ | 代理 start 前检测监听端口（m_sc->isPortFree）；后端已有 isPortFree/portConflict 基建 |

## 二、后端运维

| # | 功能 | 状态 | 说明 |
|---|------|------|------|
| B1 | 服务器崩溃自动重启（带退避） | ✅ | ServerController：serverError 触发，指数退避，可配置最大重试/EULA 不重试；m_intentionalKill 不触发 |
| B2 | 一键更新后端 jar（版本检测+下载）| ⬜ | 需 Paper/Velocity API；仅 Vanilla/Paper 类后端 |
| B3 | 定时备份 + 滚动保留 | ⬜ | 启动前/按周期 zip 服务端目录，保留最近 N 份 |
| B4 | 定时启停（cron 式） | ⬜ | QTimer + 调度表；可配置多时段 |

## 三、自动化与告警

| # | 功能 | 状态 | 说明 |
|---|------|------|------|
| A1 | Webhook 通知（崩溃/启停/玩家进服）| ✅ | Notifier 类(msm_notify)：POST JSON 到 Discord/企业微信/通用；设置页配置 URL/类型/事件开关；main.cpp 接线 serverError/stateChanged/playerJoined/proxy crashed/runningChanged |
| A2 | 资源监控（每服 RAM/CPU/TPS）| ⬜ | 已有 runningServerUsages()；需 TPS 估算 + UI 展示 |
| A3 | UPnP / Cloudflare Tunnel / ngrok 公网暴露 | ⬜ | 避免直接暴露本机 IP（离线模式尤其重要）|

## 四、QQ 机器人扩展

| # | 功能 | 状态 | 说明 |
|---|------|------|------|
| Q1 | /白名单 add/remove、/广播、/在线、/重启 命令 | ⬜ | 扩展 msm_control.py 桥接 |
| Q2 | 代理在线人数、各后端状态推送 | ⬜ | 借助 Q1 的状态接口 |

## 五、易用性 / 界面

| # | 功能 | 状态 | 说明 |
|---|------|------|------|
| U1 | 系统托盘常驻 + 最小化到托盘 | ✔ 已存在 | AppController 已实现（单击切换、关闭隐藏到托盘）；无需改动 |
| U2 | 实时控制台视图（后端）| ⬜ | ServerPage 新增控制台标签页，订阅 consoleAppended + getConsole（待做）|
| U3 | 明暗主题切换 | ✔ 已存在 | 设置页「深色模式」开关已绑定 Theme.dark，无需改动 |

---

## 实施备注
- 主题机制：`AppController::setTheme(bool dark, QColor accent)` 已持久化到 QSettings(theme/dark, theme/accent)，切换写回 Theme 单例属性。
- 托盘：main.cpp 的 AppController 已含 QSystemTrayIcon；窗口 closeMode!="close" 时隐藏到托盘。
- 端口冲突：ServerController 已有 isPortFree/assignFreePort/portConflict；代理侧本次补上。
- 崩溃检测：ServerController 已有 serverError 信号 + m_intentionalKill；ProxyController 本次新增 m_expectedExit 区分。
- 构建：PATH=D:\Developer\Qt\Tools\CMake_64\bin;D:\Developer\Qt\Tools\mingw1310_64\bin；build 树在 D:\Projects\MinecraftServerManager\build；部署目标 D:\Users\Administrator\Desktop\test\MinecraftServerManager。

## 下一步
1. 完成 B2/B3/B4（备份/定时/更新 jar）
2. 完成 A2/A3（资源监控/公网暴露）
3. 完成 P2（多代理）
4. 完成 Q1/Q2（QQ 命令）
