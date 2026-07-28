// =============================================================================
// napcat-plugin-msm —— NapCat 版 MSM 控制插件（与 NoneBot 版 msm_control.py 逻辑相同）
// 本文件由 src/index.ts + src/config.ts 手工转译（去类型、合并单文件），
// 若安装了 Node/pnpm，也可在 qqbot/napcat/ 下 `pnpm run build` 重新生成。
//
// 通信模型（单通道，与 WebUI / NoneBot 解耦）：
//   QQ 消息 ──NapCat(本插件)──HTTP──> MSM 控制通道(127.0.0.1:25585)
//   MSM ──WebSocket(25585 /ws)──> 本插件 ──> QQ（占用概览 / 异常日志 / 指令反馈）
//
// 编码说明：全程 JS 字符串(UTF-16) + fetch/WebSocket(UTF-8)，无需手动编解码。
// =============================================================================

import * as fs from "node:fs";
import { fileURLToPath } from "node:url";

export const plugin_config_ui = [
  {
    key: "msm_control_url",
    label: "MSM 控制通道地址",
    type: "string",
    default: "http://127.0.0.1:25585",
    placeholder: "http://127.0.0.1:25585",
    description: "MSM 内置控制通道(25585)，独立于 WebUI，WebUI 关闭时仍可用",
  },
  {
    key: "msm_target_server",
    label: "默认服务器名",
    type: "string",
    default: "",
    placeholder: "留空=自动选运行中的",
    description: "省略服务器名时默认操作的服务器",
  },
  {
    key: "msm_allowed_commands",
    label: "指令白名单",
    type: "string",
    default:
      "op,deop,gamemode,tp,give,kick,ban,pardon,whitelist,time,weather,say,tell,msg,list,stop",
    description: "命令 转发允许的 MC 指令(小写,逗号分隔)",
  },
  {
    key: "msm_admins",
    label: "管理员 QQ",
    type: "string",
    default: "",
    placeholder: "123456,654321",
    description: "允许使用敏感指令的 QQ 号；留空=不限制",
  },
  {
    key: "msm_admin",
    label: "异常日志接收号",
    type: "string",
    default: "",
    placeholder: "123456",
    description: "服务器异常退出时日志私信到此 QQ",
  },
  {
    key: "msm_notify_targets",
    label: "占用推送目标",
    type: "string",
    default: "",
    placeholder: "group:群号,private:QQ号",
    description: "占用概览等 all 推送目标(逗号分隔)；留空关闭推送",
  },
];

const DEFAULT_CFG = {
  msm_control_url: "http://127.0.0.1:25585",
  msm_target_server: "",
  msm_allowed_commands:
    "op,deop,gamemode,tp,give,kick,ban,pardon,whitelist,time,weather,say,tell,msg,list,stop",
  msm_admins: "",
  msm_admin: "",
  msm_notify_targets: "",
};

let cfg = { ...DEFAULT_CFG };
let ctxRef = null;
let wsRef = null;
let wsTimer = null;

function log(level, ...args) {
  const msg = `[msm] ${args.map(String).join(" ")}`;
  if (ctxRef && ctxRef.logger && ctxRef.logger.log) ctxRef.logger.log(msg);
  else console.log(msg);
}

// ---------- MSM HTTP 控制通道（UTF-8，全程字符串） ----------
async function api(method, path, body) {
  const url = cfg.msm_control_url.replace(/\/+$/, "") + path;
  try {
    const init = {
      method,
      headers: { "Content-Type": "application/json" },
    };
    if (body !== undefined) init.body = JSON.stringify(body);
    const resp = await fetch(url, init);
    return await resp.json();
  } catch (e) {
    const detail = e && e.message ? e.message : String(e);
    log("warn", `API ${method} ${path} 失败: ${detail}`);
    return { success: false, message: `连接 MSM 控制通道失败：${detail}` };
  }
}

async function serverNames() {
  const d = await api("GET", "/api/servers");
  return (d.servers || []).map((s) => s.name);
}

