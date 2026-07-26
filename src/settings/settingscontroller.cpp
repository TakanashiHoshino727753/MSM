/*
 * settingscontroller.cpp —— MSM 自身设置持久化
 * -------------------------------------------------
 * 开机自启（Windows 注册表 Run 键）、语言、WebUI 开关/端口、机器人开关、
 * 默认服务器目录。所有值经 QSettings(MSM) 读写；主题状态在 AppController 持久化。
 */
#include "settingscontroller.h"
#include <QSettings>
#include <QCoreApplication>

static const char *RUN_KEY = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";

SettingsController::SettingsController(QObject *parent) : QObject(parent)
{
    QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
    m_language = s.value(QStringLiteral("app/language"), QStringLiteral("简体中文")).toString();
    m_webui = s.value(QStringLiteral("app/webui"), false).toBool();
    m_webuiPort = s.value(QStringLiteral("app/webuiPort"), 25575).toInt();
    m_bot = s.value(QStringLiteral("app/bot"), false).toBool();
    loadAutoStart();
}

void SettingsController::loadAutoStart()
{
    QSettings reg(QString::fromLocal8Bit(RUN_KEY), QSettings::NativeFormat);
    m_autoStart = reg.contains(QStringLiteral("MSM")) &&
                  reg.value(QStringLiteral("MSM")).toString() == QCoreApplication::applicationFilePath();
}

void SettingsController::saveAutoStart()
{
    QSettings reg(QString::fromLocal8Bit(RUN_KEY), QSettings::NativeFormat);
    if (m_autoStart)
        reg.setValue(QStringLiteral("MSM"), QCoreApplication::applicationFilePath());
    else
        reg.remove(QStringLiteral("MSM"));
}

void SettingsController::setAutoStart(bool v)
{
    if (m_autoStart != v) { m_autoStart = v; emit autoStartChanged(); }
}
void SettingsController::setLanguage(const QString &v)
{
    if (m_language != v) { m_language = v; emit languageChanged(); }
}
void SettingsController::setWebuiEnabled(bool v)
{
    if (m_webui != v) { m_webui = v; emit webuiEnabledChanged(); }
}
void SettingsController::setWebuiPort(int v)
{
    if (m_webuiPort != v) { m_webuiPort = v; emit webuiPortChanged(); }
}
void SettingsController::setBotEnabled(bool v)
{
    if (m_bot != v) { m_bot = v; emit botEnabledChanged(); }
}

void SettingsController::apply()
{
    QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
    s.setValue(QStringLiteral("app/language"), m_language);
    s.setValue(QStringLiteral("app/webui"), m_webui);
    s.setValue(QStringLiteral("app/webuiPort"), m_webuiPort);
    s.setValue(QStringLiteral("app/bot"), m_bot);
    s.sync();
    saveAutoStart();
}
