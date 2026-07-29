#pragma once
#include <QObject>
#include <QString>
#include <QProcess>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <functional>

class QJsonDocument;
class ServerManager;
class ServerController;
class JavaManager;

// Velocity 反向代理聚合控制器：
// 让多台同时运行的 Minecraft 服务器通过一个统一入口端口对外服务，
// 玩家连接代理端口后可用 /server <名称> 在各后端服务器之间切换。
//
// 职责：
//  1. 自动下载 velocity.jar（PaperMC Fill v3 API，取最新版本最新构建）
//  2. 依据服务器列表生成 velocity.toml + forwarding.secret
//  3. 自动修补后端配置（server.properties online-mode=false；
//     Paper 后端另写 config/paper-global.yml 的 velocity modern forwarding）
//  4. 管理代理进程的启动 / 停止，转发控制台输出
//
// 转发模式策略：所有后端都是 Paper → modern（可拿到玩家真实 IP/UUID）；
// 混有 vanilla/forge/fabric → none（兼容一切后端，但后端看到的是代理 IP）。
class ProxyController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool installed READ installed NOTIFY installedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int proxyPort READ proxyPort WRITE setProxyPort NOTIFY proxyPortChanged)
    Q_PROPERTY(QString motd READ motd WRITE setMotd NOTIFY motdChanged)
    Q_PROPERTY(qreal installProgress READ installProgress NOTIFY installProgressChanged)
    Q_PROPERTY(QString proxyDir READ proxyDir CONSTANT)
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)
    Q_PROPERTY(bool stopBackendsWithProxy READ stopBackendsWithProxy WRITE setStopBackendsWithProxy NOTIFY stopBackendsWithProxyChanged)
    Q_PROPERTY(bool autoRestart READ autoRestart WRITE setAutoRestart NOTIFY autoRestartChanged)
public:
    explicit ProxyController(ServerManager *sm, ServerController *sc,
                             JavaManager *java = nullptr, QObject *parent = nullptr);
    // 注入统一 JavaManager：代理将使用“独立”的 JDK（托管目录或代理自身目录），
    // 不再借用某台后端服务器的 JDK。
    void setJavaManager(JavaManager *java) { m_java = java; }
    bool offlineMode() const { return m_offlineMode; }
    void setOfflineMode(bool v);
    bool stopBackendsWithProxy() const { return m_stopBackendsWithProxy; }
    void setStopBackendsWithProxy(bool v);
    bool autoRestart() const { return m_autoRestart; }
    void setAutoRestart(bool v);

    bool running() const { return m_proc && m_proc->state() != QProcess::NotRunning; }
    bool installed() const;
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    int proxyPort() const { return m_proxyPort; }
    void setProxyPort(int p);
    QString motd() const { return m_motd; }
    void setMotd(const QString &m);
    qreal installProgress() const { return m_installProgress; }
    QString proxyDir() const;

    // 下载/更新 velocity.jar（异步；进度经 installProgress，结果经 status/console）
    Q_INVOKABLE void install();
    // 依据当前服务器列表生成 velocity.toml/forwarding.secret 并修补各后端配置
    Q_INVOKABLE bool syncConfig();
    // 启动代理（内部先 syncConfig；javaPath 为空则自动探测，需 Java 17+）
    Q_INVOKABLE void start(const QString &javaPath = QString());
    // 优雅停止（发送 shutdown，超时后强杀）
    Q_INVOKABLE void stop();
    // 后端映射摘要（QML 表格显示：name / host / port / type / forwarding）
    Q_INVOKABLE QVariantList backendSummary() const;
    // 代理完整控制台历史
    Q_INVOKABLE QString getConsole() const { return m_console; }

signals:
    void runningChanged();
    void installedChanged();
    void busyChanged();
    void statusChanged();
    void proxyPortChanged();
    void motdChanged();
    void installProgressChanged();
    void offlineModeChanged();
    void stopBackendsWithProxyChanged();
    void autoRestartChanged();
    void crashed();          // 代理异常退出（非用户主动停止），用于通知/自动重拉起
    void portConflict(int port);  // 启动前入口端口被占用
    void consoleAppended(const QString &line);

private:
    void setStatus(const QString &s);
    void setBusy(bool b);
    void appendConsole(const QString &line);
    QString forwardingMode() const;          // "modern" / "none"
    QString ensureSecret();                  // 读取/生成 forwarding.secret，返回密钥
    void patchBackends(const QString &secret, const QString &mode);
    void patchPaperGlobal(const QString &serverPath, const QString &secret);
    // 探测 Java：优先代理自身目录 jvm-*/bin/java.exe，其次已为代理托管的 JDK（feature 21）；
    // 不再主动借用某台后端，缺失时交由 start() 触发独立下载（见 fallbackJava）。
    QString detectJava() const;
    // 启动代理进程（已确认 java 可用）
    void launchProcess(const QString &java);
    // 兜底探测：各后端服务器记录的 JDK 与系统 PATH 的 java（仅在独立 JDK 不可用/下载失败时）
    QString fallbackJava() const;
    // 取某后端服务器专属 JDK 路径（.msm/java.txt 优先，其次目录内 jvm*）
    QString serverJavaPath(const QString &path) const;
    void fetchJson(const QString &url, std::function<void(const QJsonDocument &)> ok,
                   std::function<void(const QString &)> fail);
    void downloadJar(const QString &url);

    ServerManager *m_sm = nullptr;
    ServerController *m_sc = nullptr;
    JavaManager *m_java = nullptr;
    QNetworkAccessManager m_nam;
    QProcess *m_proc = nullptr;
    QString m_status;
    QString m_console;
    QString m_motd;
    int m_proxyPort = 25577;
    qreal m_installProgress = 0;
    bool m_busy = false;
    bool m_offlineMode = true;   // 默认关掉正版验证，离线/非正版玩家也能进
    bool m_stopBackendsWithProxy = true;  // 停止代理时一并停止由本代理拉起的后端
    bool m_autoRestart = true;   // 代理崩溃后自动重拉起
    int m_maxRetries = 5;        // 自动重拉起最大次数
    int m_backoffSec = 5;        // 退避基数（秒），实际延迟 = base * 2^(attempt-1)
    int m_retryCount = 0;        // 当前连续重拉起次数
    bool m_expectedExit = false; // true 表示本次退出为用户主动停止，不触发自动重拉起
    QStringList m_autoStarted;   // 本次启动代理时自动拉起的后端名称
};
