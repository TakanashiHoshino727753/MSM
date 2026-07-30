# MSM 功能扩展计划（本地计划表 / 上下文记录）

> 用途：本文件是「大任务」的本地事实来源（source of truth）。
> 跨会话 / 跨上下文时，先读本文件了解进度，再继续未完成项。
> 状态图例：⛔ 不做 │ ⬜ 未开始 │ 🔧 进行中 │ ✅ 已完成 │ ✔ 已存在(无需做)

最后更新：2026-07-30（里程碑 2 完成并部署：B2/B3/B4 + A2 + Q1/Q2）

---

## 一、代理 / Velocity

| # | 功能 | 状态 | 说明 |
|---|------|------|------|
| P1 | 代理崩溃自动拉起 + 退避重试 | ✅ | ProxyController：区分用户停止 vs 崩溃；崩溃后指数退避最多 N 次自动重启 |
| P2 | 多代理支持 + 后端在线人数聚合 | ✅ | ProxyController 多实例化（instanceId/name/serverFilter/playerCount，实例目录 Velocity/instances/<id>，设置键 proxy/<id>/*，velocity.jar 共享）；新增 ProxyManager 列表模型（默认实例=索引0，旧配置完全兼容）；ProxyPage 实例页签 + 后端"聚合"勾选 + 在线人数（解析 [connected player] 日志） |
| P3 | 代理/后端 端口冲突自动检测 | ✅ | 代理 start 前检测监听端口（m_sc->isPortFree）；后端已有 isPortFree/portConflict 基建 |

## 二、后端运维

| # | 功能 | 状态 | 说明 |
|---|------|------|------|
| B1 | 服务器崩溃自动重启（带退避） | ✅ | ServerController：serverError 触发，指数退避，可配置最大重试/EULA 不重试；m_intentionalKill 不触发 |
| B2 | 一键更新后端 jar（版本检测+下载）| ✅ | UpdateController(msm_ops)：Paper(PaperMC API)/Vanilla(launchermeta) 版本检测+下载，替换前备份 server.jar.bak；ServerPage「更新核心」按钮→弹窗确认→更新 |
| B3 | 定时备份 + 滚动保留 | ✅ | BackupController(msm_ops)：tar.exe 打包到 AppData/MSM/backups，按间隔自动备份 + 保留 N 份滚动删除；可「启动时备份一次」；ServerPage「备份」按钮即时备份 |
| B4 | 定时启停（cron 式） | ✅ | SchedulerController(msm_ops)：每台服务器可配置 启动/停止/备份 + HH:MM 的定时任务；每分钟检查；ServerPage「定时任务」弹窗管理 |

## 三、自动化与告警

| # | 功能 | 状态 | 说明 |
|---|------|------|------|
| A1 | Webhook 通知（崩溃/启停/玩家进服）| ✅ | Notifier 类(msm_notify)：POST JSON 到 Discord/企业微信/通用；设置页配置 URL/类型/事件开关；main.cpp 接线 serverError/stateChanged/playerJoined/proxy crashed/runningChanged/更新完成 |
| A2 | 资源监控（每服 RAM/CPU/TPS）| ✅ | ServerPage 信息行展示 CPU%/内存/TPS；TPS 解析控制台 "Can't keep up! ... Running Nms behind" 估算，60s 无过载回退 20；昵称状态(Q2)也带平均 TPS |
| A3 | UPnP / Cloudflare Tunnel / ngrok 公网暴露 | ✅(实验) | PortMapper(src/ops)：纯 Qt 实现 UPnP IGD（SSDP M-SEARCH + 设备描述 XML + SOAP AddPortMapping/DeletePortMapping/GetExternalIPAddress），无第三方 native 依赖；ProxyPage "公网暴露"卡片：发现网关/映射代理端口/删除映射/外部 IP，检测 CG-NAT 私网地址并提示改用 frp/ngrok。本机无公网条件，未做端到端实测 |

## 四、QQ 机器人扩展

| # | 功能 | 状态 | 说明 |
|---|------|------|------|
| Q1 | /白名单 add/remove、/广播、/在线、/重启 命令 | ✅ | msm_control.py 新增 h_whitelist/h_broadcast/h_restart（白名单经 /api/command whitelist，广播经 say，重启先停后起）；在线/在线玩家已存在 |
| Q2 | 代理在线人数、各后端状态推送 | ✅ | botcontroller pushUsage 昵称已含 CPU/内存/平均 TPS/运行数；main.cpp 新增 server/proxy 启停→QQ 群推送，崩溃→私信管理员日志；msm_control.py 已能转发 scope 推送 |

## 五、易用性 / 界面

| # | 功能 | 状态 | 说明 |
|---|------|------|------|
| U1 | 系统托盘常驻 + 最小化到托盘 | ✔ 已存在 | AppController 已实现（单击切换、关闭隐藏到托盘）；无需改动 |
| U2 | 实时控制台视图（后端）| ✔ 已存在 | ServerPage 日志区已订阅 consoleAppended + getConsole（无需改动）|
| U3 | 明暗主题切换 | ✔ 已存在 | 设置页「深色模式」开关已绑定 Theme.dark，无需改动 |

---

## 实施备注
- 新增模块 `src/ops/`（msm_ops 静态库，链接 Qt6::Network + msm_server）：
  - `backupcontroller.{h,cpp}`：tar.exe 打包（Windows 10+ 自带，免第三方库）；备份目录 `QStandardPaths::AppDataLocation/backups`；滚动保留。
  - `schedulercontroller.{h,cpp}`：依赖 ServerManager/ServerController/BackupController；任务持久化于 `sched/tasks` JSON；动作 start/stop/backup。
  - `updatecontroller.{h,cpp}`：Qt6::Network 下载；Paper 取 builds 最新 build 拼 jar URL；Vanilla 查 version_manifest→version json→downloads.server.url。
- main.cpp 把三个控制器注册为 QML 上下文属性：`backupController / schedulerController / updateController`。
- 备份「启动即备份」会在应用启动时对每个受管服务器各打一份初始备份（用户需开启）。
- TPS 估算偏保守（满速 20，落后越多越低），仅作参考。
- QQ 状态推送需 VM(192.168.138.130) 上装有 NapCat/NoneBot 才能验证；本机无 bot 栈。
- 构建：PATH=D:\Developer\Qt\Tools\CMake_64\bin;D:\Developer\Qt\Tools\mingw1310_64\bin；build 树在 D:\Projects\MinecraftServerManager\build；部署目标 D:\Users\Administrator\Desktop\test\MinecraftServerManager。

## 下一步
1. 在 VM 上验证 QQ 状态推送/指令（Q1/Q2）。
2. 在有 UPnP 路由器的真实网络中实测 A3 端口映射（本机无公网条件）。
3. （可选）A3 补充 Cloudflare Tunnel / frp 子进程托管作为 CG-NAT 场景的替代方案。
