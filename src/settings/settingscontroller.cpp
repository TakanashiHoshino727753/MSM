/*
 * settingscontroller.cpp —— MSM 自身设置持久化
 * -------------------------------------------------
 * 开机自启（Windows 注册表 Run 键）、语言、WebUI 开关/端口、机器人开关、
 * 默认服务器目录。所有值经 QSettings(MSM) 读写；主题状态在 AppController 持久化。
 */
#include "settingscontroller.h"
#include <QSettings>
#include <QCoreApplication>
#include <QFileInfo>
#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
    QString v;
#ifdef Q_OS_WIN
    HKEY hKey = nullptr;
    LONG res = RegOpenKeyExA(HKEY_CURRENT_USER,
                             "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                             0, KEY_QUERY_VALUE, &hKey);
    if (res == ERROR_SUCCESS) {
        char buf[1024];
        DWORD sz = sizeof(buf);
        if (RegQueryValueExA(hKey, "MSM", nullptr, nullptr,
                             reinterpret_cast<BYTE *>(buf), &sz) == ERROR_SUCCESS)
            v = QString::fromLocal8Bit(buf);
        RegCloseKey(hKey);
    }
#else
    QSettings reg(QString::fromLocal8Bit(RUN_KEY), QSettings::NativeFormat);
    v = reg.value(QStringLiteral("MSM")).toString();
#endif
    // 去掉可能存在的引号（保存时按空格加的），并规范化路径（/ 与 \、大小写）再比较
    v = v.trimmed();
    if (v.startsWith(QLatin1Char('"')) && v.endsWith(QLatin1Char('"')))
        v = v.mid(1, v.size() - 2);
    const QString cur = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    m_autoStart = !v.isEmpty() && QFileInfo(v).canonicalFilePath() == cur;
}

void SettingsController::saveAutoStart()
{
    // 路径含空格时必须加引号，否则 Run 键启动时按空格拆分命令行，自启动失败。
    QString path = QCoreApplication::applicationFilePath();
    if (path.contains(QLatin1Char(' ')))
        path = QStringLiteral("\"%1\"").arg(path);

#ifdef Q_OS_WIN
    // 直接用 Windows API 写 Run 键，避免 QSettings 在 GUI 子系统下偶发的注册表路径歧义。
    HKEY hKey = nullptr;
    LONG res = RegOpenKeyExA(HKEY_CURRENT_USER,
                             "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                             0, KEY_SET_VALUE, &hKey);
    if (res == ERROR_SUCCESS) {
        if (m_autoStart) {
            const QByteArray data = path.toLocal8Bit();
            RegSetValueExA(hKey, "MSM", 0, REG_SZ,
                           reinterpret_cast<const BYTE *>(data.constData()),
                           static_cast<DWORD>(data.size() + 1));
        } else {
            RegDeleteValueA(hKey, "MSM");
        }
        RegCloseKey(hKey);
    }
#else
    QSettings reg(QString::fromLocal8Bit(RUN_KEY), QSettings::NativeFormat);
    if (m_autoStart)
        reg.setValue(QStringLiteral("MSM"), path);
    else
        reg.remove(QStringLiteral("MSM"));
    reg.sync();
#endif
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
