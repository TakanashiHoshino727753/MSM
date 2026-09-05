// remoteclient.h —— MSM_Remote 远控端核心：连接本地端控制器并转发操作
// 本地端通过 WebUIServer (默认 25575) 暴露 REST API（无 WebSocket，全轮询）。
// 鉴权：Authorization: Bearer <token> 或 ?token=<token>；HTTPS 自签需忽略证书错误。
#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QTimer>

class RemoteClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString host READ host NOTIFY hostChanged)
    Q_PROPERTY(int port READ port NOTIFY portChanged)
    Q_PROPERTY(QString token READ token NOTIFY tokenChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QJsonArray servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(bool https READ https NOTIFY httpsChanged)

public:
    explicit RemoteClient(QObject *parent = nullptr);

    bool connected() const { return m_connected; }
    QString host() const { return m_host; }
    int port() const { return m_port; }
    QString token() const { return m_token; }
    QString lastError() const { return m_lastError; }
    QJsonArray servers() const { return m_servers; }
    bool https() const { return m_https; }

    Q_INVOKABLE void connectTo(const QString &host, int port, const QString &token, bool https = true);
    Q_INVOKABLE void connectFromUri(const QString &msmUri);   // 解析 msm://token@host:port?t=...
    Q_INVOKABLE void disconnectFrom();
    Q_INVOKABLE bool parseUri(const QString &msmUri, QVariantMap &out);   // 仅供 QML 预览

    // —— 服务器操作（直接转发本地端 HTTP 接口）——
    Q_INVOKABLE void refreshServers();
    Q_INVOKABLE void startServer(const QString &name, int min = 1024, int max = 2048);
    Q_INVOKABLE void stopServer(const QString &name);
    Q_INVOKABLE void forceStopServer(const QString &name);
    Q_INVOKABLE void sendCommand(const QString &name, const QString &cmd);

    // —— 一键安装优化模组 ——
    // 触发检索（mc 如 "1.20.1"，loader: fabric/forge/neoforge），随后自动轮询直至返回 mods
    Q_INVOKABLE void fetchOptimods(const QString &name, const QString &mc, const QString &loader);
    Q_INVOKABLE void installOptimod(const QString &name, const QString &mc, const QString &loader);
    Q_PROPERTY(QJsonArray optMods READ optMods NOTIFY optModsChanged)
    Q_PROPERTY(bool optLoading READ optLoading NOTIFY optLoadingChanged)
    QJsonArray optMods() const { return m_optMods; }
    bool optLoading() const { return m_optLoading; }

    Q_INVOKABLE QString toMsmUri() const;   // 生成 msm:// 供分享（本地端已有，但远控端也可展示）

signals:
    void connectedChanged();
    void hostChanged();
    void portChanged();
    void tokenChanged();
    void lastErrorChanged();
    void serversChanged();
    void httpsChanged();
    void optModsChanged();
    void optLoadingChanged();
    void toast(const QString &title, const QString &text);   // 给 QML 弹提示

private:
    void setConnected(bool v);
    void setError(const QString &e);
    QNetworkReply *apiGet(const QString &path, const QVariantMap &query = {});
    QNetworkReply *apiPost(const QString &path, const QJsonObject &body = {});
    void applyToken(QNetworkRequest &req);
    QUrl buildUrl(const QString &path, const QVariantMap &query) const;

    QNetworkAccessManager *m_nam = nullptr;
    QString m_host;
    int m_port = 25575;
    QString m_token;
    bool m_https = true;
    bool m_connected = false;
    QString m_lastError;
    QJsonArray m_servers;
    QJsonArray m_optMods;
    bool m_optLoading = false;
    QTimer *m_pollTimer = nullptr;        // 服务器列表轮询（2s）
    QTimer *m_optPollTimer = nullptr;     // 优化模组检索轮询（1.5s）
    QString m_optName;                    // 正在检索的服务器名
};
