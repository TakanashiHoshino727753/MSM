#pragma once
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QMap>

// 一键更新服务端核心 jar（B2）
// 支持 Paper（PaperMC API）与官方 Vanilla（launchermeta）两种类型的版本检测与下载。
// 更新前自动备份旧 server.jar 为 server.jar.bak，再替换为新版。
class UpdateController : public QObject
{
    Q_OBJECT
public:
    explicit UpdateController(QObject *parent = nullptr);

    // 检测是否有可用更新：成功后通过 updateAvailable 信号返回最新版本/下载地址
    Q_INVOKABLE void checkUpdate(const QString &name, const QString &type, const QString &version);
    // 执行更新：下载并替换 server.jar（path 为服务端根目录）
    Q_INVOKABLE void updateJar(const QString &name, const QString &path, const QString &type, const QString &version);

signals:
    // 检测到可用更新：latest 为最新版本/构建号，url 为下载地址
    void updateAvailable(const QString &name, const QString &current, const QString &latest, const QString &url);
    // 更新完成：ok 是否成功，newVersion 为应用后的版本号，msg 为结果描述
    void updateFinished(const QString &name, bool ok, const QString &msg, const QString &newVersion);

private:
    struct Pending {
        QString name, path, type, version;
        bool checkOnly = false;
        QString url, latest;
    };
    void resolvePaper(Pending p);
    void resolveVanilla(Pending p);
    void download(const Pending &p);

    QNetworkAccessManager *m_net = nullptr;
    QMap<QNetworkReply *, Pending> m_pending;
};
