// remoteclient.cpp —— 见 remoteclient.h
#include "remoteclient.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QSslConfiguration>
#include <QRegularExpression>

RemoteClient::RemoteClient(QObject *parent) : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    // 自签证书：忽略证书错误（本地端用 openssl 自签，无法被系统信任）
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(ssl);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, &RemoteClient::refreshServers);

    m_optPollTimer = new QTimer(this);
    m_optPollTimer->setInterval(1500);
    connect(m_optPollTimer, &QTimer::timeout, this, [this]() {
        if (m_optName.isEmpty())
            return;
        QNetworkReply *r = apiGet("/api/servers/" + QUrl::toPercentEncoding(m_optName) + "/optimods");
        connect(r, &QNetworkReply::finished, this, [this, r]() {
            if (r->error() == QNetworkReply::NoError) {
                QJsonObject o = QJsonDocument::fromJson(r->readAll()).object();
                m_optMods = o.value("mods").toArray();
                m_optLoading = o.value("loading").toBool();
                emit optModsChanged();
                emit optLoadingChanged();
                if (!m_optLoading && !m_optMods.isEmpty())
                    m_optPollTimer->stop();   // 检索完成
            }
            r->deleteLater();
        });
    });
}

void RemoteClient::setConnected(bool v)
{
    if (m_connected != v) {
        m_connected = v;
        emit connectedChanged();
    }
}

void RemoteClient::setError(const QString &e)
{
    if (m_lastError != e) {
        m_lastError = e;
        emit lastErrorChanged();
    }
}

QUrl RemoteClient::buildUrl(const QString &path, const QVariantMap &query) const
{
    QUrl url;
    url.setScheme(m_https ? "https" : "http");
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath(path);
    QUrlQuery q;
    for (auto it = query.begin(); it != query.end(); ++it)
        q.addQueryItem(it.key(), it.value().toString());
    url.setQuery(q);
    return url;
}

void RemoteClient::applyToken(QNetworkRequest &req)
{
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
}

QNetworkReply *RemoteClient::apiGet(const QString &path, const QVariantMap &query)
{
    QNetworkRequest req(buildUrl(path, query));
    applyToken(req);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return m_nam->get(req);
}

QNetworkReply *RemoteClient::apiPost(const QString &path, const QJsonObject &body)
{
    QNetworkRequest req(buildUrl(path, {}));
    applyToken(req);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return m_nam->post(req, data);
}

void RemoteClient::connectTo(const QString &host, int port, const QString &token, bool https)
{
    m_host = host.trimmed();
    m_port = port;
    m_token = token.trimmed();
    m_https = https;
    emit hostChanged();
    emit portChanged();
    emit tokenChanged();
    emit httpsChanged();

    // 用 /api/servers 探活：成功即视为已连接
    QNetworkReply *r = apiGet("/api/servers");
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        if (r->error() == QNetworkReply::NoError) {
            setConnected(true);
            setError(QString());
            refreshServers();
            emit toast(tr("已连接"), m_host + ":" + QString::number(m_port));
        } else {
            setConnected(false);
            setError(r->errorString());
            emit toast(tr("连接失败"), r->errorString());
        }
        r->deleteLater();
    });
}

void RemoteClient::connectFromUri(const QString &msmUri)
{
    QVariantMap m;
    if (parseUri(msmUri, m))
        connectTo(m.value("host").toString(), m.value("port").toInt(), m.value("token").toString(),
                  m.value("https", true).toBool());
    else
        setError(tr("无法解析连接串"));
}

bool RemoteClient::parseUri(const QString &msmUri, QVariantMap &out)
{
    // 格式：msm://token@host:port?t=token   或   msm://host:port?t=token
    QRegularExpression re("^msm://(?:([^@]+)@)?([^:/?]+)(?::(\\d+))?(?:\\?t=([^&]+))?$");
    QRegularExpressionMatch mt = re.match(msmUri.trimmed());
    if (!mt.hasMatch())
        return false;
    out["token"] = mt.captured(1).isEmpty() ? mt.captured(4) : mt.captured(1);
    out["host"] = mt.captured(2);
    out["port"] = mt.captured(3).isEmpty() ? 25575 : mt.captured(3).toInt();
    out["https"] = true;
    return true;
}

