# MSM 共用契约（shared/）

本目录存放**桌面端与远控端之间唯一的共用物：接口契约**。

两个子项目之间没有编译期代码耦合：

- `desktop/`（Qt/C++）**实现**这套 HTTP JSON API；
- `remote/`（Android / iOS）**调用**这套 API。

两端各自按本文件描述的契约实现 / 调用。任何接口变更都应先改本文件，再同步两端。

> 说明：除本契约外，两端不共享代码——桌面端是 C++/QML，远控端是 Kotlin/Swift，
> 无法共用实现，因此"共用"明确收敛为"契约"这一层。

---

## 1. 连接基础

| 项 | 值 |
| --- | --- |
| 协议 | HTTP/1.1，HTTPS 时服务端使用**自签证书** |
| 默认端口 | 25575（设置项 `webuiPort`，用户可改） |
| 默认绑定 | `127.0.0.1`（仅本机）；开启「暴露到局域网」后为 `0.0.0.0` |
| 静态资源 | `GET /` 或 `/index.html` 返回 SPA 面板；`/qrcode.js` 为公开二维码库 |

**自签证书**：服务端证书为自动生成/用户自定义，不会被系统信任。
客户端必须放行证书校验（Android 端已内置信任策略；浏览器需手动「继续访问」）。

> 已知行为：桌面端若 TLS 不可用会自动回退明文 HTTP（`m_https=false`）。
> 客户端应以实际连通性为准，不要假定一定是 HTTPS。

## 2. 鉴权

除 `/api/pair` 外，**所有 `/api/*` 请求都必须携带访问令牌**，否则返回 `401`。

两种携带方式（任选其一）：

```
Authorization: Bearer <token>
?token=<token>
```

令牌即设置中的 `webuiToken`（桌面端自动生成，可在设置页重新生成）。

## 3. 移动端配对（免令牌入口）

手机拿不到令牌，因此提供一次性配对码换取令牌的流程：

1. 桌面端生成配对码（`generatePairCode()`），有效期 **10 分钟**、**一次性**。
2. 手机提交配对码：

```
POST /api/pair
Content-Type: application/json

{ "code": "XXXXXX" }          // 大小写不敏感

200 → { "token": "...", "port": 25575, "https": true }
403 → 配对码无效 / 已使用 / 已过期
```

3. 换取 token 后，后续请求按第 2 节携带令牌。

**已鉴权的配对信息查询**（供桌面端界面/二维码展示）：

```
GET /api/paircode
→ { "code": "XXXXXX", "uri": "msm://token@<host>:<port>?t=<token>",
    "remainSec": 600, "used": false }
```

### 配对 URI 格式

```
msm://token@<host>:<port>?t=<token>
```

- 仅本机时 `<host>` 为 `127.0.0.1`；
- 开启「暴露到局域网」时为本机首选非回环 IPv4，便于手机同网段直连。

远控端 App 已注册 `msm://` 深链，扫码或粘贴该 URI 即可直连。

## 4. API 端点

以下路径均带 `/api` 前缀，且**需令牌**（除非注明）。

### 服务器

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET | `/servers` | 服务器列表（含 running 状态） |
| GET | `/servers/{name}` | 详情：name/version/type/path/running/players/console/properties/mods |
| DELETE | `/servers/{name}` | 删除；运行中返回 `409` |
| GET | `/servers/{name}/start?min=&max=` | 启动（min/max 为内存 MB，缺省 1024/2048） |
| GET | `/servers/{name}/stop` | 停止 |
| GET | `/servers/{name}/forcestop` | 强制停止 |
| GET | `/servers/{name}/send?cmd=` | 发送控制台指令 |
| GET | `/servers/{name}/console` | 控制台输出 |
| GET | `/servers/{name}/players` | 在线玩家 |
| POST | `/servers/{name}/proxy` | 绑定/设置代理 |
| GET | `/servers/{name}/watchdog` | 看门狗状态 |
| GET | `/servers/{name}/optimods` | 优化模组检索结果 |
| POST | `/servers/{name}/installoptimod` | 安装优化模组 |

### 系统 / 配置 / 主题

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET | `/system` | 轻量系统监控（CPU/内存，供轮询） |
| GET | `/state` | 全局状态 |
| GET | `/errors` | 异常列表 |
| POST | `/errors/{path}/{action}` | 异常处理动作 |
| GET/PUT | `/settings` | 读写设置 |
| GET/PUT | `/theme` | 读写主题（dark/accent） |
| GET | `/proxies` | 代理实例列表 |

### 安装 / 下载 / 目录

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| POST | `/installserver` | 提交安装服务器任务 |
| GET/DELETE | `/installtasks` | 查询 / 清除安装任务 |
| GET/POST | `/create`、`/create/loadversions`、`/create/progress` | 创建服务器 |
| GET | `/createtypes`、`/versions` | 可用类型 / 版本 |
| POST | `/import`、`/import/progress` | 导入整合包 |
| GET | `/downloads` | 下载任务列表 |
| GET | `/downloads/pause\|resume\|cancel\|restart\|remove` | 下载控制 |
| GET | `/downloads/clear` | 清除已完成任务 |
| GET | `/catalog`、`/catalog/refresh`、`/catalog/download`、`/catalog/savedir`、`/catalog/modversion`、`/catalog/modloader`、`/catalog/downloadselected`、`/catalog/downloadloader`、`/catalog/pause\|resume\|cancel`、`/catalog/cleantempjava` | 下载目录相关 |

### Java / 机器人

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET | `/java` | Java 运行环境信息 |
| GET | `/java/download` | 下载安装 Java |
| GET | `/java/sethome` | 设置 JAVA_HOME |
| GET/POST | `/bot` | QQ 机器人状态 / 控制 |

## 5. 约定

- 请求/响应体为 JSON（`application/json`）；查询参数用于简单动作（如 `?cmd=`）。
- 成功返回 `200` + JSON；动作类通常返回 `{"ok": true}`。
- 常见错误码：`401` 未授权（令牌缺失/错误）、`403` 配对码无效、`404` 资源不存在、
  `405` 方法不允许、`409` 状态冲突（如删除运行中的服务器）。
- 所有路径中的服务器名需 URL 编码（`encodeURIComponent`），服务端会解码后匹配。

> 注：桌面端另有一套**控制通道**（供 QQ 机器人链路使用，端口与 WebUI 不同，
> 见 `botcontroller.cpp`），不属于本契约；远控端不应依赖它。
