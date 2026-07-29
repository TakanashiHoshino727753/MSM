#pragma once
#include <QObject>
#include <QString>
#include <QPointer>
#include <QNetworkAccessManager>

// 通用 Webhook 通知器：崩溃 / 启停 / 玩家进服时向 Discord / 企业微信 / 自定义端点 POST JSON。
// 由 main.cpp 在业务信号上调用 send()，并随设置页的配置实时同步。
class Notifier : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QString type READ type WRITE setType NOTIFY typeChanged) // discord | wecom | generic
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool notifyCrash READ notifyCrash WRITE setNotifyCrash NOTIFY notifyCrashChanged)
    Q_PROPERTY(bool notifyState READ notifyState WRITE setNotifyState NOTIFY notifyStateChanged)
    Q_PROPERTY(bool notifyPlayer READ notifyPlayer WRITE setNotifyPlayer NOTIFY notifyPlayerChanged)

public:
    explicit Notifier(QObject *parent = nullptr);

    QString url() const { return m_url; }
    void setUrl(const QString &v);
    QString type() const { return m_type; }
    void setType(const QString &v);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool v);
    bool notifyCrash() const { return m_notifyCrash; }
    void setNotifyCrash(bool v);
    bool notifyState() const { return m_notifyState; }
    void setNotifyState(bool v);
    bool notifyPlayer() const { return m_notifyPlayer; }
    void setNotifyPlayer(bool v);

    // 供 C++ 业务层直接调用；enabled 且 url 非空才真正发送
    Q_INVOKABLE void send(const QString &title, const QString &text);

signals:
    void urlChanged();
    void typeChanged();
    void enabledChanged();
    void notifyCrashChanged();
    void notifyStateChanged();
    void notifyPlayerChanged();

private:
    QByteArray payload(const QString &title, const QString &text) const;
    QNetworkAccessManager *nam();

    QString m_url;
    QString m_type = QStringLiteral("discord");
    bool m_enabled = false;
    bool m_notifyCrash = true;
    bool m_notifyState = true;
    bool m_notifyPlayer = false;
    QPointer<QNetworkAccessManager> m_nam;
};
