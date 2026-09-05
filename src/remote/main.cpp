// MSM_Remote main.cpp —— 远控端入口。本地端的远程版：UI 与本地端一致，操作经 HTTPS REST 发给本地端执行。
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>
#include <QTranslator>
#include <QtQml>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "remoteclient.h"
#include "appearancesettings.h"
#include "msm_logging.h"   // 统一日志处理器（desktop/remote 共用）

int main(int argc, char *argv[])
{
    // 统一日志：所有级别写入 logs/msm-remote.log；Debug 构建额外回显控制台并写 logs/msm-remote-debug.log
    msmInstallLogging(QStringLiteral("msm-remote.log"), QStringLiteral("msm-remote-debug.log"));

    // 必须用 openssl 后端（MinGW 下 schannel 偶发 device not open）；本地端自签证书
    qputenv("QT_TLS_BACKEND", "openssl");

    QApplication app(argc, argv);
    app.setOrganizationName("MSM");
    app.setApplicationName("MSM_Remote");
    app.setWindowIcon(QIcon(":/icons/app.ico"));

#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#endif

    QQmlApplicationEngine engine;

    // 远控端核心：连接本地端控制器并转发操作
    RemoteClient remote;
    engine.rootContext()->setContextProperty("remote", &remote);

    // 外观个性化（背景图/背景音乐）
    AppearanceSettings appearance;
    engine.rootContext()->setContextProperty("appearance", &appearance);

    // 背景音乐播放：资源为用户选文件复制到 backgroundmusic/bgm.<ext>（回退 qrc:/audio/bgm）
    {
        QMediaPlayer *bgm = new QMediaPlayer(&app);
        bgm->setAudioOutput(new QAudioOutput(&app));
        bgm->setLoops(QMediaPlayer::Infinite);
        const QString skinBgm = AppearanceSettings::bgmDir() + QStringLiteral("/") + appearance.bgmPath();
        if (!appearance.bgmPath().isEmpty() && QFile::exists(skinBgm))
            bgm->setSource(QUrl::fromLocalFile(skinBgm));
        else if (QFile::exists(QStringLiteral(":/audio/bgm.mp3")))
            bgm->setSource(QUrl(QStringLiteral("qrc:/audio/bgm.mp3")));
        if (bgm->audioOutput())
            bgm->audioOutput()->setVolume(appearance.bgmVolume());
        if (appearance.bgmEnabled() && !bgm->source().isEmpty())
            bgm->play();
        // 开关/音量变化实时联动
        QObject::connect(&appearance, &AppearanceSettings::bgmEnabledChanged, &app, [bgm, &appearance]() {
            if (appearance.bgmEnabled() && !bgm->source().isEmpty())
                bgm->play();
            else
                bgm->stop();
        });
        QObject::connect(&appearance, &AppearanceSettings::bgmVolumeChanged, &app, [bgm, &appearance]() {
            if (bgm->audioOutput())
                bgm->audioOutput()->setVolume(appearance.bgmVolume());
        });
        QObject::connect(&appearance, &AppearanceSettings::bgmPathChanged, &app, [bgm, &appearance]() {
            const QString p = appearance.bgmPath().isEmpty() ? QString()
                : (AppearanceSettings::bgmDir() + QStringLiteral("/") + appearance.bgmPath());
            if (!p.isEmpty() && QFile::exists(p)) {
                bgm->setSource(QUrl::fromLocalFile(p));
                if (appearance.bgmEnabled())
                    bgm->play();
            }
        });
    }

    // 简易暗色主题（与原 MSM 风格统一）
    qmlRegisterSingletonType(QUrl("qrc:/Theme.qml"), "Theme", 1, 0, "Theme");

    const QUrl url(QStringLiteral("qrc:/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    engine.load(url);

    // 系统托盘（常驻；关窗=收起，退出走托盘菜单）
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        QSystemTrayIcon tray(&app);
        tray.setIcon(QIcon(":/icons/app.ico"));
        tray.setToolTip("MSM 远控端");
        QMenu menu;
        QAction *showAct = menu.addAction("显示");
        QAction *quitAct = menu.addAction("退出");
        QObject::connect(showAct, &QAction::triggered, &app, [&engine]() {
            if (auto *w = engine.rootObjects().first())
                w->setProperty("visible", true);
        });
        QObject::connect(quitAct, &QAction::triggered, &app, &QApplication::quit);
        tray.setContextMenu(&menu);
        tray.show();
    }

    return app.exec();
}