QString RemoteClient::toMsmUri() const
{
    return QString("msm://%1@%2:%3?t=%1").arg(m_token, m_host, QString::number(m_port));
}

void RemoteClient::disconnectFrom()
{
    m_pollTimer->stop();
    setConnected(false);
}

void RemoteClient::refreshServers()
{
    if (!m_connected)
        return;
    QNetworkReply *r = apiGet("/api/servers");
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        if (r->error() == QNetworkReply::NoError) {
            QJsonObject o = QJsonDocument::fromJson(r->readAll()).object();
            m_servers = o.value("servers").toArray();
            emit serversChanged();
        }
        r->deleteLater();
    });
}

void RemoteClient::startServer(const QString &name, int min, int max)
{
    QNetworkReply *r = apiGet("/api/servers/" + QUrl::toPercentEncoding(name) + "/start",
                              {{"min", min}, {"max", max}});
    connect(r, &QNetworkReply::finished, this, [this, r, name]() {
        if (r->error() == QNetworkReply::NoError)
            emit toast(tr("启动"), name);
        else
            emit toast(tr("启动失败"), r->errorString());
        r->deleteLater();
        refreshServers();
    });
}

void RemoteClient::stopServer(const QString &name)
{
    QNetworkReply *r = apiGet("/api/servers/" + QUrl::toPercentEncoding(name) + "/stop");
    connect(r, &QNetworkReply::finished, this, [this, r, name]() {
        if (r->error() == QNetworkReply::NoError)
            emit toast(tr("停止"), name);
        else
            emit toast(tr("停止失败"), r->errorString());
        r->deleteLater();
        refreshServers();
    });
}

void RemoteClient::forceStopServer(const QString &name)
{
    QNetworkReply *r = apiGet("/api/servers/" + QUrl::toPercentEncoding(name) + "/forcestop");
    connect(r, &QNetworkReply::finished, this, [this, r, name]() {
        r->deleteLater();
        refreshServers();
    });
}

void RemoteClient::sendCommand(const QString &name, const QString &cmd)
{
    QNetworkReply *r = apiGet("/api/servers/" + QUrl::toPercentEncoding(name) + "/send",
                              {{"cmd", cmd}});
    connect(r, &QNetworkReply::finished, this, [r]() { r->deleteLater(); });
}

void RemoteClient::fetchOptimods(const QString &name, const QString &mc, const QString &loader)
{
    m_optName = name;
    m_optMods = QJsonArray();
    m_optLoading = true;
    emit optModsChanged();
    emit optLoadingChanged();
    // 触发检索
    QNetworkReply *r = apiGet("/api/servers/" + QUrl::toPercentEncoding(name) + "/optimods",
                              {{"mc", mc}, {"loader", loader}});
    connect(r, &QNetworkReply::finished, this, [this, r, name]() {
        r->deleteLater();
        if (r->error() == QNetworkReply::NoError)
            m_optPollTimer->start();   // 开始轮询直至 loading=false
        else
            emit toast(tr("优化模组检索失败"), r->errorString());
    });
}

void RemoteClient::installOptimod(const QString &name, const QString &mc, const QString &loader)
{
    QNetworkReply *r = apiPost("/api/servers/" + QUrl::toPercentEncoding(name) + "/installoptimod",
                               {{"mc", mc}, {"loader", loader}});
    connect(r, &QNetworkReply::finished, this, [this, r, name]() {
        if (r->error() == QNetworkReply::NoError)
            emit toast(tr("已发起优化模组安装"), name + " 将在本地端后台安装");
        else
            emit toast(tr("安装失败"), r->errorString());
        r->deleteLater();
    });
}
