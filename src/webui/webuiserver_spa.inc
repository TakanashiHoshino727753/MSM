// 由 webuiserver.cpp 包含：返回与本地端 UI 设计与功能一致的单页管理面板。
// 除"关闭 WebUI"设置外，其余功能与本地端保持一致；原弹窗（创建/导入）以标签页切换呈现。
QString WebUIServer::m_spaHtml() const
{
    return R"WEBUI(<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>MSM 管理面板</title>
<style>
:root{
  --bg:#0f1115; --panel:#171a21; --panel-alt:#1e222b; --border:#2a2f3a;
  --text:#e6e8ee; --text-muted:#9aa3b2; --accent:#4f8cff; --accent-soft:rgba(79,140,255,.15);
}
*{box-sizing:border-box}
body{margin:0;font-family:"Segoe UI",system-ui,"Microsoft YaHei",sans-serif;background:var(--bg);color:var(--text);font-size:14px}
button{font:inherit;cursor:pointer;border:1px solid var(--border);background:var(--panel-alt);color:var(--text);border-radius:8px;padding:8px 14px;transition:.15s}
button:hover{border-color:var(--accent)}
button.primary{background:var(--accent);color:#fff;border-color:var(--accent)}
button.ghost{background:transparent}
button:disabled{opacity:.5;cursor:not-allowed}
input,select{font:inherit;background:var(--panel-alt);color:var(--text);border:1px solid var(--border);border-radius:8px;padding:8px 10px;outline:none}
input:focus,select:focus{border-color:var(--accent)}
.layout{display:flex;min-height:100vh}
.sidebar{width:210px;background:var(--panel);border-right:1px solid var(--border);padding:18px 12px;display:flex;flex-direction:column;gap:6px;position:sticky;top:0;height:100vh}
.brand{font-size:18px;font-weight:700;padding:6px 10px 14px;color:var(--accent)}
.nav{display:flex;flex-direction:column;gap:4px}
.nav button{text-align:left;background:transparent;border:0;color:var(--text-muted);padding:10px 12px;border-radius:8px}
.nav button.active{background:var(--accent-soft);color:var(--accent);font-weight:600}
.mon{margin-top:auto;padding:10px;font-size:12px;color:var(--text-muted)}
.mon .monhead{display:flex;justify-content:space-between;margin-top:6px}
.mon .chart{display:block;width:100%;height:34px;background:var(--bg);border:1px solid var(--border);border-radius:4px;margin:4px 0 2px}
.main{flex:1;padding:24px;overflow:auto}
.page{display:none}
.page.active{display:block}
h2{margin:0 0 16px;font-size:20px}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.card{background:var(--panel);border:1px solid var(--border);border-radius:12px;padding:16px;margin-bottom:12px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:12px}
.serv{background:var(--panel);border:1px solid var(--border);border-radius:12px;padding:14px;cursor:pointer;transition:.15s}
.serv:hover{border-color:var(--accent)}
.badge{display:inline-block;padding:2px 8px;border-radius:999px;font-size:12px;background:rgba(62,196,109,.18);color:#3ec46d}
.badge.off{background:var(--panel-alt);color:var(--text-muted)}
.sdot{width:8px;height:8px;border-radius:50%;background:var(--text-muted);flex:none}
.sdot.run{background:#3ec46d}
.iconbtn{display:inline-flex;align-items:center;justify-content:center;min-width:34px;height:32px;padding:0 8px;font-size:15px;border-radius:8px}
.dl-go{background:#3ec46d;color:#06210f;border-color:#3ec46d}
.dl-go:hover{filter:brightness(1.08)}
.runbtn{background:#3ec46d;color:#06210f;border-color:#3ec46d}
.runbtn:hover{filter:brightness(1.08)}
.stopbtn{background:#e0533d;color:#fff;border-color:#e0533d}
.stopbtn:hover{filter:brightness(1.08)}
.stopbtn.force{background:#b23b2b}
.swbtn{display:inline-flex;align-items:center;gap:6px;padding:6px 12px;font-size:13px}
.swbtn.on{background:var(--accent-soft);color:var(--accent);border-color:var(--accent)}
.lg-red{color:var(--lg-red)}.lg-orange{color:var(--lg-orange)}.lg-yellow{color:var(--lg-yellow)}.lg-green{color:var(--lg-green)}.lg-blue{color:var(--lg-blue)}.lg-white{color:var(--lg-white)}
.tag{font-size:12px;color:var(--text-muted)}
.console{background:var(--console-bg);color:var(--console-fg);font-family:Consolas,monospace;font-size:12px;padding:12px;border-radius:8px;height:300px;overflow:auto;white-space:pre-wrap;line-height:1.45}
.muted{color:var(--text-muted)}
.pill{padding:4px 10px;border-radius:999px;border:1px solid var(--border);background:var(--panel-alt);color:var(--text-muted)}
.pill.on{color:var(--accent);border-color:var(--accent)}
.cat{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:8px}
.cat button{padding:8px 16px}
.cat button.on{background:var(--accent-soft);color:var(--accent);border-color:var(--accent);font-weight:600}
.item{display:flex;gap:12px;align-items:center;justify-content:space-between;padding:10px 12px;border:1px solid var(--border);border-radius:10px;background:var(--panel)}
.item .meta{min-width:0}
.item .t{font-weight:600}
.item .s{font-size:12px;color:var(--text-muted);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.prog{height:6px;background:var(--panel-alt);border-radius:4px;overflow:hidden;width:160px}
.prog i{display:block;height:100%;background:var(--accent);width:0%}
.dlitem{padding:10px 0;border-top:1px solid var(--border)}
.dlitem:first-child{border-top:0}
.dlrow{display:flex;justify-content:space-between;align-items:center;gap:10px}
.dltitle{font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.dlstate{font-size:12px;padding:2px 8px;border-radius:999px;background:var(--panel-alt);color:var(--text-muted)}
.dlstate.downloading{background:var(--accent-soft);color:var(--accent)}
.dlstate.done{background:rgba(80,200,120,.18);color:#5fd98a}
.dlstate.error,.dlstate.canceled{background:rgba(255,90,90,.18);color:#ff8585}
.dlstate.paused{background:rgba(255,190,90,.18);color:#ffc46b}
.dlbar{height:6px;background:var(--panel-alt);border-radius:4px;overflow:hidden;margin:8px 0}
.dlbar i{display:block;height:100%;background:var(--accent);width:0%}
.dlbar.done i{background:#5fd98a}
.dlbar.error i,.dlbar.canceled i{background:#ff7070}
.dlbar.paused i{background:#ffc46b}
.dlbar.downloading i{background:var(--accent)}
.dlactions{display:flex;gap:6px;flex-wrap:wrap;margin-top:6px}
.dlerr{font-size:12px;color:#ff8585;margin-top:4px;word-break:break-all}
.dlpath{font-size:11px;color:var(--text-muted);margin-top:4px;word-break:break-all}
.swatches{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.swatch{width:26px;height:26px;border-radius:13px;border:1px solid #0003;cursor:pointer}
.swatch.on{outline:3px solid var(--text);outline-offset:1px}
label.ck{display:flex;gap:8px;align-items:center;cursor:pointer}
hr{border:0;border-top:1px solid var(--border);margin:16px 0}
.msg{font-size:13px;color:var(--text-muted);margin-top:8px}
.err{color:#ff8080}
#toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:var(--panel);border:1px solid var(--accent);color:var(--text);padding:10px 18px;border-radius:10px;opacity:0;transition:.25s;pointer-events:none}
#toast.show{opacity:1}
.modal{background:var(--panel);border:1px solid var(--border);border-radius:12px;width:min(700px,94vw);max-height:90vh;display:flex;flex-direction:column;box-shadow:0 10px 40px rgba(0,0,0,.45)}
.modal-h{display:flex;justify-content:space-between;align-items:center;padding:14px 16px;border-bottom:1px solid var(--border)}
.modal-h button{padding:4px 12px}
.modal-b{padding:14px 16px;overflow:auto}
.modal-f{padding:10px 16px;border-top:1px solid var(--border);font-size:12px}
.cat-title{color:var(--accent);font-weight:600;font-size:13px;margin:12px 0 6px}
.cmdgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:8px}
.cmdc{display:flex;flex-direction:column;gap:3px;align-items:flex-start;text-align:left;padding:8px 10px}
.cmdc .cc{font-family:Consolas,monospace;font-size:13px}
.cmdc .cd{font-size:11px;color:var(--text-muted)}
#cmdset,#world,#players,#dlpop,#props{position:fixed;inset:0;background:rgba(0,0,0,.5);display:none;align-items:center;justify-content:center;z-index:50}
#dlpop .modal-b{max-height:62vh;overflow:auto}
.qk{padding:5px 10px;font-size:12px}
.pa{margin-left:6px;padding:3px 9px;font-size:12px}
.worldstate{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;margin:6px 0}
.worldstate .k{color:var(--text-muted)}
.worldstate .v{font-family:Consolas,monospace}
table td{padding:5px 6px;border-bottom:1px solid var(--border);font-size:13px;vertical-align:top}
table td.t{color:var(--text-muted);width:42%}
.prop-row{display:flex;align-items:center;gap:10px;padding:6px 0;border-bottom:1px solid var(--border)}
.prop-key{color:var(--text);min-width:160px;max-width:40%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:13px}
.toggle-wrap{display:flex;align-items:center;gap:8px}
.toggle-wrap input[type=checkbox]{display:none}
.toggle-wrap .slider{width:36px;height:20px;border-radius:10px;background:var(--border);position:relative;cursor:pointer;transition:.2s;flex-shrink:0}
.toggle-wrap .slider::after{content:'';position:absolute;width:16px;height:16px;border-radius:50%;background:#fff;top:2px;left:2px;transition:.2s}
.toggle-wrap input:checked+.slider{background:var(--accent)}
.toggle-wrap input:checked+.slider::after{left:18px}
.toggle-wrap .sval{font-size:12px;color:var(--text-muted);min-width:30px}
.prop-input.text{background:var(--panel-alt);border:1px solid var(--border);border-radius:6px;padding:6px 10px;color:var(--text);font-size:13px;font-family:Consolas,monospace}
#props-form{max-height:50vh;overflow-y:auto}
</style>
</head>
<body>
<div class="layout">
  <aside class="sidebar">
    <div class="brand">⛏ MSM</div>
    <div class="nav">
      <button data-tab="servers" class="active">🖥 服务器</button>
      <button data-tab="downloads">⬇ 下载中心</button>
      <button data-tab="create">➕ 创建服务器</button>
      <button data-tab="import">📦 导入整合包</button>
      <button data-tab="settings">⚙ 应用设置</button>
    </div>
    <div class="mon">
      <div class="monhead"><span id="monCpu">CPU</span><span id="cpuv">0%</span></div>
      <canvas id="cpuChart" class="chart"></canvas>
      <div class="monhead"><span id="monMem">内存</span><span id="memv">0%</span></div>
      <canvas id="memChart" class="chart"></canvas>
    </div>
  </aside>
  <main class="main">
    <section id="page-servers" class="page active"></section>
    <section id="page-downloads" class="page"></section>
    <section id="page-create" class="page"></section>
    <section id="page-import" class="page"></section>
    <section id="page-settings" class="page"></section>
  </main>
</div>
<div id="toast"></div>
<div id="cmdset"></div>
<div id="world"></div>
<div id="players"></div>
<div id="props"></div>
<div id="tokModal" style="display:none;position:fixed;inset:0;background:rgba(0,0,0,.6);z-index:1000;align-items:center;justify-content:center">
  <div style="background:var(--panel);border:1px solid var(--border);border-radius:12px;padding:22px;width:340px;max-width:90vw">
    <h2 style="margin:0 0 6px;font-size:18px">🔒 需要访问令牌</h2>
    <p id="tokMsg" style="color:var(--text-muted);font-size:13px;margin:0 0 14px">请输入应用设置中的“WebUI 访问令牌”以继续。</p>
    <input id="tokInput" type="text" placeholder="访问令牌" style="width:100%" autocomplete="off"/>
    <div class="row" style="margin-top:14px;justify-content:flex-end">
      <button class="primary" id="tokSave">保存并继续</button>
    </div>
  </div>
</div>
<script>
let theme = { dark:true, accent:'#4f8cff' };
const CATEGORIES=[{key:'java',label:'Java'},{key:'server',label:'服务端'},{key:'mod',label:'模组'},{key:'modpack',label:'整合包'},{key:'resourcepack',label:'资源包'},{key:'plugin',label:'插件'}];
const SERVERTYPES=[{key:'paper',label:'Paper'},{key:'vanilla',label:'Vanilla'},{key:'mod',label:'模组服'}];
const CMDCAT=[{cat:'玩家',items:[
 {c:'gamemode <mode> <player>',d:'设置玩家游戏模式'},
 {c:'tp <player> <target>',d:'传送玩家到目标'},
 {c:'give <player> <item> [count]',d:'给予玩家物品'},
 {c:'kill <player>',d:'杀死实体/玩家'},
 {c:'op <player>',d:'给予管理员权限'},
 {c:'deop <player>',d:'撤销管理员权限'},
 {c:'kick <player> [reason]',d:'踢出玩家'},
 {c:'ban <player> [reason]',d:'封禁玩家'},
 {c:'pardon <player>',d:'解封玩家'},
 {c:'whitelist add <player>',d:'加入白名单'},
 {c:'whitelist remove <player>',d:'移除白名单'}
]},{cat:'世界 / 环境',items:[
 {c:'time set day',d:'设为白天'},
 {c:'time set night',d:'设为夜晚'},
 {c:'weather clear',d:'放晴'},
 {c:'weather rain',d:'下雨'},
 {c:'weather thunder',d:'雷暴'},
 {c:'difficulty <difficulty>',d:'设置游戏难度'},
 {c:'gamerule keepInventory true',d:'死亡保留物品'},
 {c:'gamerule doDaylightCycle false',d:'停止昼夜循环'},
 {c:'worldborder set <size>',d:'设置世界边界'}
]},{cat:'管理',items:[
 {c:'save-all',d:'保存所有存档'},
 {c:'save-on',d:'开启自动保存'},
 {c:'save-off',d:'关闭自动保存'},
 {c:'stop',d:'关闭服务器'},
 {c:'reload',d:'重新加载配置'},
 {c:'whitelist on',d:'开启白名单'},
 {c:'whitelist off',d:'关闭白名单'},
 {c:'banlist',d:'查看封禁列表'},
 {c:'list',d:'列出在线玩家'}
]}];
const QUICK=[{label:'保存',cmd:'save-all'},{label:'白天',cmd:'time set day'},{label:'夜晚',cmd:'time set night'},{label:'放晴',cmd:'weather clear'},{label:'难度',cmd:'difficulty <difficulty>'},{label:'OP',cmd:'op <player>'},{label:'传送',cmd:'tp <player> <target>'},{label:'给予',cmd:'give <player> <item> [count]'},{label:'踢出',cmd:'kick <player>'},{label:'重载',cmd:'reload'},{label:'列出',cmd:'list'},{label:'关机',cmd:'stop'}];
const ACCENTS=['#4f8cff','#8a5cf6','#ec4899','#22c55e','#f59e0b','#ef4444','#06b6d4','#14b8a6','#a855f7','#eab308','#3b82f6','#64748b'];
// 属性名中英映射
const PROPS_ZH={
  'accepts-transfers':'接受传送请求','allow-flight':'允许飞行','allow-nether':'允许下界',
  'broadcast-console-to-ops':'控制台广播给管理员','broadcast-rcon-to-ops':'RCON 广播给管理员',
  'bug-report-link':'Bug 报告链接','difficulty':'游戏难度','enable-code-of-conduct':'启用行为准则',
  'enable-command-block':'启用命令方块','enable-jmx-monitoring':'启用 JMX 监控',
  'enable-query':'启用查询','enable-rcon':'启用 RCON','enable-status':'启用状态',
  'enforce-secure-profile':'强制安全配置文件','enforce-whitelist':'强制白名单',
  'entity-broadcast-range-percentage':'实体广播范围百分比','force-gamemode':'强制游戏模式',
  'function-permission-level':'函数权限等级','gamemode':'默认游戏模式','generate-structures':'生成结构',
  'generator-settings':'生成器设置','hardcore':'极限模式','hide-online-players':'隐藏在线玩家',
  'initial-disabled-packs':'初始禁用数据包','initial-enabled-packs':'初始启用数据包',
  'level-name':'世界名称','level-seed':'世界种子','level-type':'世界类型','log-ips':'记录 IP',
  'management-server-allowed-origins':'管理服务器允许的来源','management-server-enabled':'启用管理服务器',
  'management-server-host':'管理服务器主机','management-server-port':'管理服务器端口',
  'management-server-secret':'管理服务器密钥',
  'management-server-tls-enabled':'管理服务器 TLS','management-server-tls-keystore':'TLS 密钥库路径',
  'management-server-tls-keystore-password':'TLS 密钥库密码',
  'max-chained-neighbor-updates':'最大连锁更新','max-build-height':'最大建筑高度',
  'max-entities':'最大实体数','max-players':'最大玩家数','max-tick-time':'最大单 tick 时间(ms)',
  'max-world-size':'最大世界大小','motd':'服务器描述','network-compression-threshold':'网络压缩阈值',
  'online-mode':'在线验证','op-permission-level':'管理员权限等级','player-idle-timeout':'玩家空闲超时(分钟)',
  'pause-when-empty-seconds':'无人时暂停(秒)',
  'previews-chat':'预览聊天','prevent-proxy-connections':'阻止代理连接','pvp':'允许 PVP',
  'query.port':'查询端口','rate-limit':'速率限制','rcon.password':'RCON 密码','rcon.port':'RCON 端口',
  'region-file-compression':'区域文件压缩','require-resource-pack':'强制资源包',
  'resource-pack':'资源包地址','resource-pack-id':'资源包 UUID','resource-pack-prompt':'资源包提示',
  'resource-pack-sha1':'资源包 SHA1','server-ip':'服务器 IP','server-port':'服务器端口',
  'simulation-distance':'模拟距离','spawn-animals':'生成动物','spawn-monsters':'生成怪物',
  'spawn-npcs':'生成 NPC','spawn-protection':'出生点保护范围','status-heartbeat-interval':'状态心跳间隔',
  'sync-chunk-writes':'同步区块写入','text-filtering-config':'文本过滤配置',
  'text-filtering-version':'文本过滤版本号','use-native-transport':'使用本地传输',
  'view-distance':'视距','white-list':'白名单'
};
function propLabel(k){return curLang==='en'?k:(PROPS_ZH[k]||k);}
let curLang='zh'; // 'zh'|'en'
let curTab='servers', curServer=null, timers={};
// 全局语言：WebUI 自身文案中英映射。T(zh) 在英文模式下返回映射值，否则原样返回。
// 缺失项回退到中文原文，因此未翻译文案不会报错。
function T(zh){return curLang==='en'?(STR[zh]||zh):zh;}
const STR={
 '🖥 服务器':'🖥 Servers','⬇ 下载中心':'⬇ Downloads','➕ 创建服务器':'➕ Create Server','📦 导入整合包':'📦 Import Modpack','⚙ 应用设置':'⚙ App Settings',
 '玩家':'Players','世界 / 环境':'World / Environment','管理':'Management',
 '设置玩家游戏模式':'Set Player Game Mode','传送玩家到目标':'Teleport Player to Target','给予玩家物品':'Give Player Item','杀死实体/玩家':'Kill Entity/Player','给予管理员权限':'Grant Operator','撤销管理员权限':'Revoke Operator','踢出玩家':'Kick Player','封禁玩家':'Ban Player','解封玩家':'Unban Player','加入白名单':'Add to Whitelist','移除白名单':'Remove from Whitelist',
 '设为白天':'Set to Day','设为夜晚':'Set to Night','放晴':'Clear Weather','下雨':'Rain','雷暴':'Thunderstorm','设置游戏难度':'Set Game Difficulty','死亡保留物品':'Keep Inventory on Death','停止昼夜循环':'Stop Day/Night Cycle','设置世界边界':'Set World Border',
 '保存所有存档':'Save All Worlds','开启自动保存':'Enable Auto-Save','关闭自动保存':'Disable Auto-Save','关闭服务器':'Stop Server','重新加载配置':'Reload Config','开启白名单':'Enable Whitelist','关闭白名单':'Disable Whitelist','查看封禁列表':'View Ban List','列出在线玩家':'List Online Players',
 '保存':'Save','白天':'Day','夜晚':'Night','放晴':'Clear','难度':'Difficulty','OP':'OP','传送':'Teleport','给予':'Give','踢出':'Kick','重载':'Reload','列出':'List','关机':'Shutdown',
 '服务器':'Servers','总览':'Overview','暂无服务器，去"创建服务器"或"导入整合包"添加一个。':'No servers yet. Go to "Create Server" or "Import Modpack" to add one.',
 '运行中':'Running','已停止':'Stopped','启动':'Start','停止':'Stop','强制停止':'Force Stop','在线玩家：':'Online players: ','快捷操作':'Quick Actions','发送':'Send','指令集':'Command Set','世界管理':'World Management','查看属性':'View Properties','属性':'Properties','玩家管理':'Player Management','查看 Mods':'View Mods','无在线玩家':'No online players',
'下载任务':'Download Tasks','已完成':'Completed','出错':'Error','已暂停':'Paused','下载中':'Downloading','清空已完成':'Clear Finished','暂无下载任务':'No download tasks',
 '服务端属性':'Server Properties','暂无配置项':'No config items','Mods':'Mods',
 '已填入输入框':'Filled into input','已发送：':'Sent: ','失败：':'Failed: ',
 '点击指令以填入输入框':'Click a command to fill the input box','点击指令以对玩家执行操作':'Click a command to act on the player',
 '刷新状态':'Refresh Status','快捷命令':'Quick Commands','世界配置（server.properties）':'World Config (server.properties)','无':'None','读取中…':'Reading…',
 '和平':'Peaceful','简单':'Easy','普通':'Normal','困难':'Hard',
 '难度':'Difficulty','时间(daytime)':'Time (daytime)','天气':'Weather','种子':'Seed','未读取到（服务器未运行或日志非英文）':'Not read (server not running or non-English log)',
 '下载中心':'Downloads','搜索…':'Search…','搜索':'Search','刷新':'Refresh','清理临时 Java':'Clean Temp Java','保存目录：':'Save directory: ','设置':'Set','状态：':'Status: ','加载中…':'Loading…','已下载':'Downloaded','下载':'Download','服务端':'Server','模组':'Mod','整合包':'Modpack','模组服':'Modded','资源包':'Resource Pack','插件':'Plugin','内存':'Memory','创建后自动检查/安装':'will be checked/installed after creation',
 '版本':'Version','加载器':'Loader','下载选中加载器':'Download Selected Loaders','已选':'Selected',
 '创建服务器':'Create Server','类型':'Type','名称':'Name','自动生成':'Auto','目录':'Directory','我已阅读并同意 Minecraft EULA':'I have read and agree to the Minecraft EULA','创建并安装':'Create & Install',
 '导入整合包':'Import Modpack','整合包(.zip)':'Modpack (.zip)','目标目录':'Target directory','导入':'Import',
 '应用设置':'App Settings','外观':'Appearance','深色模式':'Dark Mode','主题色':'Theme Color','语言与自启':'Language & Autostart','语言':'Language','开机自启':'Start on Boot','保存设置':'Save Settings','默认服务器目录':'Default Server Directory','浏览':'Browse','提示：WebUI 自身的"启用/端口"设置不会在此显示，避免从面板内部把自己关掉。':'Note: WebUI\'s own "Enable/Port" settings are hidden here to avoid disabling itself from within the panel.',
 '已开始下载':'Download started','已开始下载选中加载器':'Started downloading selected loaders','已清理临时 Java':'Cleaned temp Java','已提交':'Submitted','安装完成':'Installation complete','导入完成':'Import complete','已保存':'Saved','属性已保存':'Properties saved','保存失败':'Save failed','失败:':'Failed: ','保存失败:':'Save failed:','加载失败':'Load failed','提示：':'Note: ','暂不支持':'Not supported','QQ机器人':'QQ Bot','本地插件控制':'Local plugins','启用QQ机器人':'Enable QQ Bot','联动启动':'Linked start','运行中':'Running','已停止':'Stopped',
'NapCat 路径':'NapCat Path','NoneBot 目录':'NoneBot Dir','运行状态':'Status','msm 控制插件':'msm control plugin','昵称状态更新间隔(秒)':'Nickname update interval (s)','0=关闭':'0 = off','留空则自动探测':'Auto-detect if empty',
'未启用':'Disabled','启动中':'Starting','外部运行（由你自行启动）':'External (started by you)','等待连接':'Waiting for connection',
'已安装':'Installed','未安装（机器人将无法控制服务器）':'Not installed (bot cannot control servers)','安装中…':'Installing…','安装失败':'Install failed','未知':'Unknown',
'设置与本地端一致；"运行状态"指 NapCat / NoneBot 程序进程本身':'Same as the desktop app; "Status" refers to the NapCat / NoneBot processes themselves'
};
const NAVMAP={servers:'🖥 服务器',downloads:'⬇ 下载中心',create:'➕ 创建服务器',import:'📦 导入整合包',settings:'⚙ 应用设置'};
function relocalizeNav(){document.querySelectorAll('.nav button').forEach(b=>{if(NAVMAP[b.dataset.tab])b.textContent=T(NAVMAP[b.dataset.tab]);});}
function relocalizeMonitors(){const c=document.getElementById('monCpu');if(c)c.textContent=T('CPU');const m=document.getElementById('monMem');if(m)m.textContent=T('内存');}
function applyLangUI(){
  relocalizeNav();
  relocalizeMonitors();
  if(curTab==='servers'){ if(curServer) refreshServer(); else showGrid(); }
  else if(curTab==='downloads') renderDownloads();
  else if(curTab==='create') renderCreate();
  else if(curTab==='import') renderImport();
  else if(curTab==='settings') renderSettings();
}

function hexA(hex,a){const h=hex.replace('#','');const r=parseInt(h.substr(0,2),16),g=parseInt(h.substr(2,2),16),b=parseInt(h.substr(4,2),16);return `rgba(${r},${g},${b},${a})`;}
function applyTheme(){const r=document.documentElement.style;r.setProperty('--accent',theme.accent);r.setProperty('--accent-soft',hexA(theme.accent,.15));
  r.setProperty('--bg',theme.dark?'#0f1115':'#f4f6fb');r.setProperty('--panel',theme.dark?'#171a21':'#ffffff');
  r.setProperty('--panel-alt',theme.dark?'#1e222b':'#eef1f6');r.setProperty('--border',theme.dark?'#2a2f3a':'#d7dce5');
  r.setProperty('--text',theme.dark?'#e6e8ee':'#1c2230');r.setProperty('--text-muted',theme.dark?'#9aa3b2':'#5b667a');
  // 控制台：背景跟随主题（深色用深面板、浅色用浅面板），不再硬黑底；染色颜色随主题深浅切换以保证对比度
  r.setProperty('--console-bg',theme.dark?'#14161c':'#f7f9fc');
  r.setProperty('--console-fg',theme.dark?'#cfe3ff':'#1c2230');
  r.setProperty('--lg-red',theme.dark?'#ff6b6b':'#c92a2a');
  r.setProperty('--lg-orange',theme.dark?'#ffa94d':'#e8590c');
  r.setProperty('--lg-yellow',theme.dark?'#ffe066':'#b08900');
  r.setProperty('--lg-green',theme.dark?'#69db7c':'#2f9e44');
  r.setProperty('--lg-blue',theme.dark?'#74c0fc':'#1971c2');
  r.setProperty('--lg-white',theme.dark?'#cfe3ff':'#1c2230');}
function esc(s){return (s==null?'':String(s)).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));}
function toast(m){const t=document.getElementById('toast');t.textContent=m;t.classList.add('show');setTimeout(()=>t.classList.remove('show'),1800);}
// ---------------- 访问令牌 ----------------
// 令牌来自：1) URL ?token=（桌面端“在浏览器打开”自动带上）；2) 上次输入后存入 localStorage。
let API_TOKEN = (function(){ try { return localStorage.getItem('msm_token') || ''; } catch(e){ return ''; } })();
(function(){
  try{
    const t = new URLSearchParams(location.search).get('token');
    if(t){ API_TOKEN = t; localStorage.setItem('msm_token', t); }
  }catch(e){}
})();
function showTokenModal(msg){
  const m=document.getElementById('tokModal');
  if(msg) document.getElementById('tokMsg').textContent=msg;
  m.style.display='flex';
  const inp=document.getElementById('tokInput'); inp.value=''; inp.focus();
}
document.getElementById('tokSave').onclick=function(){
  const v=document.getElementById('tokInput').value.trim();
  if(!v) return;
  API_TOKEN=v; try{ localStorage.setItem('msm_token',v); }catch(e){}
  document.getElementById('tokModal').style.display='none';
  // 重新加载当前页数据
  if(curTab==='servers'){ if(curServer) refreshServer(); else showGrid(); }
  else if(curTab==='downloads') renderDownloads();
  else if(curTab==='create') renderCreate();
  else if(curTab==='import') renderImport();
  else if(curTab==='settings') renderSettings();
  tickMon();
};
async function api(path,opts){
  opts=opts||{};
  const h=opts.headers||{};
  if(API_TOKEN) h['Authorization']='Bearer '+API_TOKEN;
  opts.headers=h;
  const r=await fetch(path,opts);
  if(r.status===401){ showTokenModal('访问被拒绝：令牌错误或未提供，请重新输入。'); throw new Error('HTTP 401'); }
  if(!r.ok) throw new Error('HTTP '+r.status);
  return r.json();
}

document.querySelectorAll('.nav button').forEach(b=>b.onclick=()=>switchTab(b.dataset.tab));
function switchTab(tab){
  curTab=tab;
  document.querySelectorAll('.nav button').forEach(b=>b.classList.toggle('active',b.dataset.tab===tab));
  document.querySelectorAll('.page').forEach(p=>p.classList.toggle('active',p.id==='page-'+tab));
  stopTimers();
  if(tab==='servers')enterServers();
  else if(tab==='downloads')renderDownloads();
  else if(tab==='create')renderCreate();
  else if(tab==='import')renderImport();
  else if(tab==='settings')renderSettings();
}
function stopTimers(){Object.values(timers).forEach(clearInterval);timers={};}

// 系统监控 —— 折线图（仿本地桌面端：固定容量、最新点在最右、>85% 变红）
const CHART_CAP=60; let cpuHist=[], memHist=[];
// 折线图绘制：与本地 SideBar.drawLineChart 逻辑一致（新样本在最右，自右向左滑动）
function drawLineChart(canvas, history, capacity){
  const ctx=canvas.getContext('2d'); const w=canvas.width, h=canvas.height;
  ctx.clearRect(0,0,w,h);
  const len=history.length; if(len===0) return;
  const step=w/(capacity-1);
  const last=history[len-1]/100;
  const lineColor=last>0.85?'#e74c3c':'#4f8cff';
  const fill=last>0.85?'rgba(231,76,60,0.18)':'rgba(79,140,255,0.15)';
  const px=i=>w-(len-1-i)*step, py=i=>h-(history[i]/100)*h;
  ctx.beginPath(); ctx.moveTo(px(0),h);
  for(let i=0;i<len;i++) ctx.lineTo(px(i),py(i));
  ctx.lineTo(px(len-1),h); ctx.closePath(); ctx.fillStyle=fill; ctx.fill();
  ctx.beginPath(); ctx.moveTo(px(0),py(0));
  for(let j=1;j<len;j++) ctx.lineTo(px(j),py(j));
  ctx.lineWidth=1.5; ctx.strokeStyle=lineColor; ctx.stroke();
}
function paintChart(id,hist){const c=document.getElementById(id);if(!c)return;if(c.clientWidth>0)c.width=c.clientWidth;if(c.clientHeight>0)c.height=c.clientHeight;drawLineChart(c,hist,CHART_CAP);}
function tickMon(){api('/api/system').then(s=>{const cpu=s.cpu||0,mem=s.mem||0;
  const cv=document.getElementById('cpuv'); if(cv) cv.textContent=Math.round(cpu)+'%';
  const mv=document.getElementById('memv'); if(mv){ mv.textContent=Math.round(mem)+'%'; mv.style.color=mem>85?'#e74c3c':''; }
  cpuHist.push(cpu); if(cpuHist.length>CHART_CAP) cpuHist.shift();
  memHist.push(mem); if(memHist.length>CHART_CAP) memHist.shift();
  paintChart('cpuChart',cpuHist); paintChart('memChart',memHist);
}).catch(()=>{});}

// ---------------- 服务器 ----------------
function cardInner(s){
  const act = s.running
    ? `<button class="stopbtn" onclick="event.stopPropagation();actCard('stop','${esc(s.name)}')">${T('停止')}</button><button class="stopbtn force" onclick="event.stopPropagation();actCard('forcestop','${esc(s.name)}')">${T('强制停止')}</button>`
    : `<button class="runbtn" onclick="event.stopPropagation();actCard('start','${esc(s.name)}')">${T('启动')}</button>`;
  return `<div style="display:flex;justify-content:space-between"><b>${esc(s.name)}</b><span class="badge ${s.running?'':'off'}">${s.running?T('运行中'):T('已停止')}</span></div>`
    +`<div class="tag">${esc(s.version)} · ${esc(s.type)}</div><div class="tag" style="margin-top:4px">${esc(s.path)}</div>`
    +`<div class="row" style="margin-top:8px">${act}<button onclick="openServer('${esc(s.name)}')">${T('详情')}</button><button class="ghost" onclick="event.stopPropagation();showPropsModal('${esc(s.name)}')">${T('属性')}</button></div>`;
}
function makeCard(s){const d=document.createElement('div');d.className='serv';d.dataset.name=s.name;d.dataset.running=String(s.running);d.innerHTML=cardInner(s);d.onclick=()=>openServer(s.name);return d;}
async function renderServersInto(c){
  let data;try{data=await api('/api/servers');}catch(e){c.innerHTML='<p class="err">'+T('加载失败')+'</p>';return;}
  const servers=data.servers||[];
  c.innerHTML='<h2>'+T('服务器')+'</h2>'
    +'<div class="row" style="margin-bottom:10px">'
    +'<button class="primary" onclick="switchTab(\'create\')">'+T('创建服务器')+'</button>'
    +'<button onclick="switchTab(\'import\')">'+T('导入整合包')+'</button>'
    +'<button onclick="showGrid()">'+T('刷新')+'</button></div>'
    +'<div class="grid" id="sgrid"></div>';
  const g=document.getElementById('sgrid');
  if(!servers.length)g.innerHTML='<p class="muted">'+T('暂无服务器，去"创建服务器"或"导入整合包"添加一个。')+'</p>';
  servers.forEach(s=>g.appendChild(makeCard(s)));
  renderSwitcherList(servers);   // 复用同一份数据渲染顶部切换器，避免再发一次 /api/servers
}
// 单次取数同时刷新「网格卡片运行态」与「顶部切换器状态点」，替代原先两个独立轮询定时器（各发一次 /api/servers）。
async function refreshServers(){
  let data;
  try{ data=await api('/api/servers'); }catch(e){ return; }
  const servers=(data&&data.servers)||[];
  const map={}; servers.forEach(s=>map[s.name]=s);
  // 网格卡片：仅运行态变化才重建（避免每 2s 全量重渲染导致按钮闪烁/点击丢失）
  const g=document.getElementById('sgrid');
  if(g){
    g.querySelectorAll('.serv').forEach(c=>{
      const s=map[c.dataset.name]; if(!s) return;
      if(c.dataset.running!==String(s.running)){ c.dataset.running=String(s.running); c.innerHTML=cardInner(s); }
    });
  }
  // 顶部切换器：仅更新状态点（当前项高亮由 renderSwitcherList 决定，此处不动）
  const sl=document.getElementById('swlist');
  if(sl){
    sl.querySelectorAll('.swbtn').forEach(b=>{
      const d=b.querySelector('.sdot'); if(d) d.className='sdot '+((map[b.dataset.name]&&map[b.dataset.name].running)?'run':'');
    });
  }
}

// 确保仅有一个服务器轮询定时器（网格与切换器共用），避免重复请求
function ensureServerPolling(){ if(!timers.switcher) timers.switcher=setInterval(refreshServers,2000); }
function clearContentTimers(){['grid','server','console'].forEach(k=>{if(timers[k]){clearInterval(timers[k]);delete timers[k];}});}
function showGrid(){clearContentTimers();const c=document.getElementById('scontent');if(!c)return;renderServersInto(c);ensureServerPolling();}
function enterServers(){
  curTab='servers';
  const el=document.getElementById('page-servers');if(!el)return;
  el.innerHTML='<div id="switcher"></div><div id="scontent"></div>';
  renderSwitcher();
  ensureServerPolling();
  if(curServer)showDetail(curServer);else showGrid();
}
function deleteServer(name){
  if(!confirm(T('确定删除服务器 “')+name+'” ？此操作不可撤销。'))return;
  fetch('/api/servers/'+encodeURIComponent(name),{method:'DELETE'}).then(()=>{toast(T('已删除'));showGrid();}).catch(e=>toast(T('失败:')+e.message));
}
async function openServer(name){showDetail(name);}
async function showDetail(name){
  clearContentTimers();
  curServer=name;
  const c=document.getElementById('scontent');if(!c)return;
  c.innerHTML='<h2>'+esc(name)+'</h2><div id="sdetail"></div><div id="detailExtra"></div>';
  refreshServers();
  await refreshServer();
  timers.server=setInterval(refreshServer,2000);
}
function renderSwitcher(){
  const el=document.getElementById('switcher');if(!el)return;
  el.innerHTML='<div class="row" style="margin-bottom:14px;gap:8px;flex-wrap:wrap">'
    +'<button class="ghost" onclick="curServer=null;showGrid()">'+T('总览')+'</button>'
    +'<span id="swlist" class="row" style="gap:6px;flex-wrap:wrap"></span></div>';
  refreshSwitcher();
}
async function refreshSwitcher(){
  const sl=document.getElementById('swlist');if(!sl)return;
  try{const data=await api('/api/servers');const servers=data.servers||[];
    sl.innerHTML=servers.map(s=>`<button class="swbtn ${s.name===curServer?'on':''}" data-name="${esc(s.name)}" onclick="openServer('${esc(s.name)}')"><span class="sdot ${s.running?'run':''}"></span>${esc(s.name)}</button>`).join('');
  }catch(e){}
}
// 用已知服务器列表渲染顶部切换器（无额外请求）。当前项高亮由 curServer 决定。
function renderSwitcherList(servers){
  const sl=document.getElementById('swlist');if(!sl)return;
  sl.innerHTML=(servers||[]).map(s=>{
    const on=(s.name===curServer)?' on':'';
    return '<button class="swbtn'+on+'" data-name="'+esc(s.name)+'" onclick="openServer(\''+esc(s.name)+'\')"><span class="sdot '+(s.running?'run':'')+'"></span>'+esc(s.name)+'</button>';
  }).join('');
}
function actCard(a,name){
  api('/api/servers/'+encodeURIComponent(name)+'/'+a).then(()=>{toast(a+' ok');showGrid();}).catch(e=>toast(T('失败:')+e.message));
}
async function refreshServer(){
  if(!curServer)return;const el=document.getElementById('sdetail');if(!el)return;
  let d;try{d=await api('/api/servers/'+encodeURIComponent(curServer));}catch(e){return;}
  const running=d.running;
  const players=(d.players||[]);
  const phtml = players.length ? players.map(p=>`<div class="item"><div class="meta"><div class="t">${esc(p)}</div></div><button class="pa" onclick="pa('op','${esc(p)}')">OP</button><button class="pa" onclick="pa('deop','${esc(p)}')">${T('取消OP')}</button><button class="pa" onclick="pa('kick','${esc(p)}')">${T('踢出')}</button><button class="pa" onclick="pa('ban','${esc(p)}')">${T('封禁')}</button></div>`).join('') : '<span class="muted">'+T('无在线玩家')+'</span>';
  const qhtml = QUICK.map(q=>`<button class="qk" onclick="runQuickWeb('${esc(q.cmd)}')">${esc(T(q.label))}</button>`).join('');
  el.innerHTML=`<div class="card">
    <div class="row" style="justify-content:space-between">
      <div><span class="badge ${running?'':'off'}">${running?T('运行中'):T('已停止')}</span> <span class="tag">${esc(d.version)} · ${esc(d.type)}</span></div>
      <div class="row">
        ${running?'':`<button class="runbtn" onclick="actServer('start')">${T('启动')}</button>`}
        ${running?`<button class="stopbtn" onclick="actServer('stop')">${T('停止')}</button><button class="stopbtn force" onclick="actServer('forcestop')">${T('强制停止')}</button>`:''}
        <button class="ghost" onclick="showProps()">${T('属性')}</button>
        <button class="del" onclick="deleteServer('${esc(curServer)}')">${T('删除服务器')}</button>
      </div>
    </div>
    <div class="tag" style="margin-top:6px">目录：${esc(d.path)}</div>
    <div style="margin-top:10px"><span class="muted">${T('在线玩家：')}</span></div>
    <div style="margin-top:6px">${phtml}</div>
    <div class="row" style="margin-top:12px">
      <button class="ghost" onclick="showWorld()">${T('世界管理')}</button>
      <button class="ghost" onclick="showMods()">${T('查看 Mods')} (${ (d.mods||[]).length })</button>
      <button class="ghost" onclick="showPlayers()">${T('玩家管理')}</button>
    </div>
    <div class="row" style="margin-top:12px;align-items:center">
      <span class="muted">${T('快捷操作')}</span>
      <div class="row" style="flex-wrap:wrap;gap:6px;margin-left:6px">${qhtml}</div>
      <button class="ghost" style="margin-left:auto" onclick="showCmdSet()">${T('指令集')}</button>
    </div>
    <div class="row" style="margin-top:12px">
      <input id="cmd" placeholder="输入指令，如 /op Steve" style="flex:1"/>
      <button onclick="sendCmd()">${T('发送')}</button>
    </div>
    <div class="console" id="console" style="margin-top:10px"></div>
    <div id="extra"></div>
  </div>`;
  // 直接复用本接口已返回的 console 字段刷新控制台，省去一次独立的 /console 轮询请求
  const c=document.getElementById('console');if(c){c.innerHTML=renderConsole(d.console||'');c.scrollTop=c.scrollHeight;}
}
// 控制台刷新已合并到 refreshServer()：直接复用 /api/servers 详情已返回的 console 字段，省去一次独立 /console 轮询。
function renderConsole(text){
  const lines=(text||'').split('\n');
  return lines.map(l=>{if(!l.length)return '';return '<span class="'+logClass(l)+'">'+esc(l)+'</span>';}).join('\n');
}
function logClass(line){
  if(/\[(ERROR|SEVERE|CRITICAL|FATAL)\]/i.test(line)||/\b(ERROR|SEVERE|CRITICAL|FATAL|EXCEPTION|THROWABLE|NULLPOINTER|FAILED|STACKOVERFLOW)\b/i.test(line))return 'lg-red';
  if(/\b(WARN|WARNING|DEPRECAT)/i.test(line))return 'lg-orange';
  if(/\b(DEBUG|TRACE|FINE|VERBOSE)\b/i.test(line))return 'lg-blue';
  if(/\b(STARTING|PREPARING|LOADING|STOPPING|SAVING|ENABLED|VERSION)\b/i.test(line))return 'lg-yellow';
  if(/\b(DONE|SUCCESS|SAVED|STARTED|JOINED)\b/i.test(line))return 'lg-green';
  return 'lg-white';
}
function actServer(a){if(!curServer)return;api('/api/servers/'+encodeURIComponent(curServer)+'/'+a).then(()=>{toast(a+' ok');refreshServer();refreshServers();}).catch(e=>toast(T('失败:')+e.message));}
function sendCmd(){if(!curServer)return;const v=document.getElementById('cmd').value;if(!v)return;
  api('/api/servers/'+encodeURIComponent(curServer)+'/send?cmd='+encodeURIComponent(v)).then(()=>{document.getElementById('cmd').value='';}).catch(e=>toast(T('失败:')+e.message));}
// 生成属性表单 HTML（卡片弹窗与详情页共用），避免重复实现
function buildPropsForm(keys, p){
  if(!keys.length) return '<p class="muted">'+T('暂无配置项')+'</p>';
  let html='<div id="props-form">';
  keys.forEach(k=>{
    const v=p[k];const isBool=(v==='true'||v==='false');const pLabel=propLabel(k);
    html+=`<div class="prop-row"><span class="prop-key" title="${esc(k)}">${esc(pLabel)} (${esc(k)})</span>`;
    if(isBool){
      const ch=v==='true';
      html+=`<label class="toggle-wrap"><input type="checkbox" class="prop-input" data-key="${esc(k)}" ${ch?'checked':''}><span class="slider"></span><span class="sval">${ch?'true':'false'}</span></label>`;
    } else {
      html+=`<input class="prop-input text" data-key="${esc(k)}" value="${esc(v)}"/>`;
    }
    html+='</div>';
  });
  html+='</div>';
  return html;
}
// 绑定布尔开关的 true/false 文本切换（属性表单渲染后调用，root 可限定作用域）
function bindPropToggles(root){
  (root||document).querySelectorAll('#props-form .toggle-wrap input').forEach(cb=>{
    cb.addEventListener('change',function(){
      const s=this.parentElement.querySelector('.sval');
      if(s)s.textContent=this.checked?'true':'false';
    });
  });
}
// 详情页内联展示属性（写入 detailExtra）
async function showProps(){
  const d=await api('/api/servers/'+encodeURIComponent(curServer));
  const p=d.properties||{};const keys=Object.keys(p).sort();
  let html='<hr><h3>'+T('服务端属性')+'</h3>'+buildPropsForm(keys,p);
  html+=`<div class="row" style="margin-top:14px;justify-content:flex-end"><button class="primary" onclick="saveProps()">${T('保存')}</button></div>`;
  document.getElementById('detailExtra').innerHTML=html;
  bindPropToggles();
}
// 从服务器卡片直接打开属性弹窗（不进入服务器详情页）：name 指定服务器，缺省用当前 curServer
async function showPropsModal(name){
  if(name) curServer=name;
  if(!curServer)return;
  const ov=document.getElementById('props');
  let html='<div class="modal"><div class="modal-h"><b>'+esc(curServer)+' · '+T('服务端属性')+'</b><button onclick="closeProps()">✕</button></div><div class="modal-b">';
  try{
    const d=await api('/api/servers/'+encodeURIComponent(curServer));
    const p=d.properties||{};const keys=Object.keys(p).sort();
    html+=buildPropsForm(keys,p);
  }catch(e){
    html+='<p class="err">'+T('加载失败')+': '+esc(e.message)+'</p>';
  }
  html+='</div><div class="modal-f" style="display:flex;justify-content:flex-end"><button class="primary" onclick="saveProps()">'+T('保存')+'</button></div></div>';
  ov.innerHTML=html;ov.style.display='flex';
  bindPropToggles(ov);
}
function closeProps(){const ov=document.getElementById('props');ov.style.display='none';ov.innerHTML='';}
async function saveProps(){
  const form=document.getElementById('props-form');if(!form)return;
  const props={};
  form.querySelectorAll('.prop-input').forEach(el=>{
    if(el.type==='checkbox') props[el.dataset.key]=el.checked?'true':'false';
    else props[el.dataset.key]=el.value;
  });
  try{
    await api('/api/servers/'+encodeURIComponent(curServer)+'/properties',{
      method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify(props)
    });
    toast(T('属性已保存'));
    const sd=await api('/api/servers/'+encodeURIComponent(curServer));
    if(sd.running) api('/api/servers/'+encodeURIComponent(curServer)+'/send?cmd=reload').catch(()=>{});
  }catch(e){toast(T('保存失败:')+e.message);}
}
async function showMods(){const d=await api('/api/servers/'+encodeURIComponent(curServer));const m=d.mods||[];
  document.getElementById('detailExtra').innerHTML=`<hr><h3>${T('Mods')} (${m.length})</h3>`+(m.map(x=>`<div class="item"><div class="meta"><div class="t">${esc(x.name||x.file)}</div><div class="s">${esc(x.version||'')}</div></div></div>`).join('')||'<p class="muted">'+T('无')+'</p>');}

// ---------------- 指令集 / 快捷操作 ----------------
function runQuickWeb(cmd){
  if(!curServer)return;
  if(cmd.indexOf('<')>=0||cmd.indexOf('[')>=0){document.getElementById('cmd').value=cmd;toast(T('已填入输入框'));return;}
  api('/api/servers/'+encodeURIComponent(curServer)+'/send?cmd='+encodeURIComponent(cmd)).then(()=>toast(T('已发送：')+cmd)).catch(e=>toast(T('失败：')+e.message));
}
function showCmdSet(){
  const ov=document.getElementById('cmdset');ov.style.display='flex';
  let html='<div class="modal"><div class="modal-h"><b>'+T('指令集')+'</b><button onclick="closeCmdSet()">✕</button></div><div class="modal-b">';
  CMDCAT.forEach(cat=>{
    html+='<div class="cat-title">'+esc(T(cat.cat))+'</div><div class="cmdgrid">';
    cat.items.forEach(it=>{
      html+='<button class="cmdc" onclick="pickCmd(\''+esc(it.c).replace(/'/g,'')+'\')"><span class="cc">'+esc(it.c)+'</span><span class="cd">'+esc(T(it.d))+'</span></button>';
    });
    html+='</div>';
  });
  html+='</div><div class="modal-f"><span class="muted">'+T('点击指令以填入输入框')+'</span></div></div>';
  ov.innerHTML=html;
}
function closeCmdSet(){const ov=document.getElementById('cmdset');ov.style.display='none';ov.innerHTML='';}
function pickCmd(c){const i=document.getElementById('cmd');if(i)i.value=c;closeCmdSet();}

// ---------------- 玩家管理 ----------------
function pa(a,n){if(!curServer)return;
  api('/api/servers/'+encodeURIComponent(curServer)+'/send?cmd='+encodeURIComponent(a+' '+n)).then(()=>{toast(a+' '+n);if(curServer)refreshServer();}).catch(e=>toast(T('失败：')+e.message));}
// 玩家管理弹窗：列出在线玩家并提供 OP / 取消OP / 踢出 / 封禁 操作
async function showPlayers(){
  const ov=document.getElementById('players');ov.style.display='flex';
  let players=[];
  try{const d=await api('/api/servers/'+encodeURIComponent(curServer));players=d.players||[];}catch(e){}
  let html='<div class="modal"><div class="modal-h"><b>'+T('玩家管理')+'</b><button onclick="closePlayers()">✕</button></div><div class="modal-b">';
  if(!players.length){
    html+='<p class="muted">'+T('无在线玩家')+'</p>';
  }else{
    players.forEach(p=>{
      html+='<div class="item"><div class="meta"><div class="t">'+esc(p)+'</div></div>'
        +'<button class="pa" onclick="pa(\'op\',\''+esc(p)+'\')">OP</button>'
        +'<button class="pa" onclick="pa(\'deop\',\''+esc(p)+'\')">'+T('取消OP')+'</button>'
        +'<button class="pa" onclick="pa(\'kick\',\''+esc(p)+'\')">'+T('踢出')+'</button>'
        +'<button class="pa" onclick="pa(\'ban\',\''+esc(p)+'\')">'+T('封禁')+'</button></div>';
    });
  }
  html+='</div><div class="modal-f"><span class="muted">'+T('点击指令以对玩家执行操作')+'</span></div></div>';
  ov.innerHTML=html;
}
function closePlayers(){const ov=document.getElementById('players');ov.style.display='none';ov.innerHTML='';}

// ---------------- 世界管理 ----------------
function showWorld(){
  const ov=document.getElementById('world');ov.style.display='flex';
  ov.innerHTML=`<div class="modal"><div class="modal-h"><b>${T('世界管理')}</b><button onclick="closeWorld()">✕</button></div>
    <div class="modal-b">
      <div class="row"><button onclick="queryWorldWeb()">${T('刷新状态')}</button><span class="muted" id="wstatemsg"></span></div>
      <div class="worldstate" id="wstate">—</div>
      <div class="cat-title">${T('快捷命令')}</div>
      <div class="row" style="flex-wrap:wrap;gap:6px" id="wquick"></div>
      <div class="cat-title">${T('世界配置（server.properties）')}</div>
      <div id="wcfg"></div>
    </div></div>`;
  const wq=document.getElementById('wquick');
  [['白天','time set day'],['夜晚','time set night'],['放晴','weather clear'],['下雨','weather rain'],['雷暴','weather thunder'],['和平','difficulty peaceful'],['简单','difficulty easy'],['普通','difficulty normal'],['困难','difficulty hard'],['保存','save-all'],['重载','reload']].forEach(t=>{const b=document.createElement('button');b.className='qk';b.textContent=T(t[0]);b.onclick=()=>sendWorldWeb(t[1]);wq.appendChild(b);});
  api('/api/servers/'+encodeURIComponent(curServer)).then(d=>{const p=d.properties||{};const keys=Object.keys(p).sort();
    document.getElementById('wcfg').innerHTML=keys.length?keys.map(k=>`<div class="item"><div class="meta"><div class="t">${esc(k)}</div></div><div class="s">${esc(p[k])}</div></div>`).join(''):'<p class="muted">'+T('无')+'</p>';}).catch(()=>{});
  queryWorldWeb();
}
function closeWorld(){const ov=document.getElementById('world');ov.style.display='none';ov.innerHTML='';}
function queryWorldWeb(){
  if(!curServer)return;
  document.getElementById('wstate').innerHTML='<span class="muted">'+T('读取中…')+'</span>';
  ['difficulty','time query daytime','weather','seed'].forEach(c=>api('/api/servers/'+encodeURIComponent(curServer)+'/send?cmd='+encodeURIComponent(c)).catch(()=>{}));
  setTimeout(parseWorldWeb,1200);
}
function parseWorldWeb(){
  if(!curServer)return;
  api('/api/servers/'+encodeURIComponent(curServer)+'/console').then(d=>{
    const lines=(d.console||'').split('\n');const st={};
    lines.forEach(line=>{let m;
      if((m=/(?:set difficulty to|difficulty):\s*([a-z]+)/i.exec(line)))st['难度']=m[1];
      else if((m=/the time is\s*(\d+)/i.exec(line)))st['时间(daytime)']=m[1];
      else if((m=/weather state:\s*([a-z]+)/i.exec(line)))st['天气']=m[1];
      else if((m=/seed:\s*\[?(-?\d+)\[?\]?/i.exec(line)))st['种子']=m[1];
    });
    const el=document.getElementById('wstate');if(!el)return;
    el.innerHTML=Object.keys(st).length?Object.keys(st).map(k=>`<div class="k">${esc(T(k))}</div><div class="v">${esc(st[k])}</div>`).join(''):'<span class="muted">'+T('未读取到（服务器未运行或日志非英文）')+'</span>';
  }).catch(()=>{});
}
function sendWorldWeb(c){if(!curServer)return;
  api('/api/servers/'+encodeURIComponent(curServer)+'/send?cmd='+encodeURIComponent(c)).then(()=>{toast('已发送：'+c);if(/difficulty|time|weather/.test(c))setTimeout(queryWorldWeb,1000);}).catch(e=>toast('失败：'+e.message));}

// ---------------- 下载中心 ----------------
// WebUI 有独立的 DownloadCatalog：切换分类/类型/搜索时必须先触发 refresh() 再轮询，
// 因为刷新是异步网络加载，列表不会立即变化。
let dlWant={key:'mod',type:'paper',q:''}, dlTimer=null, dlTries=0;
async function renderDownloads(){
  const el=document.getElementById('page-downloads');
  el.innerHTML=`<h2>${T('下载中心')}</h2>
    <div class="cat" id="catrow"></div>
    <div class="cat" id="typrow"></div>
    <div class="row" style="margin:8px 0">
      <input id="search" placeholder="${T('搜索…')}" style="flex:1"/>
      <button onclick="dlSearch()">${T('搜索')}</button>
      <button onclick="dlRefresh()">${T('刷新')}</button>
      <button id="cleanjava" style="display:none" onclick="dlCleanJava()">${T('清理临时 Java')}</button>
      <button class="ghost" style="margin-left:auto" onclick="openDlPop()">${T('下载任务')} (<span id="dlcount">0</span>)</button>
    </div>
    <div class="row" style="margin-bottom:10px">
      <span class="muted">${T('保存目录：')}</span><input id="savedir" style="flex:1"/>
      <button onclick="dlSetDir()">${T('设置')}</button>
    </div>
    <div id="dlist"></div><div class="msg" id="dstatus"></div>
    <div class="card" id="instCard" style="margin-top:14px;display:none">
      <h3>生成服务端（打包下载）</h3>
      <p class="sub">一次请求触发完整流程：下载 &rarr; 解析 &rarr; 打包成 .tar.gz 到下载目录（Java 直接落在 jvm-xx/bin，无多余内层）。</p>
      <div class="row"><span>版本</span><select id="iver" onchange="fillInstallLoaders()"></select></div>
      <div class="row"><span>加载器</span><select id="iloaders"></select></div>
      <div class="row"><span>标签</span><input id="ilabel" placeholder="可选，自定义服务端名" /></div>
      <div class="row"><label class="ck"><input type="checkbox" id="iaddtolist" checked /> 完成后加入服务器列表</label></div>
      <div class="row"><button class="primary" onclick="doInstallServer()">生成并打包</button>
        <button onclick="installTasksClear()">清除已完成</button></div>
      <div class="msg" id="iprog"></div>
      <div class="prog" style="margin-top:8px"><i id="ibar" style="width:0%"></i></div>
      <div id="itasks" class="tasks"></div>
    </div>
    <div id="dlpop">
      <div class="modal">
        <div class="modal-h"><b>${T('下载任务')}</b><button onclick="closeDlPop()">✕</button></div>
        <div class="modal-b" id="dlbody"></div>
        <div class="modal-f"><button class="ghost" onclick="dlTaskClear()">${T('清空已完成')}</button></div>
      </div>
    </div>`;
  const cr=document.getElementById('catrow');
  CATEGORIES.forEach(c=>{const b=document.createElement('button');b.textContent=T(c.label);b.dataset.k=c.key;b.onclick=()=>selCat(c.key);cr.appendChild(b);});
  const tr=document.getElementById('typrow');
  SERVERTYPES.forEach(c=>{const b=document.createElement('button');b.textContent=T(c.label);b.dataset.k=c.key;b.onclick=()=>selType(c.key);tr.appendChild(b);});
  selCat(dlWant.key); startTasksPoll(); // 默认进入模组分类并触发加载，并启动下载任务轮询
}
function cur(row){const a=document.querySelector('#'+row+' button.on');return a?a.dataset.k:'';}
function markOn(row,k){document.querySelectorAll('#'+row+' button').forEach(b=>b.classList.toggle('on',b.dataset.k===k));}
function selCat(k){dlWant={key:k,type:dlWant.type,q:''};markOn('catrow',k);startDlPoll();}
function selType(k){dlWant.type=k;markOn('typrow',k);startDlPoll();}
function dlSearch(){dlWant.q=document.getElementById('search').value;startDlPoll();}
function dlRefresh(){dlWant.q=document.getElementById('search').value;startDlPoll();}
// 触发刷新并开始轮询，直到该分类加载完成（loading=false）
function startDlPoll(){
  dlTries=0;
  if(dlTimer){clearInterval(dlTimer);dlTimer=null;}
  api('/api/catalog/refresh?cat='+dlWant.key+'&type='+dlWant.type+'&q='+encodeURIComponent(dlWant.q)).catch(()=>{});
  pollDl();
  dlTimer=setInterval(pollDl,400);
}
let curDl=null;
function pollDl(){
  api('/api/catalog?cat='+dlWant.key+'&type='+dlWant.type+'&q='+encodeURIComponent(dlWant.q))
  .then(d=>{
    curDl=d;
    document.getElementById('savedir').value=d.saveDir||'';
    document.getElementById('dstatus').textContent=T('状态：')+(d.status||'');
    document.getElementById('typrow').style.display=(d.currentKey==='server')?'flex':'none';
    markOn('catrow',d.currentKey);markOn('typrow',d.serverType);
    renderDlItems(d);
    const cj=document.getElementById('cleanjava'); if(cj)cj.style.display=(d.currentKey==='java')?'inline-block':'none';
    // “生成服务端（打包下载）”卡片仅在「服务端-模组服」分类下出现
    const ic=document.getElementById('instCard');
    if(ic)ic.style.display=(d.currentKey==='server'&&d.serverType==='mod')?'block':'none';
    if(d.currentKey==='server'&&d.serverType==='mod')fillInstallSelects(d);
    const anyDl=(d.items||[]).some(i=>i.downloading);
    const finished = !d.loading && d.currentKey===dlWant.key && !anyDl;
    if(finished || dlTries++>25){ if(dlTimer){clearInterval(dlTimer);dlTimer=null;} }
  }).catch(()=>{ if(dlTimer){clearInterval(dlTimer);dlTimer=null;} });
}
function renderDlItems(d){
  const list=document.getElementById('dlist');list.innerHTML='';
  if(d.loading && !(d.items||[]).length) list.innerHTML='<p class="muted">'+T('加载中…')+'</p>';
  (d.items||[]).forEach((it,i)=>{
    const div=document.createElement('div');div.className='item';
    const right = it.downloading
      ? `<div class="prog"><i style="width:${Math.round(it.percent*100)}%"></i></div><button class="iconbtn" title="${T('暂停')}" onclick="dlPause(${i})">⏸</button>`
      : it.done
        ? `<span class="pill on">${T('已下载')}</span>`
        : `<button class="iconbtn dl-go" title="${T('下载')}" onclick="dlDownload(${i})">⬇</button>`;
    const err = it.error?`<div class="s err">${esc(it.error)}</div>`:'';
    div.innerHTML=`<div class="meta"><div class="t">${esc(it.title)}</div><div class="s">${esc(it.subtitle)}</div>${err}</div><div class="row">${right}</div>`;
    list.appendChild(div);
  });
  if((d.items||[]).some(it=>it.downloading)) timers.dl=timers.dl||setInterval(pollDl,1500);
  else if(timers.dl){clearInterval(timers.dl);delete timers.dl;}
}
// ---------------- 下载任务（与本地端“下载任务”面板同源） ----------------
let dlTasksTimer=null;
function openDlPop(){ const ov=document.getElementById('dlpop'); if(ov) ov.style.display='flex'; refreshDownloads(); }
function closeDlPop(){ const ov=document.getElementById('dlpop'); if(ov) ov.style.display='none'; }
function startTasksPoll(){ if(dlTasksTimer) clearInterval(dlTasksTimer); refreshDownloads(); dlTasksTimer=setInterval(refreshDownloads,1500); }
function refreshDownloads(){
  api('/api/downloads').then(d=>{
    const list=document.getElementById('dlbody'); if(!list) return;
    const items=d.items||[];
    const c=document.getElementById('dlcount'); if(c) c.textContent=items.length;
    if(!items.length){ list.innerHTML='<p class="muted">'+T('暂无下载任务')+'</p>'; return; }
    const map={downloading:T('下载中'),done:T('已完成'),error:T('出错'),canceled:T('已取消'),paused:T('已暂停')};
    list.innerHTML=items.map((it,i)=>{
      const pct=Math.round(it.percent||0);
      let act='';
      if(it.state==='downloading') act=`<button onclick="dlTaskAct('pause',${i})">${T('暂停')}</button><button class="del" onclick="dlTaskAct('cancel',${i})">${T('取消')}</button>`;
      else if(it.state==='paused') act=`<button onclick="dlTaskAct('resume',${i})">${T('继续')}</button><button class="del" onclick="dlTaskAct('cancel',${i})">${T('取消')}</button>`;
      else if(it.state==='error') act=`<button onclick="dlTaskAct('restart',${i})">${T('重新下载')}</button><button class="del" onclick="dlTaskAct('remove',${i})">${T('移除')}</button>`;
      else act=`<button class="del" onclick="dlTaskAct('remove',${i})">${T('移除')}</button>`;
      const err=it.errorText?`<div class="dlerr">${esc(it.errorText)}</div>`:'';
      const path=(it.state==='done'&&it.path)?`<div class="dlpath">${esc(it.path)}</div>`:'';
      return `<div class="dlitem"><div class="dlrow"><span class="dltitle">${esc(it.title)}</span><span class="dlstate ${it.state}">${map[it.state]||it.state}</span></div>`
        +`<div class="dlbar ${it.state}"><i style="width:${pct}%"></i></div>${err}${path}`
        +`<div class="dlactions">${act}</div></div>`;
    }).join('');
  }).catch(()=>{});
}
function dlTaskAct(act,row){ api('/api/downloads/'+act+'?row='+row).then(refreshDownloads).catch(e=>toast(T('失败:')+e.message)); }
function dlTaskClear(){ api('/api/downloads/clear').then(refreshDownloads).catch(()=>{}); }
function dlDownload(i){api('/api/catalog/download?index='+i).then(()=>{toast(T('已开始下载'));}).catch(e=>toast(T('失败:')+e.message));
  if(curDl&&curDl.items&&curDl.items[i]&&curDl.items[i].url&&curDl.items[i].url.indexOf('java://')===0){ if(!dlTimer){pollDl();dlTimer=setInterval(pollDl,400);} }
}
function dlPause(i){api('/api/catalog/pause?index='+i).then(()=>pollDl()).catch(e=>toast(T('失败:')+e.message));}
function dlResume(i){api('/api/catalog/resume?index='+i).then(()=>pollDl()).catch(e=>toast(T('失败:')+e.message));}
function dlCancel(i){api('/api/catalog/cancel?index='+i).then(()=>pollDl()).catch(e=>toast(T('失败:')+e.message));}
// 模组服多加载器面板
function loaderCompatible(l,v){
  if(l==='fabric')return true;
  if(l==='forge')return v>='1.1';
  if(l==='neoforge'){ if(v<'1.20.1')return false; if(v==='1.20')return false; return true; }
  return false;
}

/* ===== 生成服务端（打包下载）面板：一次 API 触发完整流程，同页显示进度 ===== */
var installTimer = null;     // 进度轮询定时器
var installIds = [];         // 当前关注的任务 id 列表
var _instFillKey = '';        // 上次填充的选项指纹，避免高频轮询重复重建下拉

function fillInstallSelects(d) {
  const v = document.getElementById('iver');
  const l = document.getElementById('iloaders');
  if (!v || !l) return;
  // 选项集合未变化则跳过重建，保留用户当前选择（pollDl 每 400ms 一次，重建会打断选择）。
  const key = (d.mcReleases || []).join('|') + '#' + (d.modLoaders || []).join('|');
  if (key === _instFillKey) return;
  _instFillKey = key;
  const curVer = v.value, curLd = l.value;
  v.innerHTML = '';
  (d.mcReleases || []).forEach(function (r) {
    const o = document.createElement('option');
    o.value = r; o.textContent = r;
    v.appendChild(o);
  });
  if (curVer) v.value = curVer; else if (d.modVersion) v.value = d.modVersion;
  fillInstallLoaders(d);
  if (curLd) l.value = curLd;
}
function fillInstallLoaders(d) {
  const v = document.getElementById('iver');
  const l = document.getElementById('iloaders');
  if (!v || !l) return;
  const ver = v.value, ld = d.modLoaders || [];
  const cur = l.value;
  l.innerHTML = '';
  ld.forEach(function (o) {
    const op = document.createElement('option');
    op.value = o; op.textContent = o;
    if (!loaderCompatible(o, ver)) { op.disabled = true; op.textContent += '（该版本不支持）'; }
    l.appendChild(op);
  });
  if (cur && loaderCompatible(cur, ver)) l.value = cur;
}

function doInstallServer() {
  const type = 'mod'; // 此卡片仅在模组服分类下显示，类型固定为 mod
  const version = document.getElementById('iver').value.trim();
  const label = document.getElementById('ilabel').value.trim();
  const addToList = document.getElementById('iaddtolist').checked;
  let loaders = [];
  const ldv = document.getElementById('iloaders').value;
  if (ldv) loaders = [ldv];
  if (!version) { installMsg('请选择版本', true); return; }
  if (!loaders.length) { installMsg('请选择加载器（forge / fabric / neoforge）', true); return; }
  installMsg('提交中…');
  api('/api/installserver', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ type, version, loaders, label, addToList })
  })
    .then(r => {
      const id = r && r.taskId ? r.taskId : null;
      if (!id) { installMsg('未返回任务 ID：' + JSON.stringify(r), true); return; }
      if (installIds.indexOf(id) < 0) installIds.push(id);
      installMsg('已提交任务 ' + id + '，开始打包…');
      if (!installTimer) installTimer = setInterval(pollInstallTasks, 1000);
      pollInstallTasks();
    })
    .catch(e => installMsg('提交失败：' + (e && e.message ? e.message : e), true));
}

function installMsg(m, bad) {
  const el = document.getElementById('iprog');
  el.textContent = m || '';
  el.className = 'msg' + (bad ? ' err' : '');
}

function pollInstallTasks() {
  api('/api/installtasks').then(res => {
    const arr = (res && res.tasks) ? res.tasks : (Array.isArray(res) ? res : []);
    const tasks = arr.filter(t => installIds.indexOf(t.id) >= 0);
    renderInstallTasks(tasks);
    // 所有关注任务均已完成：停止轮询并清理关注列表，避免持续轮询历史任务
    if (tasks.length === installIds.length && tasks.every(t => t.finished)) {
      clearInterval(installTimer); installTimer = null;
      for (const t of tasks) { const i = installIds.indexOf(t.id); if (i >= 0) installIds.splice(i, 1); }
    }
  }).catch(() => {});
}

function renderInstallTasks(tasks) {
  const box = document.getElementById('itasks');
  if (!tasks.length) { box.innerHTML = ''; return; }
  box.innerHTML = tasks.map(t => {
    const pct = Math.max(0, Math.min(100, Math.round((t.progress || 0) * 100)));
    const cls = t.finished ? (t.ok ? 'ok' : 'err') : '';
    const head = (t.label ? esc(t.label) + ' ' : '') + '<span class="muted">' + esc(t.type) + ' ' + esc(t.version) + (t.loaders && t.loaders.length ? ' ' + esc(t.loaders.join(',')) : '') + '</span>';
    const stage = t.stage ? esc(t.stage) + (t.stageProgress ? ' ' + Math.round(t.stageProgress * 100) + '%' : '') : '';
    const status = esc(t.status || '');
    const msg = t.message ? '<div class="muted">' + esc(t.message) + '</div>' : '';
    return '<div class="task ' + cls + '"><div class="task-h">' + head +
      (t.finished ? '<span class="badge">' + (t.ok ? '完成' : '失败') + '</span>' : '<span class="badge run">进行中</span>') +
      '</div>' +
      '<div class="prog"><i style="width:' + pct + '%"></i></div>' +
      (stage ? '<div class="muted">' + stage + '</div>' : '') +
      (status ? '<div class="muted">' + status + '</div>' : '') + msg +
      '</div>';
  }).join('');
  // 用列表中第一个未完成任务刷新顶部总进度条
  const cur = tasks.find(t => !t.finished) || tasks[tasks.length - 1];
  if (cur) {
    const pct = Math.max(0, Math.min(100, Math.round((cur.progress || 0) * 100)));
    document.getElementById('ibar').style.width = pct + '%';
    if (!cur.finished) installMsg(cur.label ? (cur.label + '：' + (cur.stage || cur.status || '处理中') + ' ' + pct + '%') : ('进度 ' + pct + '%'));
  }
}

function installTasksClear() {
  api('/api/installtasks').then(res => {
    const arr = (res && res.tasks) ? res.tasks : (Array.isArray(res) ? res : []);
    arr.forEach(t => { if (t.finished && installIds.indexOf(t.id) >= 0) installIds.splice(installIds.indexOf(t.id), 1); });
    const ids = arr.filter(t => t.finished).map(t => t.id);
    if (ids.length) api('/api/installtasks?clear=1', { method: 'DELETE' }).then(() => { document.getElementById('itasks').innerHTML=''; document.getElementById('ibar').style.width='0%'; }).catch(()=>{});
  }).catch(()=>{});
}

function dlCleanJava(){api('/api/catalog/cleantempjava').then(()=>toast(T('已清理临时 Java'))).catch(e=>toast(T('失败:')+e.message));}
function dlSetDir(){const v=document.getElementById('savedir').value;api('/api/catalog/savedir?dir='+encodeURIComponent(v)).then(dlRefresh).catch(e=>toast(T('失败:')+e.message));}

// ---------------- 创建服务器 ----------------
async function renderCreate(){
  const el=document.getElementById('page-create');
  el.innerHTML=`<h2>${T('创建服务器')}</h2>
    <div class="card">
      <div class="row" style="margin-bottom:10px">
        <label class="ck"><input type="radio" name="cmode" value="new" checked onclick="toggleCreateMode('new')"/> ${T('全新创建')}</label>
        <label class="ck" style="margin-left:14px"><input type="radio" name="cmode" value="zip" onclick="toggleCreateMode('zip')"/> ${T('从压缩包导入')}</label>
      </div>
      <div id="newArea">
        <div class="row" style="margin-bottom:10px"><span>${T('类型')}</span><select id="ctype"></select></div>
        <div class="row" style="margin-bottom:10px"><span>${T('版本')}</span><select id="cver"></select></div>
        <div id="loaderArea" style="display:none;margin-bottom:10px"><span class="muted">${T('加载器')}</span><div id="loaderList" style="margin-top:4px"></div></div>
        <div class="row" style="margin-bottom:10px"><span>${T('内存(MB)')}</span><input id="cmem" type="number" value="2048" min="512" style="flex:1"/></div>
      </div>
      <div id="zipArea" style="display:none;margin-bottom:10px">
        <div class="row" style="margin-bottom:10px"><span>${T('压缩包(.zip)')}</span><input id="czip" placeholder="D:/xxx.zip" style="flex:1"/></div>
      </div>
      <div class="row" style="margin-bottom:10px"><span>${T('名称')}</span><input id="cname" placeholder="${T('自动生成')}" style="flex:1" oninput="onNameEdit()"/></div>
      <div class="row" style="margin-bottom:10px"><span>${T('目录')}</span><input id="cdir" placeholder="${T('自动生成')}" style="flex:1"/></div>
      <div class="row" style="margin-bottom:10px"><span>${T('Java 目录(可选)')}</span><input id="cjava" placeholder="${T('留空使用自动探测')}" style="flex:1"/></div>
      <label class="ck" style="margin:8px 0"><input type="checkbox" id="ceula"/> ${T('我已阅读并同意 Minecraft EULA')}</label>
      <div id="javaInfo" class="msg" style="margin:6px 0"></div>
      <div class="row" style="margin-top:8px"><button class="primary" onclick="doCreate()">${T('创建并安装')}</button></div>
      <div id="cprog" class="msg"></div>
      <div class="prog" style="margin-top:8px;width:100%"><i id="cbar" style="width:0%"></i></div>
    </div>`;
  toggleCreateMode('new');
  const ctype=document.getElementById('ctype');
  (await api('/api/createtypes')).types.forEach(t=>{const o=document.createElement('option');o.value=t;o.textContent=t;ctype.appendChild(o);});
  ctype.onchange=()=>loadVers();
  loadVers();
}
function toggleCreateMode(m){
  const na=document.getElementById('newArea'),za=document.getElementById('zipArea');
  if(na)na.style.display=(m==='new')?'block':'none';
  if(za)za.style.display=(m==='zip')?'block':'none';
}
function onNameEdit(){
  const inp=document.getElementById('cname');
  if(inp) inp.dataset.userEdited='1';
}
async function refreshCreateState(){
  const d=await api('/api/create/progress');
  const ni=document.getElementById('cname'), di=document.getElementById('cdir');
  if(d.name && ni && !ni.dataset.userEdited) ni.value=d.name;
  if(d.saveDir && di) di.value=d.saveDir;
  const ji=document.getElementById('javaInfo');
  if(ji && d.java!=null){
    ji.textContent = d.javaFound
      ? ('Java '+d.java+' ✓ ('+d.javaPath+')')
      : ('Java '+d.java+' — '+T('创建后自动检查/安装'));
  }
  // 加载器
  const la=document.getElementById('loaderArea'), ll=document.getElementById('loaderList');
  if(la && ll && d.modLoaders){
    if(document.getElementById('ctype').value==='mod'){
      la.style.display='block';
      ll.innerHTML=d.modLoaders.map(l=>`<label class="ck" style="margin-right:12px"><input type="checkbox" class="lc" data-loader="${esc(l.key)}" ${l.compatible===false?'disabled':''}/> ${esc(l.label)}</label>`).join('');
    }else{la.style.display='none';ll.innerHTML='';}
  }
}
async function loadVers(){
  const t=document.getElementById('ctype').value;
  api('/api/create/loadversions?type='+encodeURIComponent(t)).catch(()=>{});
  pollVers();
  setTimeout(refreshCreateState,500);
}
function pollVers(){const t=document.getElementById('ctype')?.value;
  api('/api/versions?type='+encodeURIComponent(t||'')).then(d=>{
    const sel=document.getElementById('cver');const cur=sel.value;sel.innerHTML='';
    (d.versions||[]).forEach(v=>{const o=document.createElement('option');o.value=v;o.textContent=v;sel.appendChild(o);});
    if(cur)sel.value=cur;
    sel.onchange=()=>{const ni=document.getElementById('cname');if(ni&&!ni.dataset.userEdited)refreshCreateState();};
    if((d.versions||[]).length===0) timers.vers=timers.vers||setInterval(pollVers,1500);
    else {
      if(timers.vers){clearInterval(timers.vers);delete timers.vers;}
      refreshCreateState();
    }
  }).catch(()=>{});
}
function doCreate(){
  const mode=(document.querySelector('input[name=cmode]:checked')||{}).value||'new';
  // 可选 Java 目录（影响全局探测），先写回 JavaManager
  const javaDir=document.getElementById('cjava').value.trim();
  if(javaDir)fetch('/api/java/sethome?dir='+encodeURIComponent(javaDir),{method:'GET'}).catch(()=>{});
  if(mode==='zip'){
    const body={importMode:true,zipPath:document.getElementById('czip').value,
      name:document.getElementById('cname').value,saveDir:document.getElementById('cdir').value,eulaAccepted:document.getElementById('ceula').checked};
    fetch('/api/create',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}).then(()=>{toast(T('已提交'));timers.create=setInterval(pollCreate,1200);pollCreate();}).catch(e=>toast(T('失败:')+e.message));
    return;
  }
  const body={type:document.getElementById('ctype').value,version:document.getElementById('cver').value,
    name:document.getElementById('cname').value,saveDir:document.getElementById('cdir').value,eulaAccepted:document.getElementById('ceula').checked,
    memory:parseInt(document.getElementById('cmem').value||'2048',10)};
  if(document.getElementById('ctype').value==='mod'){
    body.selectedLoaders=[].map.call(document.querySelectorAll('.lc:checked'),cb=>cb.dataset.loader);
  }
  fetch('/api/create',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}).then(()=>{toast(T('已提交'));timers.create=setInterval(pollCreate,1200);pollCreate();}).catch(e=>toast(T('失败:')+e.message));
}
function pollCreate(){
  api('/api/create/progress').then(d=>{
    document.getElementById('cprog').textContent=d.status||'';
    document.getElementById('cbar').style.width=Math.round(d.progress*100)+'%';
    const ji=document.getElementById('javaInfo');
    if(ji && d.java!=null){
      ji.textContent = d.javaFound
        ? ('Java '+d.java+' ✓ ('+d.javaPath+')')
        : ('Java '+d.java+' — '+T('创建后自动检查/安装'));
    }
    if(d.done&&timers.create){clearInterval(timers.create);delete timers.create;toast(T('安装完成'));}
  }).catch(()=>{});
}

// ---------------- 导入整合包 ----------------
async function renderImport(){
  const el=document.getElementById('page-import');
  el.innerHTML=`<h2>${T('导入整合包')}</h2>
    <div class="card">
      <div class="row" style="margin-bottom:10px"><span>${T('整合包(.zip)')}</span><input id="izip" placeholder="D:/xxx.zip" style="flex:1"/></div>
      <div class="row" style="margin-bottom:10px"><span>${T('目标目录')}</span><input id="idir" placeholder="D:/Servers/xxx" style="flex:1"/></div>
      <div class="row"><button class="primary" onclick="doImport()">${T('导入')}</button></div>
      <div id="iprogress" class="msg"></div>
      <div class="prog" style="margin-top:8px;width:100%"><i id="ibar" style="width:0%"></i></div>
    </div>`;
  timers.imp=setInterval(pollImport,1200);pollImport();
}
function doImport(){
  const body={zipPath:document.getElementById('izip').value,targetDir:document.getElementById('idir').value};
  fetch('/api/import',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}).then(()=>toast(T('已提交'))).catch(e=>toast(T('失败:')+e.message));
}
function pollImport(){
  api('/api/import/progress').then(d=>{
    document.getElementById('iprogress').textContent=d.status||'';
    document.getElementById('ibar').style.width=Math.round(d.progress*100)+'%';
    const info=(d.modpackName?'名称：'+d.modpackName+'　':'')+(d.loader?'加载器：'+d.loader+'　':'')+(d.gameVersion?'版本：'+d.gameVersion:'');
    if(info)document.getElementById('iprogress').textContent+='　'+info;
    if(d.done&&timers.imp){clearInterval(timers.imp);delete timers.imp;toast(T('导入完成'));}
  }).catch(()=>{});
}

// ---------------- 设置 ----------------
async function renderSettings(){
  const el=document.getElementById('page-settings');
  let s;try{s=await api('/api/settings');}catch(e){s={};}
  let t;try{t=await api('/api/theme');theme=t;}catch(e){}
  applyTheme();
  el.innerHTML=`<h2>${T('应用设置')}</h2>
    <div class="card">
      <h3>${T('外观')}</h3>
      <label class="ck"><input type="checkbox" id="dark" ${theme.dark?'checked':''}/> ${T('深色模式')}</label>
      <div class="row" style="margin-top:12px"><span>${T('主题色')}</span>
        <div class="swatches" id="sw"></div>
        <input type="color" id="custom" value="${theme.accent}"/>
      </div>
    </div>
    <div class="card">
      <h3>${T('语言与自启')}</h3>
      <div class="row"><span>${T('语言')}</span><select id="lang"><option value="简体中文">简体中文</option><option value="English">English</option></select></div>
      <label class="ck" style="margin-top:10px"><input type="checkbox" id="autostart"/> ${T('开机自启')}</label>

    </div>
    <div class="card">
      <h3>${T('目录')}</h3>
      <div class="row"><span>${T('默认服务器目录')}</span><input id="defdir" style="flex:1" placeholder="${T('留空使用程序默认')}"/></div>
    </div>
    <div class="card">
      <h3>${T('QQ机器人')}</h3>
      <div class="muted" style="margin-bottom:6px">${T('设置与本地端一致；"运行状态"指 NapCat / NoneBot 程序进程本身')}</div>
      <label class="ck"><input type="checkbox" id="botLinkedStart"/> ${T('联动启动')}</label>
      <label class="ck" style="margin-top:8px"><input type="checkbox" id="botEnabled"/> ${T('启用QQ机器人')}</label>
      <div class="row" style="margin-top:10px"><span>${T('NapCat 路径')}</span><input id="botNapcatPath" style="flex:1" placeholder="${T('留空则自动探测')}"/></div>
      <div class="row" style="margin-top:6px"><span>${T('运行状态')}</span><span id="napcatState" class="muted">—</span></div>
      <div class="row" style="margin-top:10px"><span>${T('NoneBot 目录')}</span><input id="botNonebotDir" style="flex:1" placeholder="${T('留空则自动探测')}"/></div>
      <div class="row" style="margin-top:6px"><span>${T('运行状态')}</span><span id="nonebotState" class="muted">—</span></div>
      <div class="row" style="margin-top:10px"><span>${T('msm 控制插件')}</span><span id="msmPluginState" class="muted">—</span></div>
      <div class="row" style="margin-top:10px"><span>${T('昵称状态更新间隔(秒)')}</span><input id="botUsageInterval" type="number" min="0" max="86400" style="width:110px"/><span class="muted">${T('0=关闭')}</span></div>
    </div>
    <div class="row" style="margin-top:10px"><button class="primary" onclick="saveSettings()">${T('保存设置')}</button></div>
    <div class="msg">${T('提示：WebUI 自身的"启用/端口"设置不会在此显示，避免从面板内部把自己关掉。')}</div>`;
  document.getElementById('lang').value=s.language||'简体中文';
  curLang=s.language==='English'?'en':'zh';
  document.getElementById('autostart').checked=!!s.autoStart;

  const dd=document.getElementById('defdir');if(dd)dd.value=s.defaultServerDir||'';
  const sw=document.getElementById('sw');
  ACCENTS.forEach(c=>{const d=document.createElement('div');d.className='swatch'+(c.toLowerCase()===theme.accent.toLowerCase()?' on':'');d.style.background=c;d.onclick=()=>setAccent(c);sw.appendChild(d);});
  document.getElementById('custom').oninput=e=>setAccent(e.target.value);
  document.getElementById('dark').onchange=e=>{theme.dark=e.target.checked;applyTheme();pushTheme();};

  const botPost=async body=>{ try{ await api('/api/bot',{method:'POST',body:JSON.stringify(body)}); }catch(e){} loadBotState(); };
  const be=document.getElementById('botEnabled');
  if(be) be.onchange=()=>botPost({enabled:be.checked});
  const bl=document.getElementById('botLinkedStart');
  if(bl) bl.onchange=()=>botPost({linkedStart:bl.checked});
  const bp=document.getElementById('botNapcatPath');
  if(bp) bp.onchange=()=>botPost({napcatPath:bp.value});
  const bd=document.getElementById('botNonebotDir');
  if(bd) bd.onchange=()=>botPost({nonebotDir:bd.value});
  const bi=document.getElementById('botUsageInterval');
  if(bi) bi.onchange=()=>botPost({usageInterval:Math.max(0,Math.min(86400,Number(bi.value)||0))});
  loadBotState();
  if(!timers.bot) timers.bot=setInterval(()=>{ if(curTab==='settings') loadBotState(); },3000);
}
function setAccent(c){theme.accent=c;applyTheme();document.querySelectorAll('.swatch').forEach(s=>s.classList.toggle('on',s.style.background.includes(c)));pushTheme();}
function pushTheme(){fetch('/api/theme',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(theme)}).catch(()=>{});}
function saveSettings(){
  const langEl=document.getElementById('lang');
  const lang=langEl.value;
  curLang=(lang==='English'?'en':'zh');
  const body={language:lang,autoStart:document.getElementById('autostart').checked,defaultServerDir:document.getElementById('defdir').value};
  fetch('/api/settings',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}).then(()=>{
    toast(T('已保存'));
    // 重新渲染当前页与导航，使全局语言立即生效
    applyLangUI();
    if(document.getElementById('props-form')) showProps();
  }).catch(e=>toast(T('失败:')+e.message));
}
// 运行状态映射：与本地端一致（指 NapCat/NoneBot 程序进程本身）
function botStateText(s,enabled){
  if(!enabled) return T('未启用');
  const m={running:'运行中',starting:'启动中',external:'外部运行（由你自行启动）',waiting:'等待连接',stopped:'已停止'};
  return T(m[s]||s||'已停止');
}
function setBotStateEl(id,s,enabled){
  const el=document.getElementById(id); if(!el)return;
  el.textContent=botStateText(s,enabled);
  el.style.color=(enabled&&s==='running')?'#69db7c':'var(--text-muted)';
}
async function loadBotState(){
  try{
    const b=await api('/api/bot');
    const be=document.getElementById('botEnabled'); if(be)be.checked=!!b.botEnabled;
    const bl=document.getElementById('botLinkedStart'); if(bl)bl.checked=!!b.botLinkedStart;
    const bp=document.getElementById('botNapcatPath'); if(bp&&document.activeElement!==bp)bp.value=b.napcatPath||'';
    const bd=document.getElementById('botNonebotDir'); if(bd&&document.activeElement!==bd)bd.value=b.nonebotDir||'';
    const bi=document.getElementById('botUsageInterval'); if(bi&&document.activeElement!==bi)bi.value=(b.usageInterval??300);
    setBotStateEl('napcatState',b.napcatState,!!b.botEnabled);
    setBotStateEl('nonebotState',b.nonebotState,!!b.botEnabled);
    const mp=document.getElementById('msmPluginState');
    if(mp){
      const pm={ok:'已安装',missing:'未安装（机器人将无法控制服务器）',installing:'安装中…',error:'安装失败'};
      mp.textContent=T(pm[b.msmPluginState]||'未知');
      mp.style.color=b.msmPluginState==='ok'?'#69db7c':(b.msmPluginState==='error'?'#e06c5a':'var(--text-muted)');
    }
  }catch(e){}
}

// 初始化
(async function init(){
  try{const st=await api('/api/state');theme=st.theme||theme;}catch(e){}
  try{const s=await api('/api/settings');curLang=(s.language==='English')?'en':'zh';}catch(e){}
  applyTheme();tickMon();setInterval(tickMon,3000);
  relocalizeNav();
  relocalizeMonitors();
  switchTab('servers');
})();
</script>
</body>
</html>)WEBUI";
}
