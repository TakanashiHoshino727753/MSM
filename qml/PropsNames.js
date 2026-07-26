// 服务器属性中英名称映射表
// 可直接在 QML 中 import 使用，也可在纯 JS 环境引用此对象。

.pragma library

var _current = 'zh'; // 'zh' | 'en'

var _props = {
    'accepts-transfers':         { zh: '接受传送请求',         en: 'Accept Transfers' },
    'allow-flight':              { zh: '允许飞行',            en: 'Allow Flight' },
    'enable-code-of-conduct':    { zh: '启用行为准则',         en: 'Enable Code of Conduct' },
    'allow-nether':              { zh: '允许下界',            en: 'Allow Nether' },
    'broadcast-console-to-ops':  { zh: '控制台广播给管理员',   en: 'Broadcast Console to Ops' },
    'broadcast-rcon-to-ops':     { zh: 'RCON 广播给管理员',    en: 'Broadcast RCON to Ops' },
    'bug-report-link':           { zh: 'Bug 报告链接',         en: 'Bug Report Link' },
    'difficulty':                { zh: '游戏难度',            en: 'Difficulty' },
    'enable-command-block':      { zh: '启用命令方块',         en: 'Enable Command Block' },
    'enable-jmx-monitoring':     { zh: '启用 JMX 监控',       en: 'Enable JMX Monitoring' },
    'enable-query':              { zh: '启用查询',            en: 'Enable Query' },
    'enable-rcon':               { zh: '启用 RCON',           en: 'Enable RCON' },
    'enable-status':             { zh: '启用状态',            en: 'Enable Status' },
    'enforce-secure-profile':    { zh: '强制安全配置文件',     en: 'Enforce Secure Profile' },
    'enforce-whitelist':         { zh: '强制白名单',          en: 'Enforce Whitelist' },
    'entity-broadcast-range-percentage': { zh: '实体广播范围百分比', en: 'Entity Broadcast Range %' },
    'force-gamemode':            { zh: '强制游戏模式',         en: 'Force Gamemode' },
    'function-permission-level': { zh: '函数权限等级',         en: 'Function Permission Level' },
    'gamemode':                  { zh: '默认游戏模式',         en: 'Gamemode' },
    'generate-structures':       { zh: '生成结构',            en: 'Generate Structures' },
    'generator-settings':        { zh: '生成器设置',          en: 'Generator Settings' },
    'hardcore':                  { zh: '极限模式',            en: 'Hardcore' },
    'hide-online-players':       { zh: '隐藏在线玩家',         en: 'Hide Online Players' },
    'initial-disabled-packs':    { zh: '初始禁用数据包',       en: 'Initial Disabled Packs' },
    'initial-enabled-packs':     { zh: '初始启用数据包',       en: 'Initial Enabled Packs' },
    'level-name':                { zh: '世界名称',            en: 'Level Name' },
    'level-seed':                { zh: '世界种子',            en: 'Level Seed' },
    'level-type':                { zh: '世界类型',            en: 'Level Type' },
    'log-ips':                   { zh: '记录 IP',            en: 'Log IPs' },
    'pause-when-empty-seconds':  { zh: '无人时暂停(秒)',      en: 'Pause When Empty (s)' },
    'management-server-enabled': { zh: '启用管理服务器',       en: 'Management Server Enabled' },
    'management-server-host':    { zh: '管理服务器主机',       en: 'Management Server Host' },
    'management-server-port':    { zh: '管理服务器端口',       en: 'Management Server Port' },
    'management-server-secret':  { zh: '管理服务器密钥',       en: 'Management Server Secret' },
    'management-server-allowed-origins': { zh: '管理服务器允许的来源', en: 'Allowed Origins' },
    'management-server-tls-enabled': { zh: '管理服务器 TLS',   en: 'Management Server TLS' },
    'management-server-tls-keystore': { zh: 'TLS 密钥库路径',  en: 'TLS Keystore' },
    'management-server-tls-keystore-password': { zh: 'TLS 密钥库密码', en: 'TLS Keystore Password' },
    'max-chained-neighbor-updates': { zh: '最大连锁更新',     en: 'Max Chained Neighbor Updates' },
    'max-build-height':          { zh: '最大建筑高度',         en: 'Max Build Height' },
    'max-entities':              { zh: '最大实体数',          en: 'Max Entities' },
    'max-players':               { zh: '最大玩家数',          en: 'Max Players' },
    'max-tick-time':             { zh: '最大单 tick 时间(ms)', en: 'Max Tick Time' },
    'max-world-size':            { zh: '最大世界大小',         en: 'Max World Size' },
    'motd':                      { zh: '服务器描述',          en: 'MOTD' },
    'network-compression-threshold': { zh: '网络压缩阈值',     en: 'Network Compression Threshold' },
    'online-mode':               { zh: '在线验证',            en: 'Online Mode' },
    'op-permission-level':       { zh: '管理员权限等级',       en: 'OP Permission Level' },
    'player-idle-timeout':       { zh: '玩家空闲超时(分钟)',   en: 'Player Idle Timeout' },
    'previews-chat':             { zh: '预览聊天',            en: 'Previews Chat' },
    'prevent-proxy-connections': { zh: '阻止代理连接',         en: 'Prevent Proxy Connections' },
    'pvp':                       { zh: '允许 PVP',            en: 'PVP' },
    'query.port':                { zh: '查询端口',            en: 'Query Port' },
    'rate-limit':                { zh: '速率限制',            en: 'Rate Limit' },
    'rcon.password':             { zh: 'RCON 密码',           en: 'RCON Password' },
    'rcon.port':                 { zh: 'RCON 端口',           en: 'RCON Port' },
    'region-file-compression':   { zh: '区域文件压缩',         en: 'Region File Compression' },
    'require-resource-pack':     { zh: '强制资源包',          en: 'Require Resource Pack' },
    'resource-pack':             { zh: '资源包地址',          en: 'Resource Pack URL' },
    'resource-pack-id':          { zh: '资源包 UUID',         en: 'Resource Pack UUID' },
    'resource-pack-prompt':      { zh: '资源包提示',          en: 'Resource Pack Prompt' },
    'resource-pack-sha1':        { zh: '资源包 SHA1',         en: 'Resource Pack SHA1' },
    'server-ip':                 { zh: '服务器 IP',           en: 'Server IP' },
    'server-port':               { zh: '服务器端口',          en: 'Server Port' },
    'simulation-distance':       { zh: '模拟距离',            en: 'Simulation Distance' },
    'spawn-animals':             { zh: '生成动物',            en: 'Spawn Animals' },
    'spawn-monsters':            { zh: '生成怪物',            en: 'Spawn Monsters' },
    'spawn-npcs':                { zh: '生成 NPC',           en: 'Spawn NPCs' },
    'spawn-protection':          { zh: '出生点保护范围',       en: 'Spawn Protection' },
    'sync-chunk-writes':         { zh: '同步区块写入',         en: 'Sync Chunk Writes' },
    'text-filtering-config':     { zh: '文本过滤配置',         en: 'Text Filtering Config' },
    'text-filtering-version':    { zh: '文本过滤版本号',       en: 'Text Filtering Version' },
    'status-heartbeat-interval': { zh: '状态心跳间隔',         en: 'Status Heartbeat Interval' },
    'use-native-transport':      { zh: '使用本地传输',         en: 'Use Native Transport' },
    'view-distance':             { zh: '视距',               en: 'View Distance' },
    'white-list':                { zh: '白名单',              en: 'White List' }
};

// 获取属性名的当前语言显示
function label(key) {
    var entry = _props[key];
    if (!entry) return key;             // 无映射则返回原 key
    return entry[_current] || entry['zh'];
}

// 切换当前语言（'zh'｜'en'）
function setLanguage(lang) {
    _current = (lang === 'en' || lang === 'English') ? 'en' : 'zh';
}

// 获取当前语言代码
function currentLanguage() { return _current; }
