"""msm_control —— 把 QQ 消息桥接到 MSM 的独立控制通道。

通信模型（与 WebUI 完全解耦）：
  QQ 消息 ──NapCat(反向WS)──> NoneBot(本插件) ──HTTP──> MSM 控制通道(127.0.0.1:25585)
  MSM ──WebSocket(25585 /ws)──> 本插件 ──> QQ（占用概览 / 异常日志 / 指令反馈）

- 控制通道端口为 MSM 内置、独立于 WebUI，因此即使 WebUI 关闭，QQ 插件照样可用。
- 三者（WebUI / NapCat / NoneBot）可独立开关，并可通过本插件的
  `webui/napcat/nonebot on|off` 指令互相启停。
- 推送策略：常态仅周期推送“服务器占用”；服务器异常退出时日志私信管理员；
  其余只在执行指令后回传反馈，不主动打扰。
"""
from nonebot import on_message, get_driver, get_bots
from nonebot.adapters.onebot.v11 import Bot, MessageEvent
from nonebot.log import logger
import httpx
import asyncio
import json
import websockets
import threading

driver = get_driver()
config = driver.config

# MSM 内置控制通道（独立于 WebUI）
CONTROL_URL = (getattr(config, "msm_control_url", "http://127.0.0.1:25585") or "http://127.0.0.1:25585").rstrip("/")
# 管理员私信 QQ 号：用于接收服务器异常退出日志
ADMIN = str(getattr(config, "msm_admin", "") or "").strip()
# 占用概览等“all”推送目标，格式 group:群号,private:QQ号
NOTIFY_TARGETS = []
for _item in (getattr(config, "msm_notify_targets", "") or "").split(","):
    _item = _item.strip()
    if not _item or ":" not in _item:
        continue
    _t, _id = _item.split(":", 1)
    NOTIFY_TARGETS.append((_t.strip().lower(), _id.strip()))
# 转发到 MC 的控制台指令白名单
ALLOWED = set(getattr(config, "msm_allowed_commands",
                      ["op", "deop", "gamemode", "tp", "give", "kick", "ban", "pardon",
                       "whitelist", "time", "weather", "say", "tell", "msg", "list", "stop"]) or [])
# 允许使用敏感指令的 QQ 号；留空=不限制
ADMINS = set(str(x).strip() for x in (getattr(config, "msm_admins", "") or []) if str(x).strip())


def _api(method: str, path: str, **kwargs):
    """请求 MSM 控制通道，返回解析后的 JSON（失败返回带 success=False 的字典）。"""
    try:
        r = httpx.request(method, CONTROL_URL + path, timeout=10, **kwargs)
        return r.json()
    except Exception as e:  # noqa: BLE001
        logger.warning("msm_control: API %s %s 失败: %s", method, path, e)
        return {"success": False, "message": f"连接 MSM 控制通道失败：{e}"}


def _server_names():
    return [s.get("name") for s in _api("GET", "/api/servers").get("servers", [])]


def _pick_name(rest: str):
    """若 rest 首词是已知服务器名则拆出，否则返回 ("", rest) 由 MSM 自动选择。"""
    toks = rest.split(None, 1)
    if toks and toks[0] in _server_names():
        return toks[0], (toks[1] if len(toks) > 1 else "")
    return "", rest


# ---------- 指令处理 ----------

HELP = (
    "【MSM 机器人】\n"
    "帮助/help/? 显示本帮助\n"
    "状态/status 三远端状态 + 运行服务器\n"
    "服务器/servers 列出全部服务器\n"
    "在线/list [名称] 在线玩家\n"
    "控制台/console [名称] [行数] 控制台末尾\n"
    "属性/props [名称] server.properties\n"
    "模组/mods [名称] 已装模组\n"
    "占用/usage 服务器资源占用\n"
    "命令/cmd <指令> 转发 MC 指令（受白名单限制，回传反馈）\n"
    "启动/start [名称] 启动服务器\n"
    "停止/stop [名称] 停止服务器\n"
    "强关/forcestop [名称] 强制结束进程\n"
    "删除/delete <名称> 删除服务器（仅管理员）\n"
    "下载/downloads 当前下载任务\n"
    "webui/napcat/nonebot on|off 互控三远端开关\n"
    "注：WebUI 关闭时本插件仍可用，三者独立。"
)

