#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>

// 控制器（MSM 自身）设置逻辑层（C++）：开机自启（Windows 注册表）、
// 界面语言、WebUI 开关与端口、QQ 机器人（NapCat / NoneBot）开关与路径、
// 默认服务器目录，均通过 QSettings 持久化。主题的深浅色与主色调已在 AppController 中持久化，此处不再重复。
class SettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool webuiEnabled READ webuiEnabled WRITE setWebuiEnabled NOTIFY webuiEnabledChanged)
    Q_PROPERTY(int webuiPort READ webuiPort WRITE setWebuiPort NOTIFY webuiPortChanged)
    // 访问令牌：所有 /api/* 必须携带，否则 401。自动生成，可在设置页查看/重置。
    Q_PROPERTY(QString webuiToken READ webuiToken WRITE setWebuiToken NOTIFY webuiTokenChanged)
    // 是否把 WebUI 暴露到局域网（0.0.0.0）。默认 false=仅本机(127.0.0.1)。开启时必须带令牌。
    Q_PROPERTY(bool webuiExposeLan READ webuiExposeLan WRITE setWebuiExposeLan NOTIFY webuiExposeLanChanged)
    // 自定义证书（PEM）路径；留空则使用自动生成的自签证书（存入 Windows 证书存储）。
    Q_PROPERTY(QString webuiCertPath READ webuiCertPath WRITE setWebuiCertPath NOTIFY webuiCertPathChanged)
    Q_PROPERTY(QString webuiKeyPath READ webuiKeyPath WRITE setWebuiKeyPath NOTIFY webuiKeyPathChanged)
    // 启动时是否显示主窗口。默认 false=启动仅驻留托盘、不弹窗（点托盘“显示主窗口”可打开）
    Q_PROPERTY(bool showOnStartup READ showOnStartup WRITE setShowOnStartup NOTIFY showOnStartupChanged)

    // Webhook 通知（崩溃 / 启停 / 玩家进服推送）：URL、类型(discord|wecom|generic)、总开关与三类事件开关
    Q_PROPERTY(QString webhookUrl READ webhookUrl WRITE setWebhookUrl NOTIFY webhookUrlChanged)
    Q_PROPERTY(QString webhookType READ webhookType WRITE setWebhookType NOTIFY webhookTypeChanged)
    Q_PROPERTY(bool webhookEnabled READ webhookEnabled WRITE setWebhookEnabled NOTIFY webhookEnabledChanged)
    Q_PROPERTY(bool webhookCrash READ webhookCrash WRITE setWebhookCrash NOTIFY webhookCrashChanged)
    Q_PROPERTY(bool webhookState READ webhookState WRITE setWebhookState NOTIFY webhookStateChanged)
    Q_PROPERTY(bool webhookPlayer READ webhookPlayer WRITE setWebhookPlayer NOTIFY webhookPlayerChanged)

    Q_PROPERTY(bool napcatEnabled READ napcatEnabled WRITE setNapcatEnabled NOTIFY napcatEnabledChanged)
    Q_PROPERTY(QString napcatPath READ napcatPath WRITE setNapcatPath NOTIFY napcatPathChanged)
    Q_PROPERTY(bool nonebotEnabled READ nonebotEnabled WRITE setNonebotEnabled NOTIFY nonebotEnabledChanged)
    Q_PROPERTY(QString nonebotDir READ nonebotDir WRITE setNonebotDir NOTIFY nonebotDirChanged)
    // 服务器占用推送间隔（秒），0 表示关闭；仅 NoneBot 运行时按此间隔向 QQ 推送占用概览
    Q_PROPERTY(int botUsageInterval READ botUsageInterval WRITE setBotUsageInterval NOTIFY botUsageIntervalChanged)
    // 联动启动：开启时由 MSM 拉起 NapCat / NoneBot 进程；关闭则 MSM 仅开放控制通道，
    // 由用户自行启动机器人并主动连入（默认关闭）。
    Q_PROPERTY(bool botLinkedStart READ botLinkedStart WRITE setBotLinkedStart NOTIFY botLinkedStartChanged)
    Q_PROPERTY(bool botEnabled READ botEnabled WRITE setBotEnabled NOTIFY botEnabledChanged)

    Q_PROPERTY(QString defaultServerDir READ defaultServerDir WRITE setDefaultServerDir NOTIFY defaultServerDirChanged)

    // 外观个性化：用户从电脑中选择背景图/背景音乐文件，MSM 复制到 dataDir/skin/ 下持久使用
    Q_PROPERTY(bool bgEnabled READ bgEnabled WRITE setBgEnabled NOTIFY bgEnabledChanged)
    Q_PROPERTY(QString bgImagePath READ bgImagePath WRITE setBgImagePath NOTIFY bgImagePathChanged)
    Q_PROPERTY(bool bgmEnabled READ bgmEnabled WRITE setBgmEnabled NOTIFY bgmEnabledChanged)
    Q_PROPERTY(QString bgmPath READ bgmPath WRITE setBgmPath NOTIFY bgmPathChanged)
    Q_PROPERTY(double bgmVolume READ bgmVolume WRITE setBgmVolume NOTIFY bgmVolumeChanged)

    // 壁纸轮换：文件夹可包含多张壁纸，支持顺序/随机播放，用户可在程序内调整顺序
    Q_PROPERTY(QString bgImageFolder READ bgImageFolder WRITE setBgImageFolder NOTIFY bgImageFolderChanged)
    Q_PROPERTY(QStringList bgImageList READ bgImageList NOTIFY bgImageListChanged)
    Q_PROPERTY(QString bgImageMode READ bgImageMode WRITE setBgImageMode NOTIFY bgImageModeChanged)   // "sequential" | "random"
    Q_PROPERTY(int bgImageInterval READ bgImageInterval WRITE setBgImageInterval NOTIFY bgImageIntervalChanged)  // 数值（按 bgImageIntervalUnit 解释）
    Q_PROPERTY(QString bgImageIntervalUnit READ bgImageIntervalUnit WRITE setBgImageIntervalUnit NOTIFY bgImageIntervalUnitChanged)  // 轮换间隔单位：sec/min/hour/day
    Q_PROPERTY(int bgImageIndex READ bgImageIndex WRITE setBgImageIndex NOTIFY bgImageIndexChanged)

