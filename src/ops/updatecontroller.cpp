/*
 * updatecontroller.cpp —— 一键更新服务端核心 jar（B2）
 * Paper: https://api.papermc.io/v2/projects/paper/versions/{ver}/builds
 * Vanilla: https://launchermeta.mojang.com/mc/game/version_manifest.json
 */
#include "updatecontroller.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

UpdateController::UpdateController(QObject *parent) : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);
}

void UpdateController::checkUpdate(const QString &name, const QString &type, const QString &version)
{
    Pending p;
    p.name = name; p.type = type; p.version = version; p.checkOnly = true;
    if (type == QStringLiteral("paper"))
        resolvePaper(p);
    else if (type == QStringLiteral("vanilla"))
        resolveVanilla(p);
    else
        emit updateAvailable(name, version, version,
                             QStringLiteral("暂不支持该服务端类型（仅支持 Paper / Vanilla）的自动更新"));
}

void UpdateController::updateJar(const QString &name, const QString &path, const QString &type, const QString &version)
{
    Pending p;
    p.name = name; p.path = path; p.type = type; p.version = version; p.checkOnly = false;
    if (type == QStringLiteral("paper"))
        resolvePaper(p);
    else if (type == QStringLiteral("vanilla"))
        resolveVanilla(p);
    else
        emit updateFinished(name, false,
                            QStringLiteral("暂不支持该服务端类型（仅支持 Paper / Vanilla）的自动更新"), version);
}

void UpdateController::resolvePaper(Pending p)
{
    const QString url = QStringLiteral("https://api.papermc.io/v2/projects/paper/versions/%1/builds").arg(p.version);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MSM/1.0"));
    QNetworkReply *r = m_net->get(req);
    m_pending.insert(r, p);
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        Pending p = m_pending.take(r);
        const QByteArray data = r->readAll();
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            const QString msg = QStringLiteral("检测更新失败：") + r->errorString();
            if (p.checkOnly) emit updateAvailable(p.name, p.version, p.version, msg);
            else emit updateFinished(p.name, false, msg, p.version);
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject() || !doc.object().contains(QStringLiteral("builds"))) {
            const QString msg = QStringLiteral("无法解析 Paper 构建列表");
            if (p.checkOnly) emit updateAvailable(p.name, p.version, p.version, msg);
            else emit updateFinished(p.name, false, msg, p.version);
            return;
        }
        const QJsonArray builds = doc.object().value(QStringLiteral("builds")).toArray();
        if (builds.isEmpty()) {
            const QString msg = QStringLiteral("Paper 无可用构建");
            if (p.checkOnly) emit updateAvailable(p.name, p.version, p.version, msg);
            else emit updateFinished(p.name, false, msg, p.version);
            return;
        }
        // 取构建号最大者
        int latestBuild = 0;
        for (const QJsonValue &b : builds) {
            const int n = b.toObject().value(QStringLiteral("build")).toInt();
            if (n > latestBuild) latestBuild = n;
        }
        p.latest = QString::number(latestBuild);
        p.url = QStringLiteral("https://api.papermc.io/v2/projects/paper/versions/%1/builds/%2/downloads/paper-%1-%2.jar")
                .arg(p.version).arg(latestBuild);
        if (p.checkOnly)
            emit updateAvailable(p.name, p.version, p.latest, p.url);
        else
            download(p);
    });
}