async function pickName(rest) {
  const names = await serverNames();
  const toks = rest.split(/\s+/).filter(Boolean);
  if (toks.length && names.includes(toks[0])) {
    return [toks[0], toks.slice(1).join(" ")];
  }
  return ["", rest];
}

// ---------- 回复 QQ ----------
async function send(ctx, event, text) {
  if (!text) return;
  const chunks = text.match(/[\s\S]{1,1000}/g) || [text];
  for (const c of chunks) {
    const params = { message: c, message_type: event.message_type };
    if (event.message_type === "group") params.group_id = String(event.group_id);
    else params.user_id = String(event.user_id);
    try {
      await ctx.actions.call("send_msg", params, ctx.adapterName, ctx.pluginManager.config);
    } catch (e) {
      log("error", `发送失败: ${e && e.message ? e.message : e}`);
    }
  }
}

// ---------- 指令处理 ----------
const HELP =
  "【MSM 机器人】\n" +
  "帮助/help/? 显示本帮助\n" +
  "状态/status 三远端状态 + 运行服务器\n" +
  "服务器/servers 列出全部服务器\n" +
  "在线/list [名称] 在线玩家\n" +
  "控制台/console [名称] [行数] 控制台末尾\n" +
  "属性/props [名称] server.properties\n" +
  "模组/mods [名称] 已装模组\n" +
  "占用/usage 服务器资源占用\n" +
  "命令/cmd <指令> 转发 MC 指令(受白名单限制)\n" +
  "启动/start [名称] 启动服务器\n" +
  "停止/stop [名称] 停止服务器\n" +
  "强关/forcestop [名称] 强制结束进程\n" +
  "删除/delete <名称> 删除服务器(仅管理员)\n" +
  "下载/downloads 当前下载任务\n" +
  "webui/napcat/nonebot on|off 互控三远端开关\n" +
  "注：WebUI 关闭时本插件仍可用，三者独立。";

const PRIVILEGED = new Set([
  "命令", "启动", "停止", "强关", "删除", "webui", "napcat", "nonebot",
  "cmd", "start", "stop", "forcestop", "delete",
]);

async function h_help(ctx, event) {
  await send(ctx, event, HELP);
}

async function h_status(ctx, event) {
  const data = await api("GET", "/api/status");
  if (!data || data.success === false) {
    await send(ctx, event, (data && data.message) || "查询失败");
    return;
  }
  const r = data.remotes || {};
  const fmt = (k) => {
    const v = r[k] || {};
    let en = v.enabled ? "开" : "关";
    const st = v.state || "";
    if (st && st !== "stopped" && st !== "running") en += `(${st})`;
    return `${k}:${en}`;
  };
  const lines = [fmt("webui"), fmt("napcat"), fmt("nonebot")];
  lines.push(`运行中服务器：${data.running || 0} 台`);
  const names = data.servers || [];
  if (names.length) lines.push("服务器：" + names.join("、"));
  await send(ctx, event, lines.join("\n"));
}

async function h_servers(ctx, event) {
  const data = await api("GET", "/api/servers");
  const arr = data.servers || [];
  if (!arr.length) {
    await send(ctx, event, "暂无服务器");
    return;
  }
  const lines = arr.map((s) => {
    const state = s.running ? "运行中" : "已停止";
    const pl = s.players || [];
    return `${s.name} [${state}]${pl.length ? " 在线" + pl.length + "人" : ""}`;
  });
  await send(ctx, event, lines.join("\n"));
}

async function h_players(ctx, event, rest) {
  const [name] = await pickName(rest);
  const data = await api("GET", "/api/players" + (name ? `?name=${name}` : ""));
  const pl = data.players || [];
  await send(ctx, event, pl.length ? "在线：" + pl.join("、") : "无在线玩家");
}

async function h_console(ctx, event, rest) {
  const parts = rest.split(/\s+/).filter(Boolean);
  let name = "";
  let lines = 20;
  const names = await serverNames();
  if (parts.length && names.includes(parts[0])) {
    name = parts[0];
    if (parts[1] && /^\d+$/.test(parts[1])) lines = parseInt(parts[1], 10);
  }
  const data = await api("GET", `/api/console?name=${name}&lines=${lines}`);
  await send(ctx, event, data.console || "（空）");
}