public:
    explicit SettingsController(QObject *parent = nullptr);

    // 皮肤目录：dataDir()/skin/，MSM 把用户选择的背景图/音乐复制到这里
    static QString skinDir() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
               + QStringLiteral("/MSM/skin");
    }
    // 背景图目录：程序同目录/backgroundpic/（用户从电脑选的图片复制到此处，供 MainWindow 铺背景）
    static QString bgImageDir() {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/backgroundpic");
    }
    // 背景音乐目录：程序同目录/backgroundmusic/（用户从电脑选的音乐复制到此处，供 BGM 循环播放）
    static QString bgmDir() {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/backgroundmusic");
    }

    bool autoStart() const { return m_autoStart; }
    void setAutoStart(bool v);
    QString language() const { return m_language; }
    void setLanguage(const QString &v);
    bool webuiEnabled() const { return m_webui; }
    void setWebuiEnabled(bool v);
    int webuiPort() const { return m_webuiPort; }
    void setWebuiPort(int v);
    QString webuiToken() const { return m_webuiToken; }
    void setWebuiToken(const QString &v);
    bool webuiExposeLan() const { return m_webuiExposeLan; }
    void setWebuiExposeLan(bool v);
    QString webuiCertPath() const { return m_webuiCertPath; }
    void setWebuiCertPath(const QString &v);
    QString webuiKeyPath() const { return m_webuiKeyPath; }
    void setWebuiKeyPath(const QString &v);

    bool showOnStartup() const { return m_showOnStartup; }
    void setShowOnStartup(bool v);

    QString webhookUrl() const { return m_webhookUrl; }
    void setWebhookUrl(const QString &v);
    QString webhookType() const { return m_webhookType; }
    void setWebhookType(const QString &v);
    bool webhookEnabled() const { return m_webhookEnabled; }
    void setWebhookEnabled(bool v);
    bool webhookCrash() const { return m_webhookCrash; }
    void setWebhookCrash(bool v);
    bool webhookState() const { return m_webhookState; }
    void setWebhookState(bool v);
    bool webhookPlayer() const { return m_webhookPlayer; }
    void setWebhookPlayer(bool v);

    bool napcatEnabled() const { return m_napcat; }
    void setNapcatEnabled(bool v);
    QString napcatPath() const { return m_napcatPath; }
    void setNapcatPath(const QString &v);
    bool nonebotEnabled() const { return m_nonebot; }
    void setNonebotEnabled(bool v);
    QString nonebotDir() const { return m_nonebotDir; }
    void setNonebotDir(const QString &v);
    int botUsageInterval() const { return m_botUsageInterval; }
    void setBotUsageInterval(int v);
    bool botLinkedStart() const { return m_botLinkedStart; }
    void setBotLinkedStart(bool v);
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
        QSettings s;
        const QString custom = stripFileUrl(s.value(QStringLiteral("path/serverDir")).toString());
        if (!custom.isEmpty()) return custom;
#ifdef Q_OS_LINUX
        return QDir::cleanPath(QDir::homePath() + QStringLiteral("/MSM/Servers"));
#else
        return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/MSM");