PRIVILEGED = {"命令", "启动", "停止", "强关", "删除", "webui", "napcat", "nonebot",
              "cmd", "start", "stop", "forcestop", "delete"}


async def _send(bot: Bot, event: MessageEvent, text: str):
    if not text:
        return
    for chunk in [text[i:i + 1000] for i in range(0, len(text), 1000)]:
        await bot.send(event, chunk)


async def h_help(bot, event, rest):
    await _send(bot, event, HELP)


async def h_status(bot, event, rest):
    data = _api("GET", "/api/status")
    if not data.get("success", True):
        await _send(bot, event, data.get("message", "查询失败"))
        return
    r = data.get("remotes", {})
    def fmt(k):
        v = r.get(k, {})
        en = "开" if v.get("enabled") else "关"
        st = v.get("state", "")
        if st and st != "stopped" and st != "running":
            en += f"({st})"
        return f"{k}:{en}"
    lines = [fmt("webui"), fmt("napcat"), fmt("nonebot")]
    lines.append(f"运行中服务器：{data.get('running', 0)} 台")
    names = data.get("servers", [])
    if names:
        lines.append("服务器：" + "、".join(names))
    await _send(bot, event, "\n".join(lines))


async def h_servers(bot, event, rest):
    data = _api("GET", "/api/servers")
    arr = data.get("servers", [])
    if not arr:
        await _send(bot, event, "暂无服务器")
        return
    lines = []
    for s in arr:
        state = "运行中" if s.get("running") else "已停止"
        pl = s.get("players", [])
        lines.append(f"{s.get('name')} [{state}] {'在线'+str(len(pl))+'人' if pl else ''}")
    await _send(bot, event, "\n".join(lines))


async def h_players(bot, event, rest):
    name, _ = _pick_name(rest)
    data = _api("GET", "/api/players" + (f"?name={name}" if name else ""))
    pl = data.get("players", [])
    if not pl:
        await _send(bot, event, "无在线玩家")
        return
    await _send(bot, event, "在线：" + "、".join(pl))


async def h_console(bot, event, rest):
    parts = rest.split()
    name = ""
    lines = 20
    if parts and parts[0] in _server_names():
        name = parts[0]
        if len(parts) > 1:
            lines = int(parts[1]) if parts[1].isdigit() else 20
    data = _api("GET", f"/api/console?name={name}&lines={lines}")
    c = data.get("console", "")
    await _send(bot, event, (c or "（空）"))


async def h_props(bot, event, rest):
    name, _ = _pick_name(rest)
    data = _api("GET", "/api/props" + (f"?name={name}" if name else ""))
    props = data.get("properties", {})
    if not props:
        await _send(bot, event, "（无属性）")
        return
    await _send(bot, event, "\n".join(f"{k}={v}" for k, v in props.items()))


async def h_mods(bot, event, rest):
    name, _ = _pick_name(rest)
    data = _api("GET", "/api/mods" + (f"?name={name}" if name else ""))
    mods = data.get("mods", [])
    if not mods:
        await _send(bot, event, "（无模组）")
        return
    await _send(bot, event, "模组：\n" + "\n".join(mods))


async def h_command(bot, event, rest):
    name, cmd = _pick_name(rest)
    if not cmd:
        await _send(bot, event, "用法：命令 <MC指令>")
        return
    head = cmd.split()[0].lower().lstrip("/")
    if ALLOWED and head not in ALLOWED:
        await _send(bot, event, f"指令 '{head}' 不在白名单内")
        return
    res = _api("POST", "/api/command", json={"name": name, "command": cmd})
    if not res.get("success"):
        await _send(bot, event, res.get("message", "发送失败"))
        return
    # 回传反馈：拉取控制台末尾作为指令输出
    fb = _api("GET", f"/api/console?name={res.get('name','')}&lines=8")
    await _send(bot, event, "已发送。最近输出：\n" + (fb.get("console", "") or "（空）"))


async def h_start(bot, event, rest):
    name, _ = _pick_name(rest)
    res = _api("POST", "/api/start", json={"name": name})
    await _send(bot, event, res.get("message") or ("已请求启动 " + res.get("name", name)))


async def h_stop(bot, event, rest):
    name, _ = _pick_name(rest)
    res = _api("POST", "/api/stop", json={"name": name})
    await _send(bot, event, res.get("message") or ("已请求停止 " + res.get("name", name)))


