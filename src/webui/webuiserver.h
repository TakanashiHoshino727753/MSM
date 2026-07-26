#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QTcpServer>
#include <QTcpSocket>
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

// 本地 WebUI 服务：在 0.0.0.0:端口 监听 HTTP，提供与本地端功能/设计一致的单页管理面板。
// 除“关闭 WebUI”这类有害设置外，其余功能与本地端保持一致；弹窗（创建/导入）改为标签页切换。
class WebUIServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(int port READ port NOTIFY portChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

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

    void setPort(int p);
    void setEnabled(bool on);
    bool enabled() const { return m_enabled; }

    // 由本地端主题变化同步调用，使 WebUI 与本地端主题保持一致
    Q_INVOKABLE void setThemeState(bool dark, const QColor &accent);

signals:
    void runningChanged();
    void portChanged();
    void errorChanged();
    void themeChangeRequested(bool dark, const QColor &accent);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void dispatch(const QString &method, const QString &path, const QString &query,
                  const QByteArray &body, QTcpSocket *sock);
    void sendJson(QTcpSocket *sock, const QJsonObject &obj, int code = 200);
    void sendHtml(QTcpSocket *sock, const QString &html);
    void sendText(QTcpSocket *sock, const QString &text, const QString &ct);
    void sendStatus(QTcpSocket *sock, int code, const QString &msg);

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

    int m_port = 25575;
    bool m_enabled = false;
    bool m_dark = true;
    QColor m_accent = QColor(QStringLiteral("#4f8cff"));
    QString m_error;

    QMap<QTcpSocket *, QByteArray> m_buffers;
};
