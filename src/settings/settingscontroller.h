#pragma once
#include <QObject>
#include <QString>
#include <QSettings>
#include <QStandardPaths>

// 控制器（MSM 自身）设置逻辑层（C++）：开机自启（Windows 注册表）、
// 界面语言、WebUI 开关与端口、机器人插件开关、默认服务器目录，均通过 QSettings 持久化。
// 主题的深浅色与主色调已在 AppController 中持久化，此处不再重复。
class SettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool webuiEnabled READ webuiEnabled WRITE setWebuiEnabled NOTIFY webuiEnabledChanged)
    Q_PROPERTY(int webuiPort READ webuiPort WRITE setWebuiPort NOTIFY webuiPortChanged)
    Q_PROPERTY(bool botEnabled READ botEnabled WRITE setBotEnabled NOTIFY botEnabledChanged)
    Q_PROPERTY(QString defaultServerDir READ defaultServerDir WRITE setDefaultServerDir NOTIFY defaultServerDirChanged)

public:
    explicit SettingsController(QObject *parent = nullptr);

    bool autoStart() const { return m_autoStart; }
    void setAutoStart(bool v);
    QString language() const { return m_language; }
    void setLanguage(const QString &v);
    bool webuiEnabled() const { return m_webui; }
    void setWebuiEnabled(bool v);
    int webuiPort() const { return m_webuiPort; }
    void setWebuiPort(int v);
    bool botEnabled() const { return m_bot; }
    void setBotEnabled(bool v);

    // 去掉 file:/// 前缀，只保留本地路径（FolderDialog 返回 URL，注册表里也可能存有旧脏数据）
    static QString stripFileUrl(QString path) {
        if (path.startsWith(QStringLiteral("file:///")))
            path = path.mid(8);
        else if (path.startsWith(QStringLiteral("file://")))
            path = path.mid(7);
        return path;
    }
    QString defaultServerDir() const {
        QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
        const QString custom = stripFileUrl(s.value(QStringLiteral("path/serverDir")).toString());
        if (!custom.isEmpty()) return custom;
        return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/MSM");
    }
    void setDefaultServerDir(const QString &v) {
        QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
        s.setValue(QStringLiteral("path/serverDir"), stripFileUrl(v));
        emit defaultServerDirChanged();
    }

    Q_INVOKABLE void apply();   // 持久化所有设置（QSettings + 注册表）

signals:
    void autoStartChanged();
    void languageChanged();
    void webuiEnabledChanged();
    void webuiPortChanged();
    void botEnabledChanged();
    void defaultServerDirChanged();

private:
    void loadAutoStart();
    void saveAutoStart();

    bool m_autoStart = false;
    QString m_language = QStringLiteral("简体中文");
    bool m_webui = false;
    int m_webuiPort = 25575;
    bool m_bot = false;
};