async function h_props(ctx, event, rest) {
  const [name] = await pickName(rest);
  const data = await api("GET", "/api/props" + (name ? `?name=${name}` : ""));
  const props = data.properties || {};
  const keys = Object.keys(props);
  await send(ctx, event, keys.length ? keys.map((k) => `${k}=${props[k]}`).join("\n") : "（无属性）");
}

async function h_mods(ctx, event, rest) {
  const [name] = await pickName(rest);
  const data = await api("GET", "/api/mods" + (name ? `?name=${name}` : ""));
  const mods = data.mods || [];
  await send(ctx, event, mods.length ? "模组：\n" + mods.join("\n") : "（无模组）");
}

async function h_usage(ctx, event) {
  const data = await api("GET", "/api/usage");
  const arr = data.servers || [];
  if (!arr.length) {
    await send(ctx, event, "当前无运行中的服务器");
    return;
  }
  const lines = arr.map((s) =>
    `${s.name}：在线${s.players || 0}人,内存${Math.round(s.memMB || 0)}MB,` +
    `CPU${Math.round((s.cpu || 0) * 10) / 10}%,已运行${Math.floor((s.uptimeSec || 0) / 60)}分钟`
  );
  await send(ctx, event, "服务器占用：\n" + lines.join("\n"));
}