void UpdateController::resolveVanilla(Pending p)
{
    const QString manifestUrl = QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest.json");
    QNetworkRequest req{QUrl(manifestUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MSM/1.0"));
    QNetworkReply *r = m_net->get(req);
    m_pending.insert(r, p);
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        Pending p = m_pending.take(r);
        const QByteArray data = r->readAll();
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            const QString msg = QStringLiteral("检测更新失败：") + r->errorString();
            if (p.checkOnly) emit updateAvailable(p.name, p.version, p.version, msg);
            else emit updateFinished(p.name, false, msg, p.version);
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        const QJsonArray versions = doc.object().value(QStringLiteral("versions")).toArray();
        QString versionUrl;
        for (const QJsonValue &v : versions) {
            if (v.toObject().value(QStringLiteral("id")).toString() == p.version) {
                versionUrl = v.toObject().value(QStringLiteral("url")).toString();
                break;
            }
        }
        if (versionUrl.isEmpty()) {
            const QString msg = QStringLiteral("未找到版本 %1").arg(p.version);
            if (p.checkOnly) emit updateAvailable(p.name, p.version, p.version, msg);
            else emit updateFinished(p.name, false, msg, p.version);
            return;
        }
        QNetworkRequest req2{QUrl(versionUrl)};
        req2.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MSM/1.0"));
        QNetworkReply *r2 = m_net->get(req2);
        m_pending.insert(r2, p);
        connect(r2, &QNetworkReply::finished, this, [this, r2]() {
            Pending p = m_pending.take(r2);
            const QByteArray d2 = r2->readAll();
            r2->deleteLater();
            if (r2->error() != QNetworkReply::NoError) {
                const QString msg = QStringLiteral("检测更新失败：") + r2->errorString();
                if (p.checkOnly) emit updateAvailable(p.name, p.version, p.version, msg);
                else emit updateFinished(p.name, false, msg, p.version);
                return;
            }
            const QJsonDocument d = QJsonDocument::fromJson(d2);
            const QString jarUrl = d.object().value(QStringLiteral("downloads")).toObject()
                    .value(QStringLiteral("server")).toObject()
                    .value(QStringLiteral("url")).toString();
            if (jarUrl.isEmpty()) {
                const QString msg = QStringLiteral("未找到服务端下载地址");
                if (p.checkOnly) emit updateAvailable(p.name, p.version, p.version, msg);
                else emit updateFinished(p.name, false, msg, p.version);
                return;
            }
            p.url = jarUrl;
            p.latest = p.version;   // 官方同版本号，jar 可能已修复重发
            if (p.checkOnly)
                emit updateAvailable(p.name, p.version, p.latest, p.url);
            else
                download(p);
        });
    });
}

void UpdateController::download(const Pending &p)
{
    if (p.path.isEmpty() || p.url.isEmpty()) {
        emit updateFinished(p.name, false, QStringLiteral("路径或下载地址为空"), p.version);
        return;
    }
    const QString jar = p.path + QStringLiteral("/server.jar");
    if (!QFile::exists(jar)) {
        emit updateFinished(p.name, false, QStringLiteral("未找到 server.jar，无法更新"), p.version);
        return;
    }
    QNetworkRequest req(QUrl(p.url));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MSM/1.0"));
    QNetworkReply *r = m_net->get(req);
    m_pending.insert(r, p);
    connect(r, &QNetworkReply::finished, this, [this, r, jar]() {
        Pending p = m_pending.take(r);
        const QByteArray data = r->readAll();
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError || data.size() < 1024 * 1024) {
            const QString msg = QStringLiteral("下载失败：") + r->errorString();
            emit updateFinished(p.name, false, msg, p.version);
            return;
        }
        // 先备份旧 jar
        const QString bak = jar + QStringLiteral(".bak");
        QFile::remove(bak);
        QFile::copy(jar, bak);
        // 写入新 jar（先临时文件再替换，避免半截文件）
        const QString tmp = p.path + QStringLiteral("/.msm_update.jar");
        {
            QFile f(tmp);
            if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
                emit updateFinished(p.name, false, QStringLiteral("写入临时文件失败"), p.version);
                return;
            }
            f.close();
        }
        QFile::remove(jar);
        if (!QFile::rename(tmp, jar)) {
            emit updateFinished(p.name, false, QStringLiteral("替换 server.jar 失败"), p.version);
            return;
        }
        emit updateFinished(p.name, true,
                            QStringLiteral("已更新到 %1（旧版本备份为 server.jar.bak）").arg(p.latest), p.latest);
    });
}
