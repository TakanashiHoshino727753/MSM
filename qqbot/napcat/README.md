# MSM NapCat 插件（napcat-plugin-msm）

把 QQ 消息桥接到 **MSM 内置控制通道（默认 `25585`）**，直接控制 Minecraft 服务器。
与 `../plugins/msm_control.py`（NoneBot 版）逻辑**完全相同**——两者都是「装在各自平台里、
直接打 MSM 接口、处理 QQ 消息」的对称前端，可任选其一或并存。

## 通信模型（单通道，与 WebUI / NoneBot 解耦）

```
QQ 消息 ──NapCat(本插件)──HTTP──> MSM 控制通道(127.0.0.1:25585)
MSM ──WebSocket(25585 /ws)──> 本插件 ──> QQ（占用概览 / 异常日志 / 指令反馈）
```

- 控制通道是 MSM 程序内置、独立于 WebUI（仅监听 `127.0.0.1`），**WebUI 关闭时仍可用**。
- WebUI / NapCat / NoneBot 三者可独立开关，也可通过 `webui`/`napcat`/`nonebot on|off` 互控。
- 推送策略：常态仅周期推送「服务器占用」；服务器异常退出时日志私信 `msm_admin`；
  其余只在执行指令后回传反馈。

## 编码说明（重要）

全程使用 JS 字符串（UTF-16）+ 全局 `fetch` / `WebSocket`（UTF-8）：

- QQ 的中文 / emoji 在 NapCat 侧已是标准 UTF-8 字符串，`raw_message` 直接可用。
- 发给 MSM 时 `JSON.stringify` 自动按 UTF-8 序列化；MSM 经 WS 推回的也是 UTF-8 文本帧。
- **无需任何手动编解码**，不会因 GBK / latin1 乱码。指令解析前仅剥离 `[CQ:...]` 码。

## 构建与安装

> 需要 Node.js ≥ 21（全局 `WebSocket` 在 Node 21+ 自带；NapCat 自带运行时通常满足）。

```bash
cd qqbot/napcat
pnpm install        # 或 npm install
pnpm run build      # 产物输出到 dist/（index.mjs + package.json）
```

部署（二选一）：

1. **手动**：把 `dist/` 整个目录复制到 NapCat 的插件目录
   （NapCat Shell 版路径为 `napcat.bat` 旁的 `napcat/plugins/napcat-plugin-msm/`，
   如 `NapCat4.16.0/napcat/plugins/napcat-plugin-msm/`），重启 NapCat 即在 WebUI 中可见。
2. **热部署**（推荐开发期）：先装并启用 `napcat-plugin-debug`，然后
   `pnpm run deploy` 自动复制 + 重载。

## 配置

在 NapCat WebUI 的插件配置面板填写（或编辑插件目录下的 `config.json`）：

| 配置项 | 默认 | 说明 |
| --- | --- | --- |
| `msm_control_url` | `http://127.0.0.1:25585` | MSM 内置控制通道地址 |
| `msm_target_server` | 空 | 默认服务器名；留空自动选运行中的 |
| `msm_allowed_commands` | 见上 | `命令` 转发白名单（小写，逗号分隔） |
| `msm_admins` | 空 | 允许使用敏感指令的 QQ 号；留空=不限制 |
| `msm_admin` | 空 | 管理员私信 QQ 号；异常退出日志私信到此号 |
| `msm_notify_targets` | 空 | 占用概览等「all」推送目标，格式 `group:群号,private:QQ号` |

## 指令一览

与 NoneBot 版一致：`帮助` `状态` `服务器` `在线` `控制台` `属性` `模组` `占用`
`命令 <指令>` `启动` `停止` `强关` `删除 <名称>` `下载` `webui|napcat|nonebot on|off`。

> 跨机部署：把 `msm_control_url` 改为 MSM 所在机器 IP（需 `25585` 可达）。