async def h_forcestop(bot, event, rest):
    name, _ = _pick_name(rest)
    res = _api("POST", "/api/forcestop", json={"name": name})
    await _send(bot, event, res.get("message") or ("已强制结束 " + res.get("name", name)))


async def h_delete(bot, event, rest):
    name = rest.strip()
    if not name:
        await _send(bot, event, "用法：删除 <名称>")
        return
    res = _api("POST", "/api/delete", json={"name": name})
    await _send(bot, event, res.get("message") or ("已删除 " + name))


async def h_downloads(bot, event, rest):
    data = _api("GET", "/api/downloads")
    arr = data.get("downloads", [])
    if not arr:
        await _send(bot, event, "无下载任务")
        return
    lines = []
    for d in arr:
        lines.append(f"{d.get('name','?')} {d.get('state','?')} {d.get('percent',0)}%")
    await _send(bot, event, "下载：\n" + "\n".join(lines))


async def h_usage(bot, event, rest):
    data = _api("GET", "/api/usage")
    arr = data.get("servers", [])
    if not arr:
        await _send(bot, event, "当前无运行中的服务器")
        return
    lines = []
    for s in arr:
        lines.append(f"{s.get('name')}：在线{s.get('players',0)}人，内存{round(s.get('memMB',0))}MB，"
                     f"CPU{round(s.get('cpu',0),1)}%，已运行{int(s.get('uptimeSec',0))//60}分钟")
    await _send(bot, event, "服务器占用：\n" + "\n".join(lines))


_ON = {"on", "开", "启用", "开启", "true", "1", "start"}
_OFF = {"off", "关", "关闭", "停用", "off", "false", "0", "stop"}


async def h_control(bot, event, rest):
    parts = rest.split()
    if len(parts) < 2:
        await _send(bot, event, "用法：webui|napcat|nonebot on|off")
        return
    target, arg = parts[0].lower(), parts[1].lower()
    if target not in ("webui", "napcat", "nonebot"):
        await _send(bot, event, "未知目标，仅支持 webui/napcat/nonebot")
        return
    if arg in _ON:
        en = True
    elif arg in _OFF:
        en = False
    else:
        await _send(bot, event, "请使用 on 或 off")
        return
    res = _api("POST", "/api/control", json={"target": target, "enabled": en})
    if not res.get("success"):
        await _send(bot, event, res.get("message", "操作失败"))
        return
    await _send(bot, event, f"{target} 已{'开启' if en else '关闭'}")


ALIAS = {
    "帮助": h_help, "help": h_help, "?": h_help,
    "状态": h_status, "status": h_status,
    "服务器": h_servers, "servers": h_servers,
    "在线": h_players, "list": h_players,
    "控制台": h_console, "console": h_console,
    "属性": h_props, "props": h_props,
    "模组": h_mods, "mods": h_mods,
    "占用": h_usage, "usage": h_usage,
    "命令": h_command, "cmd": h_command,
    "启动": h_start, "start": h_start,
    "停止": h_stop, "stop": h_stop,
    "强关": h_forcestop, "forcestop": h_forcestop,
    "删除": h_delete, "delete": h_delete,
    "下载": h_downloads, "downloads": h_downloads,
    "webui": h_control, "napcat": h_control, "nonebot": h_control,
}


_nb_loop = None  # 捕获 nonebot 主事件循环，供后台 WS 线程回传 QQ 消息

_matcher = on_message()


async def _dispatch(bot: Bot, event: MessageEvent):
    global _nb_loop
    _nb_loop = asyncio.get_running_loop()
    text = event.get_plaintext().strip()
    if not text:
        return
    parts = text.split(None, 1)
    cmd = parts[0].lower()
    rest = parts[1].strip() if len(parts) > 1 else ""
    handler = ALIAS.get(cmd)
    if not handler:
        return
    if cmd in PRIVILEGED and ADMINS and str(event.get_user_id()) not in ADMINS:
        await _send(bot, event, "权限不足：该指令仅管理员可用")
        return
    await handler(bot, event, rest)


_matcher.handle(_dispatch)


# ---------- MSM -> QQ 推送通道 ----------

