#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QSet>

class ServerManager;
class ServerController;
class SettingsController;
class DownloadManager;
class InstallCoordinator;

// QQ 机器人控制：独立管理 NapCat / NoneBot 两个外部进程，并内置一个
// 与 WebUI 完全解耦的本地 HTTP 控制通道（默认端口 25585）。
// 三者（WebUI / NapCat / NoneBot）状态互不影响、可独立开关，且能通过
// 本控制通道或 WebUI 设置页互相启停。MSM 只在以下情况主动推送 QQ：
//   1) 服务器资源占用（按间隔周期推送，默认 5 分钟）
//   2) 服务器异常退出（推送日志到管理员私信）
//   其余只在执行 QQ 指令后回传操作反馈，不主动打扰。
class BotController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool napcatEnabled READ napcatEnabled WRITE setNapcatEnabled NOTIFY napcatEnabledChanged)
    Q_PROPERTY(QString napcatPath READ napcatPath WRITE setNapcatPath NOTIFY napcatPathChanged)
    Q_PROPERTY(QString napcatState READ napcatState NOTIFY napcatStateChanged)
    Q_PROPERTY(bool nonebotEnabled READ nonebotEnabled WRITE setNonebotEnabled NOTIFY nonebotEnabledChanged)
    Q_PROPERTY(QString nonebotDir READ nonebotDir WRITE setNonebotDir NOTIFY nonebotDirChanged)
    Q_PROPERTY(QString nonebotState READ nonebotState NOTIFY nonebotStateChanged)
    // 联动启动：开启时由 MSM 拉起 NapCat / NoneBot 进程；关闭则 MSM 仅开放控制通道，
    // 由用户自行启动机器人并主动连入（默认关闭）。
    Q_PROPERTY(bool botLinkedStart READ botLinkedStart WRITE setBotLinkedStart NOTIFY botLinkedStartChanged)
    // 主开关：NapCat 与 NoneBot 已绑定，一起开、一起关（任一缺失都无法工作）
    Q_PROPERTY(bool botEnabled READ botEnabled WRITE setBotEnabled NOTIFY botEnabledChanged)
    Q_PROPERTY(int controlPort READ controlPort CONSTANT)
    Q_PROPERTY(int usageInterval READ usageInterval WRITE setUsageInterval NOTIFY usageIntervalChanged)
    // msm_control 控制插件在所选 NoneBot 目录中的状态：
    // "unknown" / "ok" / "missing" / "installing" / "error"
    Q_PROPERTY(QString msmPluginState READ msmPluginState NOTIFY msmPluginStateChanged)
    // napcat-plugin-msm 在 NapCat plugins/ 目录中的状态：
    // "unknown" / "ok" / "missing-source" / "error"
    Q_PROPERTY(QString napcatPluginState READ napcatPluginState NOTIFY napcatPluginStateChanged)

public:
    explicit BotController(ServerManager *sm, ServerController *sc,
                           SettingsController *settings, DownloadManager *dm,
                           QObject *parent = nullptr);
    ~BotController() override;

    bool napcatEnabled() const { return m_napcat; }
    QString napcatPath() const { return m_napcatPath; }
    QString napcatState() const { return m_napcatState; }
    bool nonebotEnabled() const { return m_nonebot; }
    QString nonebotDir() const { return m_nonebotDir; }
    QString nonebotState() const { return m_nonebotState; }
    int controlPort() const { return m_controlPort; }
    int usageInterval() const { return m_usageInterval; }
    bool botLinkedStart() const { return m_linkedStart; }
    bool botEnabled() const { return m_bot; }

    void setNapcatEnabled(bool v);
    void setNapcatPath(const QString &v);
    void setNonebotEnabled(bool v);
    void setNonebotDir(const QString &v);
    void setUsageInterval(int v);
    void setBotLinkedStart(bool v);
    void setBotEnabled(bool v);

    // 统一安装协调器（服务器类型+版本+加载器，支持多任务并发）。由主程序注入。
    void setInstallCoordinator(InstallCoordinator *c) { m_install = c; }

    // ---- 自动搜索候选（设置页下拉用，后续会接入"安装"功能）----
    Q_INVOKABLE QStringList detectNonebotDirs() const;
    Q_INVOKABLE QStringList detectNapcatPaths() const;
    // 最佳单个检测结果（设置页"自动检测"按钮用；优先已配置路径，其次扫描候选）
    Q_INVOKABLE QString detectNapcatPath() const;
    Q_INVOKABLE QString detectNonebotDir() const;
    // 检测当前 NoneBot 目录是否已接入 msm_control 插件
    Q_INVOKABLE bool isMsmPluginInstalled() const;
    // 退出时彻底停止所有机器人相关进程（控制服务 / NapCat / NoneBot），避免残留
    void stopAll();

    // 自动把 MSM 自带的 msm_control 插件部署进当前 NoneBot 目录（复制文件 + 改写 pyproject），成功后自动启动
    Q_INVOKABLE bool installMsmPlugin();
    // 用户在"手动安装"后点击"我已装好，重试"时调用：重新检测并启动 NoneBot
    Q_INVOKABLE void retryStartNonebot();
    QString msmPluginState() const { return m_msmPluginState; }

    // 检测 NapCat plugins/ 目录是否已装 napcat-plugin-msm
    Q_INVOKABLE bool isNapcatPluginInstalled() const;
    // 强制把构建好的 napcat-plugin-msm（dist 产物）拷进 NapCat 的 plugins/
    Q_INVOKABLE bool installNapcatPlugin();
    QString napcatPluginState() const { return m_napcatPluginState; }

    // 主动向 QQ 推送一条消息：scope="all" 推送给配置的通知目标，
    // scope="admin" 仅推送给管理员私信（用于异常日志上报）。
    Q_INVOKABLE void notify(const QString &message, const QString &scope = QStringLiteral("all"));
    // 服务器异常退出时调用：整理日志尾部并私信管理员
    Q_INVOKABLE void pushError(const QString &name, const QString &logTail);