async function h_command(ctx, event, rest) {
  const [name, cmd] = await pickName(rest);
  if (!cmd) {
    await send(ctx, event, "用法：命令 <MC指令>");
    return;
  }
  const head = cmd.split(/\s+/)[0].toLowerCase().replace(/^\//, "");
  const allowed = cfg.msm_allowed_commands.split(",").map((s) => s.trim().toLowerCase()).filter(Boolean);
  if (allowed.length && !allowed.includes(head)) {
    await send(ctx, event, `指令 '${head}' 不在白名单内`);
    return;
  }
  const res = await api("POST", "/api/command", { name, command: cmd });
  if (!res || !res.success) {
    await send(ctx, event, (res && res.message) || "发送失败");
    return;
  }
  const fb = await api("GET", `/api/console?name=${res.name || ""}&lines=8`);
  await send(ctx, event, "已发送。最近输出：\n" + (fb.console || "（空）"));
}

async function h_start(ctx, event, rest) {
  const [name] = await pickName(rest);
  const res = await api("POST", "/api/start", { name });
  await send(ctx, event, (res && res.message) || `已请求启动 ${(res && res.name) || name}`);
}

async function h_stop(ctx, event, rest) {
  const [name] = await pickName(rest);
  const res = await api("POST", "/api/stop", { name });
  await send(ctx, event, (res && res.message) || `已请求停止 ${(res && res.name) || name}`);
}

async function h_forcestop(ctx, event, rest) {
  const [name] = await pickName(rest);
  const res = await api("POST", "/api/forcestop", { name });
  await send(ctx, event, (res && res.message) || `已强制结束 ${(res && res.name) || name}`);
}

async function h_delete(ctx, event, rest) {
  const name = rest.trim();
  if (!name) {
    await send(ctx, event, "用法：删除 <名称>");
    return;
  }
  const res = await api("POST", "/api/delete", { name });
  await send(ctx, event, (res && res.message) || `已删除 ${name}`);
}

async function h_downloads(ctx, event) {
  const data = await api("GET", "/api/downloads");
  const arr = data.downloads || [];
  if (!arr.length) {
    await send(ctx, event, "无下载任务");
    return;
  }
  const lines = arr.map((d) => `${d.name || "?"} ${d.state || "?"} ${d.percent || 0}%`);
  await send(ctx, event, "下载：\n" + lines.join("\n"));
}

const _ON = new Set(["on", "开", "启用", "开启", "true", "1", "start"]);
const _OFF = new Set(["off", "关", "关闭", "停用", "false", "0", "stop"]);

async function h_control(ctx, event, rest) {
  const parts = rest.split(/\s+/).filter(Boolean);
  if (parts.length < 2) {
    await send(ctx, event, "用法：webui|napcat|nonebot on|off");
    return;
  }
  const target = parts[0].toLowerCase();
  const arg = parts[1].toLowerCase();
  if (!["webui", "napcat", "nonebot"].includes(target)) {
    await send(ctx, event, "未知目标，仅支持 webui/napcat/nonebot");
    return;
  }
  let en;
  if (_ON.has(arg)) en = true;
  else if (_OFF.has(arg)) en = false;
  else {
    await send(ctx, event, "请使用 on 或 off");
    return;
  }
  const res = await api("POST", "/api/control", { target, enabled: en });
  if (!res || !res.success) {
    await send(ctx, event, (res && res.message) || "操作失败");
    return;
  }
  await send(ctx, event, `${target} 已${en ? "开启" : "关闭"}`);
}

const ALIAS = {
  帮助: h_help, help: h_help, "?": h_help,
  状态: h_status, status: h_status,
  服务器: h_servers, servers: h_servers,
  在线: h_players, list: h_players, players: h_players,
  控制台: h_console, console: h_console,
  属性: h_props, props: h_props,
  模组: h_mods, mods: h_mods,
  占用: h_usage, usage: h_usage,
  命令: h_command, cmd: h_command,
  启动: h_start, start: h_start,
  停止: h_stop, stop: h_stop,
  强关: h_forcestop, forcestop: h_forcestop,
  删除: h_delete, delete: h_delete,
  下载: h_downloads, downloads: h_downloads,
  webui: h_control, napcat: h_control, nonebot: h_control,
};

function parseTargets(s) {
  const out = [];
  for (const item of (s || "").split(",")) {
    const t = item.trim();
    if (!t || !t.includes(":")) continue;
    const idx = t.indexOf(":");
    out.push([t.slice(0, idx).trim().toLowerCase(), t.slice(idx + 1).trim()]);
  }
  return out;
}

async function dispatch(ctx, event, text) {
  const parts = text.trim().split(/\s+/).filter(Boolean);
  const cmd = (parts[0] || "").toLowerCase();
  const rest = parts.slice(1).join(" ");
  const handler = ALIAS[cmd];
  if (!handler) return;
  if (PRIVILEGED.has(cmd)) {
    const admins = cfg.msm_admins.split(",").map((s) => s.trim()).filter(Boolean);
    if (admins.length && !admins.includes(String(event.user_id))) {
      await send(ctx, event, "权限不足：该指令仅管理员可用");
      return;
    }
  }
  await handler(ctx, event, rest);
}

// ---------- MSM -> QQ 推送（WS 长连接） ----------
async function dispatchPush(data) {
  const message = (data.message || "").toString().trim();
  if (!message || !ctxRef) return;
  const scope = (data.scope || "all").toString().toLowerCase();
  let targets = (data.targets || []).map((t) =>
    Array.isArray(t) ? [String(t[0]), String(t[1])] : [String(t.type), String(t.id)]
  );
  if (scope === "admin") {
    if (!cfg.msm_admin) return;
    targets = [["private", cfg.msm_admin]];
  } else if (!targets.length) {
    targets = parseTargets(cfg.msm_notify_targets);
  }
  for (const [type, id] of targets) {
    const params = {
      message,
      message_type: type === "group" ? "group" : "private",
    };
    if (type === "group") params.group_id = String(id);
    else params.user_id = String(id);
    try {
      await ctxRef.actions.call("send_msg", params, ctxRef.adapterName, ctxRef.pluginManager.config);
    } catch (e) {
      log("warn", `推送失败 ${type}->${id}: ${e && e.message ? e.message : e}`);
    }
  }
}

function scheduleReconnect() {
  if (wsTimer) return;
  wsTimer = setTimeout(() => {
    wsTimer = null;
    wsLoop();
  }, 5000);
}

function wsLoop() {
  const base = cfg.msm_control_url
    .replace(/\/+$/, "")
    .replace(/^https:\/\//, "wss://")
    .replace(/^http:\/\//, "ws://");
  const wsUrl = base + "/ws";
  log("info", `连接 MSM 控制通道(WS) ${wsUrl}`);
  try {
    const ws = new WebSocket(wsUrl);
    wsRef = ws;
    ws.onopen = () => log("info", "WS 已连入 MSM，开始接收推送");
    ws.onmessage = async (ev) => {
      try {
        const raw = typeof ev.data === "string" ? ev.data : String(ev.data);
        await dispatchPush(JSON.parse(raw));
      } catch (e) {
        log("warn", `推送解析失败: ${e && e.message ? e.message : e}`);
      }
    };
    ws.onclose = () => {
      wsRef = null;
      log("warn", "WS 断开，5s 后重连");
      scheduleReconnect();
    };
    ws.onerror = () => {
      try { ws.close(); } catch (_) { /* noop */ }
    };
  } catch (e) {
    log("warn", `WS 连接异常: ${e && e.message ? e.message : e}`);
    scheduleReconnect();
  }
}

function applyConfig(incoming) {
  if (incoming && typeof incoming === "object") {
    for (const k of Object.keys(DEFAULT_CFG)) {
      if (incoming[k] !== undefined) cfg[k] = incoming[k];
    }
  }
}

// ---------- 配置落盘（NapCat 4.18 不会替第三方插件持久化配置，需自行保存） ----------
const CONFIG_PATH = fileURLToPath(new URL("./config.json", import.meta.url));

function loadConfigFile() {
  try {
    const raw = fs.readFileSync(CONFIG_PATH, "utf-8");
    applyConfig(JSON.parse(raw));
  } catch (_) {
    // 无配置文件则沿用注入/默认配置
  }
}

function saveConfigFile() {
  try {
    fs.writeFileSync(CONFIG_PATH, JSON.stringify(cfg, null, 2), "utf-8");
  } catch (e) {
    log("warn", `保存配置失败: ${e && e.message ? e.message : e}`);
  }
}

// ---------- 生命周期 ----------
export const plugin_init = async (ctx, ...rest) => {
  ctxRef = ctx;
  // NapCat 可能在 init 时通过第二参数或 ctx 注入已保存配置
  const injected = rest[0] || (ctx && ctx.config) || null;
  applyConfig(injected);
  loadConfigFile();   // 以本地文件为准（重启后可恢复）
  saveConfigFile();   // 若本地无文件，则把注入/默认配置落盘，保证下次可恢复
  log("info", `MSM 插件已加载，控制通道=${cfg.msm_control_url}`);
  wsLoop();
};

export const plugin_onmessage = async (ctx, event) => {
  if (!event || event.post_type !== "message") return;
  let text = (event.raw_message || "").toString();
  text = text.replace(/\[CQ:[^\]]*\]/g, "").trim(); // 去掉 CQ 码，仅保留纯文本指令
  if (!text) return;
  ctxRef = ctx;
  await dispatch(ctx, event, text);
};

export const plugin_cleanup = (ctx) => {
  if (wsTimer) {
    clearTimeout(wsTimer);
    wsTimer = null;
  }
  if (wsRef) {
    try { wsRef.close(); } catch (_) { /* noop */ }
    wsRef = null;
  }
  log("info", "MSM 插件已卸载");
};

export const plugin_get_config = () => ({ ...cfg });

export const plugin_set_config = (config) => {
  applyConfig(config);
  saveConfigFile();
  return true;
};

export const plugin_on_config_change = (config) => {
  const oldUrl = cfg.msm_control_url;
  applyConfig(config);
  saveConfigFile();
  // 控制通道地址变化时重建 WS 长连接
  if (cfg.msm_control_url !== oldUrl) {
    if (wsRef) {
      try { wsRef.close(); } catch (_) { /* noop */ }
      wsRef = null;
    }
    if (wsTimer) {
      clearTimeout(wsTimer);
      wsTimer = null;
    }
    wsLoop();
  }
};