#endif
    }
    void setDefaultServerDir(const QString &v) {
        QSettings s;
        s.setValue(QStringLiteral("path/serverDir"), stripFileUrl(v));
        emit defaultServerDirChanged();
    }

    bool bgEnabled() const { return m_bgEnabled; }
    Q_INVOKABLE void setBgEnabled(bool v);
    QString bgImagePath() const { return m_bgImagePath; }
    Q_INVOKABLE void setBgImagePath(const QString &v);
    bool bgmEnabled() const { return m_bgmEnabled; }
    void setBgmEnabled(bool v);
    QString bgmPath() const { return m_bgmPath; }
    void setBgmPath(const QString &v);
    double bgmVolume() const { return m_bgmVolume; }
    void setBgmVolume(double v);

    QString bgImageFolder() const { return m_bgImageFolder; }
    Q_INVOKABLE void setBgImageFolder(const QString &v);
    QStringList bgImageList() const { return m_bgImageList; }
    QString bgImageMode() const { return m_bgImageMode; }
    Q_INVOKABLE void setBgImageMode(const QString &v);
    int bgImageInterval() const { return m_bgImageInterval; }
    Q_INVOKABLE void setBgImageInterval(int v);
    QString bgImageIntervalUnit() const { return m_bgImageIntervalUnit; }
    Q_INVOKABLE void setBgImageIntervalUnit(const QString &v);
    int bgImageIndex() const { return m_bgImageIndex; }
    Q_INVOKABLE void setBgImageIndex(int v);
    // 添加多张图片到播放列表（folder 模式下也适用）；去重、非空则自动开启背景图
    Q_INVOKABLE void addBgImages(const QStringList &files);
    Q_INVOKABLE void removeBgImage(int index);
    Q_INVOKABLE void moveBgImage(int from, int to);   // 调整播放顺序（用户可在程序内重排）
    Q_INVOKABLE void clearBgImages();

    // 把用户从电脑中选中的背景图/音乐文件复制到 skinDir()（覆盖式），
    // 成功返回 true。QML 通过 FileDialog 拿到本地路径后调用。
    Q_INVOKABLE bool importSkinImage(const QString &srcFile);
    Q_INVOKABLE bool importSkinMusic(const QString &srcFile);
    // 清除已导入的皮肤（删除 skinDir 下对应文件并清空设置）
    Q_INVOKABLE void clearSkinImage();
    Q_INVOKABLE void clearSkinMusic();

    Q_INVOKABLE void apply();   // 持久化所有设置（QSettings + 注册表）
    Q_INVOKABLE void regenerateWebuiToken();   // 重新生成访问令牌

signals:
    void autoStartChanged();
    void languageChanged();
    void webuiEnabledChanged();
    void webuiPortChanged();
    void webuiTokenChanged();
    void webuiExposeLanChanged();
    void webuiCertPathChanged();
    void webuiKeyPathChanged();
    void showOnStartupChanged();

    void webhookUrlChanged();
    void webhookTypeChanged();
    void webhookEnabledChanged();
    void webhookCrashChanged();
    void webhookStateChanged();
    void webhookPlayerChanged();

    void napcatEnabledChanged();
    void napcatPathChanged();
    void nonebotEnabledChanged();
    void nonebotDirChanged();
    void botUsageIntervalChanged();
    void botLinkedStartChanged();
    void botEnabledChanged();

    void defaultServerDirChanged();

    void bgEnabledChanged();
    void bgImagePathChanged();
    void bgmEnabledChanged();
    void bgmPathChanged();
    void bgmVolumeChanged();

    void bgImageFolderChanged();
    void bgImageListChanged();
    void bgImageModeChanged();
    void bgImageIntervalChanged();
    void bgImageIntervalUnitChanged();
    void bgImageIndexChanged();

private:
    void loadAutoStart();
    void saveAutoStart();

    bool m_autoStart = false;
    QString m_language = QStringLiteral("简体中文");
    bool m_webui = false;
    int m_webuiPort = 25575;
    QString m_webuiToken;
    bool m_webuiExposeLan = false;
    QString m_webuiCertPath;
    QString m_webuiKeyPath;
    bool m_showOnStartup = false;

    QString m_webhookUrl;
    QString m_webhookType = QStringLiteral("discord");
    bool m_webhookEnabled = false;
    bool m_webhookCrash = true;
    bool m_webhookState = true;
    bool m_webhookPlayer = false;

    bool m_napcat = false;
    QString m_napcatPath;
    bool m_nonebot = false;
    QString m_nonebotDir;
    int m_botUsageInterval = 300;
    bool m_botLinkedStart = false;
    bool m_bot = false;

    bool m_bgEnabled = false;
    QString m_bgImagePath;
    bool m_bgmEnabled = false;
    QString m_bgmPath;
    double m_bgmVolume = 0.5;

    // 壁纸轮换
    QString m_bgImageFolder;
    QStringList m_bgImageList;
    QString m_bgImageMode = QStringLiteral("sequential");
    int m_bgImageInterval = 10;   // 数值（配合单位解释）
    int m_bgImageIndex = 0;
    QString m_bgImageIntervalUnit = QStringLiteral("min");   // 轮换间隔单位：sec/min/hour/day

};