signals:
    void napcatEnabledChanged();
    void napcatPathChanged();
    void napcatStateChanged();
    void nonebotEnabledChanged();
    void nonebotDirChanged();
    void nonebotStateChanged();
    void usageIntervalChanged();
    void botLinkedStartChanged();
    void botEnabledChanged();
    // 开启 NoneBot 时检测到目标目录缺 msm_control 插件，交由 UI 弹提示框
    void pluginMissing();
    void msmPluginStateChanged();
    void napcatPluginStateChanged();

private slots:
    void onApiNewConnection();
    void onApiReadyRead();
    void onApiDisconnected();
    void onSeenTick();
    // WebUI 访问令牌（与机器人链路共用）重生成后，回写已安装的 NoneBot 插件配置，
    // 使本机链路后续启动仍与最新令牌一致（本机链路本身免令牌，这里仅为一致性）。
    void onWebuiTokenChanged();

private:
    void startNapcat();
    void stopNapcat();
    void startNonebot();
    void stopNonebot();
    void startNonebotConnectOnly();
    void startControlServer();
    void stopControlServer();
    void pushUsage();

    // 控制通道 HTTP 解析（QTcpServer 手动解析，避开 QHttpServer 在 MinGW 下的运行期崩溃）
    static QString urlDecode(const QString &s);
    static QMap<QString, QString> parseQuery(const QString &q);
    void sendJson(QTcpSocket *sock, const QJsonObject &obj, int code = 200);
    void sendText(QTcpSocket *sock, const QString &text,
                  const QString &ct = QStringLiteral("text/plain; charset=utf-8"));
    void sendStatus(QTcpSocket *sock, int code, const QString &msg);
    void dispatchApi(const QString &method, const QString &path, const QString &query,
                     const QByteArray &body, const QMap<QString, QString> &hdr, QTcpSocket *sock);
    // 控制通道鉴权：所有客户端（含本机 loopback）都必须携带正确令牌（Authorization/Bearer 或 ?token=）。
    // 系统/OS 级操作（如任务管理器终止进程、MSM 主界面直接调用的服务管理）不经过本通道，不受此门限影响；
    // 由 MSM 拉起的 NapCat/NoneBot 子进程已通过 msm_token 注入获得令牌，属于受信链路。
    bool ctrlAuthorized(QTcpSocket *s, const QMap<QString, QString> &hdr, const QString &query) const;

    // WebSocket：在 25585 同一端口复用，插件经 WS 长连接接收 MSM 主动推送
    void upgradeToWs(QTcpSocket *s, const QMap<QString, QString> &hdr);
    void parseWsFrames(QTcpSocket *s);
    void sendWsText(QTcpSocket *s, const QString &text);
    QByteArray wsEncodeFrame(const QString &text);
    void pushToClients(const QJsonObject &obj);

    QString detectPython(const QString &dir) const;
    // PATH 中的 nb 可执行（Windows 下可能是 nb.exe / nb.cmd / nb.bat / nb），找不到返回空
    QString detectNb() const;
    // 当前 NoneBot 目录是否已接入 msm_control 插件
    bool isMsmPluginInstalled(const QString &dir) const;
    // 将 MSM 自带的 msm_control 插件复制到 dir/plugins 并改写 pyproject，返回是否成功
    bool installMsmPluginInto(const QString &dir);
    // 改写 NoneBot 工程的 pyproject.toml：确保 plugin_dirs 含 plugins 并补 msm_* 配置项
    bool patchPyproject(const QString &dir) const;
    // 把访问令牌注入 NapCat 插件目录的 config.json（loopback 也需令牌，故自动注入，免手动填写）。
    bool patchNapcatConfig(const QString &batPath) const;
    // MSM 自带的 msm_control.py 源文件路径（位于程序目录 qqbot/plugins 下）
    QString sourcePluginPath() const;
    // ---- NapCat 插件（napcat-plugin-msm）自动安装 ----
    // MSM 自带的构建产物目录（含 index.mjs + package.json），找不到返回空
    QString napcatPluginSourceDir() const;
    // NapCat 安装目录（napcat.bat 所在目录）下的 plugins/napcat-plugin-msm 目标目录
    QString napcatPluginDestDir(const QString &batPath) const;
    // 从 napcat.bat 目录向上/向下查找 NapCat 已有的 plugins 目录（用于复用真实插件路径）
    QString findExistingPluginsDir(const QDir &batDir) const;
    // 从 dir 向下（depth 层）查找第一个已存在的 plugins 目录
    QString findPluginsDirRecursive(const QDir &dir, int depth) const;
    bool isNapcatPluginInstalled(const QString &batPath) const;
    // 把源目录整体拷到目标目录（覆盖旧文件），必要时补一个最小 package.json
    bool installNapcatPluginInto(const QString &batPath);
    // 启用/切换模式时调用：源比目标新或目标缺失则自动（重新）安装
    void ensureNapcatPlugin();
    // NapCat 4.18+ 在 napcat.mjs 里硬编码了官方插件白名单 Set（rme），第三方插件
    // 会被 PluginLoader 拒绝（"not in official plugin whitelist"）。本函数检测该白名单，
    // 若缺 napcat-plugin-msm 则自动追加一行，使本地插件能正常加载。
    // 返回 true：已在白名单（无需补丁）或补丁成功；false：找不到 napcat.mjs / 锚点。
    bool ensureNapcatWhitelist(const QString &batPath);
    // 在 dir 下递归查找 napcat-plugin-msm 源目录（depth 层内），优先返回带正确 package.json 的
    QString findNapcatPluginSource(const QDir &dir, int depth = 4) const;
    static bool copyDirRecursively(const QString &srcDir, const QString &dstDir);
    void setNapcatState(const QString &s);
    void setNonebotState(const QString &s);
    void setMsmPluginState(const QString &s);
    void setNapcatPluginState(const QString &s);

    ServerManager *m_sm = nullptr;
    ServerController *m_sc = nullptr;
    SettingsController *m_settings = nullptr;
    DownloadManager *m_dm = nullptr;
    InstallCoordinator *m_install = nullptr;

    QProcess *m_napcatProc = nullptr;
    QProcess *m_nonebotProc = nullptr;
    // 访问令牌重生成时，标记需在进程退出后自动重启对应机器人（使其加载注入的新令牌）
    bool m_restartNapcatPending = false;
    bool m_restartNonebotPending = false;
    bool m_nonebotStopping = false;   // 标记“用户显式停用”，用于 finished 时区分主动停止与意外退出
    QTcpServer *m_tcp = nullptr;
    QMap<QTcpSocket *, QByteArray> m_buffers;
    QSet<QTcpSocket *> m_wsClients;
    QTimer *m_usageTimer = nullptr;
    QTimer *m_seenTimer = nullptr;
    QElapsedTimer m_botSeen;
    QNetworkAccessManager *m_net = nullptr;

    bool m_napcat = false;
    QString m_napcatPath;
    QString m_napcatState = QStringLiteral("stopped");
    bool m_nonebot = false;
    QString m_nonebotDir;
    QString m_nonebotState = QStringLiteral("stopped");
    bool m_linkedStart = false;
    bool m_bot = false;
    int m_usageInterval = 300;
    int m_controlPort = 25585;
    QString m_notifyUrl = QStringLiteral("http://127.0.0.1:8080/msm/notify");
    QString m_msmPluginState = QStringLiteral("unknown");
    QString m_napcatPluginState = QStringLiteral("unknown");
};