async def _send_msg(bot, target_type, target_id, message):
    try:
        if target_type == "group":
            await bot.send_msg(message_type="group", group_id=int(target_id), message=message)
        else:
            await bot.send_msg(message_type="private", user_id=int(target_id), message=message)
    except Exception as e:  # noqa: BLE001
        logger.warning("msm_control: 推送发送失败 %s->%s: %s", target_type, target_id, e)


async def _dispatch_push(data):
    """处理 MSM 经由 WebSocket 推来的消息，转发到 QQ。

    本函数在后台线程的事件循环内运行，故发送 QQ 需借助 run_coroutine_threadsafe
    调度到 nonebot 主事件循环（_nb_loop，由首个 QQ 消息到来时捕获）。
    """
    message = (data.get("message") or "").strip()
    if not message:
        return
    scope = (data.get("scope") or "all").lower()
    targets = data.get("targets") or []
    if scope == "admin":
        if not ADMIN:
            return
        targets = [("private", ADMIN)]
    elif not targets:
        targets = NOTIFY_TARGETS
    if not targets:
        return
    bots = get_bots()
    if not bots:
        logger.warning("msm_control: 推送时无可用 Bot，跳过: %s", message[:40])
        return
    bot = next(iter(bots.values()))
    if _nb_loop is None:
        logger.warning("msm_control: 尚未捕获 nonebot 事件循环，跳过推送: %s", message[:40])
        return
    for target_type, target_id in targets:
        try:
            asyncio.run_coroutine_threadsafe(
                _send_msg(bot, target_type, target_id, message), _nb_loop
            )
        except Exception as e:  # noqa: BLE001
            logger.warning("msm_control: 推送调度失败 %s->%s: %s", target_type, target_id, e)


async def _ws_loop():
    """经 25585 的 /ws 长连接接收 MSM 推送，断线自动重连。"""
    base = CONTROL_URL.rstrip("/")
    if base.startswith("https://"):
        ws_url = "wss://" + base[8:] + "/ws"
    elif base.startswith("http://"):
        ws_url = "ws://" + base[7:] + "/ws"
    else:
        ws_url = "ws://" + base + "/ws"
    logger.info("msm_control: 连接 MSM 控制通道(WS) %s", ws_url)
    while True:
        try:
            # 注意：MSM 的 WS 服务端不一定响应客户端 ping，若启用 ping 会触发客户端
            # 自己超时断线。这里关闭客户端 ping，改由 _heartbeat() 的 /api/ping（HTTP）
            # 维持控制通道"运行中"状态，断线由下方 except 统一重连。
            async with websockets.connect(ws_url, ping_interval=None, ping_timeout=None) as ws:
                logger.info("msm_control: WS 已连入 MSM，开始接收推送")
                async for raw in ws:
                    try:
                        await _dispatch_push(json.loads(raw))
                    except Exception as e:  # noqa: BLE001
                        logger.warning("msm_control: 推送解析失败: %s", e)
        except Exception as e:  # noqa: BLE001
            logger.warning(f"msm_control: WS 断开({e})，5s 后重连")
            await asyncio.sleep(5)


# ---------- 心跳保活：启动即通知 MSM 进入"运行中"，并周期 ping 防回退 ----------
# 注意：本环境 nonebot 2.5.0 的 @driver.on_startup 不会触发，因此改用独立后台线程
# 自带事件循环运行 WS 与心跳，脱离 nonebot 的启动生命周期，确保可靠连入 25585。

async def _heartbeat():
    # 启动后立即 ping 一次，让 MSM 控制通道立刻从"等待连接"变为"运行中"
    try:
        await asyncio.to_thread(_api, "GET", "/api/ping")
    except Exception:  # noqa: BLE001
        pass
    while True:
        await asyncio.sleep(15)
        try:
            await asyncio.to_thread(_api, "GET", "/api/ping")
        except Exception:  # noqa: BLE001
            pass


def _background_loop():
    """在独立守护线程中运行 WS 与心跳，使用自带事件循环。"""
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.create_task(_ws_loop())
    loop.create_task(_heartbeat())
    try:
        loop.run_forever()
    finally:
        loop.close()


# 模块导入即启动后台连接线程（守护线程，进程退出自动回收）
_bg_thread = threading.Thread(target=_background_loop, name="msm-control-ws", daemon=True)
_bg_thread.start()
logger.info("msm_control: 后台控制通道线程已启动（脱离 on_startup 生命周期）")
