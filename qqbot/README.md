# MSM QQ 机器人（NoneBot v11 + NapCat）

MSM 内置一套与 WebUI 完全解耦的本地 HTTP 控制通道（端口 `25585`，仅监听 `127.0.0.1`）。
**NapCat 版** 与 **NoneBot 版** 是逻辑完全相同的两个对称前端，二者任选其一或并存：

- **NapCat 版**（推荐，链路更短）：`napcat/` —— 装在 NapCat 里的插件，跳过 NoneBot，
  直接经 `25585` 控制 MC 并接收 MSM 的 WS 推送。见 `napcat/README.md`。
- **NoneBot 版**：`plugins/msm_control.py` —— 用 NoneBot 框架运行，NapCat 仅作
  OneBot 11 协议端把 QQ 消息转给 NoneBot。见下文。

> **设计说明**：控制通道不直接依赖 WebUI，因此**即使 WebUI 关闭，QQ 插件照样可用**。
> WebUI / NapCat / NoneBot 三者可独立开关，也能通过插件的指令互相启停。

---

## 指令一览

| 指令 | 说明 |
| --- | --- |
| `帮助` / `help` / `?` | 显示帮助 |
| `状态` / `status` | 所有服务器运行状态（在线人数 / 版本 / 类型） |
| `服务器` / `servers` | 列出全部服务器名 |
| `在线` / `list` / `players` `[名称]` | 在线玩家 |
| `控制台` / `console` `[名称] [行数]` | 控制台末尾输出 |
| `属性` / `props` `[名称]` | `server.properties` 配置 |
| `模组` / `mods` `[名称]` | 已安装模组列表 |
| `命令 <指令>` / `cmd <指令>` | 转发 MC 指令（白名单） |
| `启动` / `start` `[名称]` | 启动服务器 |
| `停止` / `stop` `[名称]` | 停止服务器 |
| `强关` / `forcestop` `[名称]` | 强制结束进程 |
| `删除` / `delete` `<名称>` | 删除服务器（需管理员） |
| `下载` / `downloads` | 当前下载任务 |
| `占用` / `usage` | 服务器资源占用（CPU / 内存 / 在线人数 / 运行时长） |
| `webui` / `napcat` / `nonebot` `on`/`off` | 互控三远端开关（需管理员） |

`[名称]` 可省略：省略时优先用 `msm_target_server`，其次自动选第一个运行中的，最后选第一个。

> **推送策略**：MSM 默认不主动推送。常态下仅按设定间隔推送“服务器占用”概览；
> 服务器异常退出时把日志私信到 `msm_admin`；其余只在你执行指令后回传反馈。
> 三者（WebUI / NapCat / NoneBot）互不影响，可任意组合独立开关。

---

## 1. 准备 MSM

- 打开 MSM 设置里的 **QQ 机器人**，分别独立开关 **NapCat** 与 **NoneBot**
  （两者互不影响，也与 WebUI 独立）。
- MSM 会按设置自动拉起 / 停止这两个外部进程，NapCat / qqbot 路径可自动探测或手动指定
  （默认探测程序目录旁的 `qqbot` 与 `NapCat`）。首次运行的 NapCat 会弹出独立控制台窗口用于扫码登录。
- **WebUI 可保持关闭**：QQ 插件走 MSM 内置的 25585 控制通道，不依赖 WebUI。
  仅当你想用浏览器管理时才需要打开 WebUI。

## 2. 配置 NapCat（反向 WebSocket）

- 协议端：**OneBot v11 → WebSocket 反向（Reverse WS）**
- 上报地址：`ws://127.0.0.1:8080/onebot/v11/`
  （`8080` 即下方 `pyproject.toml` 的 `port`）
- Access Token 留空或与 `onebot_access_token` 保持一致。

## 3. 安装与运行

```bash
cd qqbot
python -m venv .venv
.venv\Scripts\activate        # Windows
# source .venv/bin/activate   # Linux/macOS
pip install -r requirements.txt

# 按需修改 pyproject.toml 的 msm_* 配置项
python bot.py
# 或使用 nb-cli： nb run
```

## 4. 配置项（`pyproject.toml` 的 `[tool.nonebot]`）

| 配置项 | 默认 | 说明 |
| --- | --- | --- |
| `msm_control_url` | `http://127.0.0.1:25585` | MSM 内置控制通道地址（独立于 WebUI，WebUI 关闭时仍可用） |
| `msm_target_server` | 空 | 默认服务器名；留空自动选 |
| `msm_allowed_commands` | 见文件 | `命令` 转发白名单（小写，逗号分隔） |
| `msm_admins` | 空 | 允许使用敏感指令的 QQ 号；留空=不限制 |
| `msm_admin` | 空 | 管理员私信 QQ 号；服务器异常退出时日志私信到此号 |
| `msm_notify_targets` | 空 | 占用概览等“all”推送目标，格式 `group:群号,private:QQ号`，逗号分隔；留空则关闭推送 |
| `host` / `port` | `0.0.0.0:8080` | NoneBot 反向 WS 与 `/msm/notify` 监听地址（需与 NapCat 上报地址对应） |

## 5. 说明

- 仅 `命令` 受白名单限制；`启动` / `停止` / `强关` / `删除` / 互控开关 直接操作服务器或远端，
  请用 `msm_admins` 限制使用人群（尤其是 `删除` 会彻底移除服务器目录）。
- MSM 推送策略：默认不主动推送；仅按间隔推送“服务器占用”（由 MSM 设置里的
  `占用推送间隔(秒)` 控制，0=关闭），服务器异常退出时日志私信 `msm_admin`，
  其余只在指令执行后回传反馈。
- 跨机部署时，把 `msm_control_url` 改为 MSM 所在机器 IP（需 25585 可达），
  NoneBot 的 `host`/`port` 仍需与 NapCat 上报地址对应。
- 机器人通过 NapCat 接收 QQ 消息，与 MSM 程序解耦：MSM 升级不影响机器人逻辑。
