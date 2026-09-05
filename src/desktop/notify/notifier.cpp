#include "notifier.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

Notifier::Notifier(QObject *parent) : QObject(parent) {}

void Notifier::setUrl(const QString &v) { if (v == m_url) return; m_url = v; emit urlChanged(); }
void Notifier::setType(const QString &v) { if (v == m_type) return; m_type = v; emit typeChanged(); }
void Notifier::setEnabled(bool v) { if (v == m_enabled) return; m_enabled = v; emit enabledChanged(); }
void Notifier::setNotifyCrash(bool v) { if (v == m_notifyCrash) return; m_notifyCrash = v; emit notifyCrashChanged(); }
void Notifier::setNotifyState(bool v) { if (v == m_notifyState) return; m_notifyState = v; emit notifyStateChanged(); }
void Notifier::setNotifyPlayer(bool v) { if (v == m_notifyPlayer) return; m_notifyPlayer = v; emit notifyPlayerChanged(); }

QNetworkAccessManager *Notifier::nam()
{
    if (!m_nam)
        m_nam = new QNetworkAccessManager(this);
    return m_nam;
}

QByteArray Notifier::payload(const QString &title, const QString &text) const
{
    const QString body = title.isEmpty()
        ? text
        : QStringLiteral("**[%1]**\n%2").arg(title, text);

    if (m_type == QStringLiteral("wecom")) {
        QJsonObject t; t.insert(QStringLiteral("content"), body);
        QJsonObject o; o.insert(QStringLiteral("msgtype"), QStringLiteral("text"));
        o.insert(QStringLiteral("text"), t);
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }
    if (m_type == QStringLiteral("generic")) {
        QJsonObject o;
        o.insert(QStringLiteral("title"), title);
        o.insert(QStringLiteral("text"), text);
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }
    // discord 默认
    QJsonObject o; o.insert(QStringLiteral("content"), body);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

void Notifier::send(const QString &title, const QString &text)
{
    if (!m_enabled || m_url.isEmpty())
        return;
    QNetworkRequest req(QUrl::fromUserInput(m_url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *r = nam()->post(req, payload(title, text));
    QObject::connect(r, &QNetworkReply::finished, r, &QNetworkReply::deleteLater);
    QObject::connect(r, &QNetworkReply::errorOccurred, r, &QNetworkReply::deleteLater);
}
