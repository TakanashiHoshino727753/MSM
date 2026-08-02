#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSslServer>
#include <QSslConfiguration>
#include <QMap>
#include <QByteArray>
#include <QJsonObject>

class ServerManager;
class ServerController;
class DownloadManager;
class DownloadCatalog;
class CreateServerController;
class ModpackImporter;
class SettingsController;
class SystemMonitor;
class JavaManager;
class BotController;
class InstallCoordinator;

// 本地 WebUI 服务：在 0.0.0.0:端口 监听 HTTP，提供与本地端功能/设计一致的单页管理面板。
// 除“关闭 WebUI”这类有害设置外，其余功能与本地端保持一致；弹窗（创建/导入）改为标签页切换。
class WebUIServer : public QObject
{
    Q_OBJECT
    friend class WebTcpServer;
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(int port READ port NOTIFY portChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool https READ isHttps NOTIFY runningChanged)   // 是否以 HTTPS 提供

public:
    explicit WebUIServer(ServerManager *sm,
                         ServerController *sc,
                         DownloadManager *dm,
                         CreateServerController *create,
                         ModpackImporter *import,
                         SettingsController *settings,
                         QObject *monitor,
                         JavaManager *java,
                         QObject *parent = nullptr);

    bool isRunning() const { return m_server && m_server->isListening(); }
    int port() const { return m_port; }
    QString error() const { return m_error; }
    bool isHttps() const { return m_https; }

    void setPort(int p);
    void setEnabled(bool on);
    Q_INVOKABLE void rebind();   // 重新监听（暴露范围/证书变化时）
    bool enabled() const { return m_enabled; }

    // 由本地端主题变化同步调用，使 WebUI 与本地端主题保持一致
    Q_INVOKABLE void setThemeState(bool dark, const QColor &accent);

    // 接入本地机器人控制器，使 WebUI 可控制/查看本地插件（QQ 机器人）状态
    void setBotController(BotController *bot);

    // 接入安装协调器（服务器类型+版本+加载器，支持多任务并发），供 WebUI 提交安装任务
    void setInstallCoordinator(InstallCoordinator *c) { m_install = c; }

signals:
    void runningChanged();
    void portChanged();
    void errorChanged();
    void themeChangeRequested(bool dark, const QColor &accent);

private slots:
    void onNewConnection();
    void handleIncoming(qintptr socketDescriptor);
    void onReadyRead();
    void onDisconnected();

private:
    void dispatch(const QString &method, const QString &path, const QString &query,
                  const QMap<QString, QString> &hdr, const QByteArray &body, QTcpSocket *sock);
    void sendJson(QTcpSocket *sock, const QJsonObject &obj, int code = 200);
    void sendHtml(QTcpSocket *sock, const QString &html);
    void sendText(QTcpSocket *sock, const QString &text, const QString &ct);
    void sendStatus(QTcpSocket *sock, int code, const QString &msg);

    // 安全：TLS 与令牌
    bool startListen();                 // 配置 TLS 并按设置绑定地址开始监听
    void relisten();                    // 重新监听（端口/暴露范围变化时）
    bool loadExistingTls();             // 同步加载已存在证书，成功返回 true（不阻塞）
    bool ensureCertSync();              // 同步生成并加载自签证书，成功返回 true（HTTPS 可用）
    void ensureSelfSignedCert(const QString &cerPath);  // 自动生成自签证书进 Windows 存储
    QString certDir() const;            // 自签证书 .cer 存放目录
    bool checkToken(const QMap<QString, QString> &hdr, const QMap<QString, QString> &q) const;

    // 各业务处理
    QJsonObject statePayload();
    QJsonObject systemPayload();                // 轻量系统监控（仅 CPU/内存），供侧栏轮询
    QJsonObject settingsPayload() const;
    QJsonObject themePayload() const;
    // 服务器列表（含运行态）：serverSummary 不含 running，需在接口层用 ServerController 补全
    QJsonArray serverListWithRunning() const;

    QString m_spaHtml() const;

    QTcpServer *m_server = nullptr;
    ServerManager *m_sm = nullptr;
    ServerController *m_sc = nullptr;
    DownloadCatalog *m_webCatalog = nullptr;   // WebUI 独立使用的下载目录，与本地端互不影响
    CreateServerController *m_create = nullptr;
    ModpackImporter *m_import = nullptr;
    SettingsController *m_settings = nullptr;
    QObject *m_monitor = nullptr;
    JavaManager *m_java = nullptr;
    DownloadManager *m_dm = nullptr;   // 底层下载管理器，供 WebUI 暴露统一的“下载任务”列表
    BotController *m_bot = nullptr;     // 本地机器人控制器（本地插件），用于 WebUI 控制/状态展示
    InstallCoordinator *m_install = nullptr;   // 安装协调器（多任务并发安装）

    int m_port = 25575;
    bool m_enabled = false;
    bool m_dark = true;
    QColor m_accent = QColor(QStringLiteral("#4f8cff"));
    QString m_error;
    bool m_https = false;      // 当前是否以 HTTPS 提供
    QSslConfiguration m_sslConf;   // HTTPS 时的 SSL 配置（明文模式不使用）

    QMap<QTcpSocket *, QByteArray> m_buffers;
};
