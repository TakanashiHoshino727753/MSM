/*
 * settingscontroller.cpp —— MSM 自身设置持久化
 * -------------------------------------------------
 * 开机自启（Windows 注册表 Run 键）、语言、WebUI 开关/端口、
 * QQ 机器人（NapCat / NoneBot）开关与路径、默认服务器目录。
 * 所有值经 QSettings(MSM) 读写；主题状态在 AppController 持久化。
 */
#include "settingscontroller.h"
#include <QSettings>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QRandomGenerator>
#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static const char *RUN_KEY = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// 生成 32 位十六进制访问令牌
static QString generateWebuiToken()
{
    QString out;
    out.reserve(32);
    QRandomGenerator *rng = QRandomGenerator::system();
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i)
        out.append(QLatin1Char(hex[rng->bounded(16)]));
    return out;
}

SettingsController::SettingsController(QObject *parent) : QObject(parent)
{
    QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
    m_language = s.value(QStringLiteral("app/language"), QStringLiteral("简体中文")).toString();
    m_webui = s.value(QStringLiteral("app/webui"), false).toBool();
    m_webuiPort = s.value(QStringLiteral("app/webuiPort"), 25575).toInt();
    m_webuiToken = s.value(QStringLiteral("app/webuiToken")).toString();
    if (m_webuiToken.isEmpty())
        m_webuiToken = generateWebuiToken();   // 首次运行自动生成，避免无令牌导致接口裸奔
    m_webuiExposeLan = s.value(QStringLiteral("app/webuiExposeLan"), false).toBool();
    m_webuiCertPath = s.value(QStringLiteral("app/webuiCertPath")).toString();
    m_webuiKeyPath = s.value(QStringLiteral("app/webuiKeyPath")).toString();
    m_showOnStartup = s.value(QStringLiteral("app/showOnStartup"), false).toBool();
    m_napcat = s.value(QStringLiteral("app/napcat"), false).toBool();
    m_napcatPath = s.value(QStringLiteral("app/napcatPath")).toString();
    m_nonebot = s.value(QStringLiteral("app/nonebot"), false).toBool();
    m_nonebotDir = s.value(QStringLiteral("app/nonebotDir")).toString();
    m_botUsageInterval = s.value(QStringLiteral("app/botUsageInterval"), 300).toInt();
    m_botLinkedStart = s.value(QStringLiteral("app/botLinkedStart"), false).toBool();
    m_bot = s.value(QStringLiteral("app/botEnabled"), m_napcat && m_nonebot).toBool();

    m_webhookUrl = s.value(QStringLiteral("app/webhookUrl")).toString();
    m_webhookType = s.value(QStringLiteral("app/webhookType"), QStringLiteral("discord")).toString();
    m_webhookEnabled = s.value(QStringLiteral("app/webhookEnabled"), false).toBool();
    m_webhookCrash = s.value(QStringLiteral("app/webhookCrash"), true).toBool();
    m_webhookState = s.value(QStringLiteral("app/webhookState"), true).toBool();
    m_webhookPlayer = s.value(QStringLiteral("app/webhookPlayer"), false).toBool();

    qDebug() << "[SET] raw botLinkedStart=" << s.value(QStringLiteral("app/botLinkedStart"))
             << "toBool=" << m_botLinkedStart
             << "botEnabled=" << s.value(QStringLiteral("app/botEnabled"))
             << "toBool=" << m_bot
             << "napcat=" << s.value(QStringLiteral("app/napcat")) << "toBool=" << m_napcat
             << "nonebot=" << s.value(QStringLiteral("app/nonebot")) << "toBool=" << m_nonebot;

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
    // 用 toNativeSeparators 把路径统一成 Windows 原生反斜杠，避免注册表里出现 "/"。
    QString path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
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
void SettingsController::setWebuiToken(const QString &v)
{
    const QString t = v.trimmed();
    if (m_webuiToken != t) { m_webuiToken = t; emit webuiTokenChanged(); }
}
void SettingsController::setWebuiExposeLan(bool v)
{
    if (m_webuiExposeLan != v) { m_webuiExposeLan = v; emit webuiExposeLanChanged(); }
}
void SettingsController::setWebuiCertPath(const QString &v)
{
    const QString p = stripFileUrl(v);
    if (m_webuiCertPath != p) { m_webuiCertPath = p; emit webuiCertPathChanged(); }
}
void SettingsController::setWebuiKeyPath(const QString &v)
{
    const QString p = stripFileUrl(v);
    if (m_webuiKeyPath != p) { m_webuiKeyPath = p; emit webuiKeyPathChanged(); }
}

void SettingsController::setShowOnStartup(bool v)
{
    if (m_showOnStartup != v) { m_showOnStartup = v; emit showOnStartupChanged(); }
}
void SettingsController::setWebhookUrl(const QString &v)
{
    if (m_webhookUrl != v) { m_webhookUrl = v; emit webhookUrlChanged(); }
}
void SettingsController::setWebhookType(const QString &v)
{
    if (m_webhookType != v) { m_webhookType = v; emit webhookTypeChanged(); }
}
void SettingsController::setWebhookEnabled(bool v)
{
    if (m_webhookEnabled != v) { m_webhookEnabled = v; emit webhookEnabledChanged(); }
}
void SettingsController::setWebhookCrash(bool v)
{
    if (m_webhookCrash != v) { m_webhookCrash = v; emit webhookCrashChanged(); }
}
void SettingsController::setWebhookState(bool v)
{
    if (m_webhookState != v) { m_webhookState = v; emit webhookStateChanged(); }
}
void SettingsController::setWebhookPlayer(bool v)
{
    if (m_webhookPlayer != v) { m_webhookPlayer = v; emit webhookPlayerChanged(); }
}
void SettingsController::regenerateWebuiToken()
{
    m_webuiToken = generateWebuiToken();
    emit webuiTokenChanged();
}
void SettingsController::setNapcatEnabled(bool v)
{
    if (m_napcat != v) { m_napcat = v; emit napcatEnabledChanged(); }
}
void SettingsController::setNapcatPath(const QString &v)
{
    const QString p = stripFileUrl(v);
    if (m_napcatPath != p) { m_napcatPath = p; emit napcatPathChanged(); }
}
void SettingsController::setNonebotEnabled(bool v)
{
    if (m_nonebot != v) { m_nonebot = v; emit nonebotEnabledChanged(); }
}
void SettingsController::setNonebotDir(const QString &v)
{
    const QString p = stripFileUrl(v);
    if (m_nonebotDir != p) { m_nonebotDir = p; emit nonebotDirChanged(); }
}
void SettingsController::setBotUsageInterval(int v)
{
    const int n = qMax(0, v);
    if (m_botUsageInterval != n) { m_botUsageInterval = n; emit botUsageIntervalChanged(); }
}

void SettingsController::setBotLinkedStart(bool v)
{
    if (m_botLinkedStart != v) { m_botLinkedStart = v; emit botLinkedStartChanged(); }
}

void SettingsController::setBotEnabled(bool v)
{
    if (m_bot == v)
        return;
    m_bot = v;
    // 主开关同时驱动两个子开关的内部状态，保持 /api/status 等逻辑一致
    m_napcat = v;
    m_nonebot = v;
    emit botEnabledChanged();
    emit napcatEnabledChanged();
    emit nonebotEnabledChanged();
}

void SettingsController::apply()
{
    QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
    s.setValue(QStringLiteral("app/language"), m_language);
    s.setValue(QStringLiteral("app/webui"), m_webui);
    s.setValue(QStringLiteral("app/webuiPort"), m_webuiPort);
    s.setValue(QStringLiteral("app/webuiToken"), m_webuiToken);
    s.setValue(QStringLiteral("app/webuiExposeLan"), m_webuiExposeLan);
    s.setValue(QStringLiteral("app/webuiCertPath"), m_webuiCertPath);
    s.setValue(QStringLiteral("app/webuiKeyPath"), m_webuiKeyPath);
    s.setValue(QStringLiteral("app/showOnStartup"), m_showOnStartup);
    s.setValue(QStringLiteral("app/napcat"), m_napcat);
    s.setValue(QStringLiteral("app/napcatPath"), m_napcatPath);
    s.setValue(QStringLiteral("app/nonebot"), m_nonebot);
    s.setValue(QStringLiteral("app/nonebotDir"), m_nonebotDir);
    s.setValue(QStringLiteral("app/botUsageInterval"), m_botUsageInterval);
    s.setValue(QStringLiteral("app/botLinkedStart"), m_botLinkedStart);
    s.setValue(QStringLiteral("app/botEnabled"), m_bot);
    s.setValue(QStringLiteral("app/webhookUrl"), m_webhookUrl);
    s.setValue(QStringLiteral("app/webhookType"), m_webhookType);
    s.setValue(QStringLiteral("app/webhookEnabled"), m_webhookEnabled);
    s.setValue(QStringLiteral("app/webhookCrash"), m_webhookCrash);
    s.setValue(QStringLiteral("app/webhookState"), m_webhookState);
    s.setValue(QStringLiteral("app/webhookPlayer"), m_webhookPlayer);

    s.sync();
    saveAutoStart();
}
