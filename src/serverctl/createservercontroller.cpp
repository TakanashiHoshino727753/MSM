/*
 * createservercontroller.cpp —— 创建服务器向导逻辑
 * -------------------------------------------------
 * 负责版本拉取、目录推导、下载、eula 写入、入列表，以及模组服多加载器安装与打包。
 * 大量使用 Qt 信号/回调闭包串联异步网络与进程步骤；分阶段进度（beginStages）
 * 防止进度条跳变；模组服安装期间使用临时 Java 并在完成后清理。
 */
#include "createservercontroller.h"
#include "downloadmanager.h"
#include "servermanager.h"
#include "javamanager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QPair>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QStandardPaths>
#include <QEventLoop>
#include <algorithm>
#include <QVersionNumber>
#include <QTimer>
#include <functional>
#include <memory>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>

CreateServerController::CreateServerController(DownloadManager *dm, ServerManager *sm,
                                               JavaManager *java, QObject *parent)
    : QObject(parent), m_dm(dm), m_sm(sm), m_java(java)
{
    m_nam = new QNetworkAccessManager(this);
    if (m_dm) {
        connect(m_dm, &DownloadManager::progress, this, &CreateServerController::onDownloadProgress);
        connect(m_dm, &DownloadManager::finished, this, &CreateServerController::onDownloadFinished);
        connect(m_dm, &DownloadManager::error, this, &CreateServerController::onDownloadError);
    }
}

QStringList CreateServerController::types() const
{
    // Purpur 已下架，由官方原版 Vanilla 取代；Fabric/Forge 合并为“模组服”
    return { QStringLiteral("Paper"), QStringLiteral("Vanilla"), QStringLiteral("模组服") };
}

QStringList CreateServerController::modLoaders() const
{
    // 模组服可勾选的加载器（顺序即 UI 展示顺序）
    return { QStringLiteral("forge"), QStringLiteral("fabric"), QStringLiteral("neoforge") };
}

QString CreateServerController::loaderLabel(const QString &loader) const
{
    if (loader == QStringLiteral("forge"))    return QStringLiteral("Forge");
    if (loader == QStringLiteral("fabric"))   return QStringLiteral("Fabric");
    if (loader == QStringLiteral("neoforge")) return QStringLiteral("NeoForge");
    return loader;
}

bool CreateServerController::loaderCompatible(const QString &loader, const QString &version) const
{
    if (loader == QStringLiteral("fabric"))
        return true; // Fabric 支持几乎所有 MC 版本
    const QStringList p = version.split(QLatin1Char('.'));
    const int maj = p.value(0).toInt();
    const int min = p.value(1).toInt();
    const int pat = p.value(2).toInt();
    if (loader == QStringLiteral("forge")) {
        // Forge 自 1.1 起提供（1.0 无 Forge）
        if (maj <= 0) return false;
        if (maj == 1 && min < 1) return false;
        return true;
    }
    if (loader == QStringLiteral("neoforge")) {
        // NeoForge 自 1.20.1 从 Forge 分出
        if (maj < 1) return false;
        if (maj == 1 && min < 20) return false;
        if (maj == 1 && min == 20) return pat >= 1; // 1.20.1+
        return true; // 1.21+
    }
    return false;
}

void CreateServerController::setSelectedLoaders(const QList<QString> &v)
{
    if (m_selectedLoaders != v) {
        m_selectedLoaders = v;
        emit selectedLoadersChanged();
        regenerateNameIfAuto();
    }
}

QString CreateServerController::typeKey() const
{
    // UI 传入的是显示标签（如“模组服”），内部逻辑统一用 key：mod / paper / vanilla
    QString t = m_currentType.toLower();
    if (t == QStringLiteral("模组服"))
        return QStringLiteral("mod");
    return t;
}

void CreateServerController::setStatus(const QString &s)
{
    if (m_status != s) { m_status = s; emit statusTextChanged(); }
}
void CreateServerController::setBusy(bool b)
{
    if (m_busy != b) { m_busy = b; emit busyChanged(); }
}
void CreateServerController::setDone(bool d)
{
    if (m_done != d) { m_done = d; emit doneChanged(); }
}
void CreateServerController::setProgress(qreal p)
{
    if (m_progress != p) { m_progress = p; emit progressChanged(); }
}

// 分阶段进度：把整体进度按权重分配到各阶段（准备 Java / 每加载器 / 打包），
// 避免进度条直接跳到 100%。stageProgress 表示当前阶段内部 0-100，progress 为总进度。
void CreateServerController::beginStages(const QVector<qreal> &weights)
{
    m_useStages = true;
    m_stageWeights = weights;
    m_totalWeight = 0;
    for (qreal w : weights) m_totalWeight += w;
    m_stageIdx = 0;
    m_stageProg = 0;
    setProgress(0);
    emit stageProgressChanged();
    emit stageTextChanged();
}

void CreateServerController::setStage(int idx)
{
    if (!m_useStages) return;
    if (idx < 0) idx = 0;
    if (idx > m_stageWeights.size()) idx = m_stageWeights.size();
    m_stageIdx = idx;
    m_stageProg = 0;
    qreal before = 0;
    for (int i = 0; i < idx; ++i) before += m_stageWeights.at(i);
    const qreal overall = m_totalWeight > 0 ? before / m_totalWeight * 100.0 : 0;
    setProgress(overall);
    emit stageProgressChanged();
}

void CreateServerController::setStageProgress(qreal p)
{
    if (!m_useStages) return;
    m_stageProg = qBound(0.0, p, 100.0);
    qreal before = 0;
    for (int i = 0; i < m_stageIdx && i < m_stageWeights.size(); ++i) before += m_stageWeights.at(i);
    const qreal w = (m_stageIdx >= 0 && m_stageIdx < m_stageWeights.size()) ? m_stageWeights.at(m_stageIdx) : 0;
    const qreal overall = m_totalWeight > 0
        ? (before + w * m_stageProg / 100.0) / m_totalWeight * 100.0 : 0;
    setProgress(qBound(0.0, overall, 100.0));
    emit stageProgressChanged();
}

void CreateServerController::setStageText(const QString &t)
{
    if (m_stageText != t) { m_stageText = t; emit stageTextChanged(); }
}

void CreateServerController::setCurrentType(const QString &t)
{
    if (m_currentType != t) {
        m_currentType = t;
        emit currentTypeChanged();
    }
    regenerateNameIfAuto();
}

void CreateServerController::setCurrentVersion(const QString &v)
{
    if (m_currentVersion != v) {
        m_currentVersion = v;
        emit currentVersionChanged();
    }
    regenerateNameIfAuto();
}


// 更新保存目录（基于 base + 用户输入名称），不改动 m_name 显示名。
// 即使 m_userSetDir=true（用户点过“浏览”改父目录），也始终用当前父目录 + 新文件夹名刷新，
// 让用户改名时路径里的 "_后缀" 也能即时跟进。
void CreateServerController::refreshSaveDir()
{
    // 父目录：优先用当前 saveDir 的父（保留用户选择过的位置），否则用默认
    QString parentDir;
    if (!m_saveDir.isEmpty()) {
        const int sep = std::max(m_saveDir.lastIndexOf(QChar('/')),
                                   m_saveDir.lastIndexOf(QChar('\\')));
        if (sep > 0) parentDir = m_saveDir.left(sep);
    }
    if (parentDir.isEmpty()) parentDir = m_dm->defaultServerDir();
    // 剥除残留的 file:// 前缀（兼容老 build 的路径）
    if (parentDir.startsWith(QStringLiteral("file:///")))
        parentDir = parentDir.mid(8);
    else if (parentDir.startsWith(QStringLiteral("file://")))
        parentDir = parentDir.mid(7);

    // 文件夹名：若当前用户名称与默认自动名不同，追加 "_用户输入"；否则直接用默认名
    const QString defName = defaultName();
    const QString nameKey = m_name.trimmed();
    const bool useDefault = nameKey.isEmpty() || nameKey == defName;
    // 用 m_name 先赋值默认名（以便 QML 显示），然后再考虑后缀
    // 如果用户没改过名而字段为空，先把默认名填上。
    if (m_name.isEmpty()) {
        // 稍后 setName 会补发 nameChanged
    }
    QString folderName = useDefault ? defName
                                    : (defName + QStringLiteral("_") + nameKey);
    const QString safe = folderName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
                                              QStringLiteral("_"));
    const QString dir = QDir::cleanPath(QDir::fromNativeSeparators(parentDir)
                                        + QStringLiteral("/")
                                        + safe);
    if (m_saveDir != dir) {
        m_saveDir = dir;
        emit saveDirChanged();
    }
}

void CreateServerController::setName(const QString &v)
{
    // 名称字段完全由用户控制：直接 m_name = 用户输入，不加任何自动前缀。
    // 保存目录则使用 base_userInput 的形式（base 自动生成，userInput = 用户名称）。
    const QString cleaned = v.trimmed();
    if (m_name == cleaned)
        return;
    m_name = cleaned;
    m_userSetName = !cleaned.isEmpty();
    refreshSaveDir();
    // refreshSaveDir 已发 saveDirChanged；nameChanged 最后发，
    // 保证 QML 的 onNameChanged 能读到最新的 saveDir
    emit nameChanged();
}

QString CreateServerController::defaultName() const
{
    const QString t = typeKey();
    const QString label = (t == QStringLiteral("mod")) ? QStringLiteral("Mod")
                              : (t == QStringLiteral("vanilla")) ? QStringLiteral("Vanilla")
                              : QStringLiteral("Paper");
    if (m_currentVersion.isEmpty())
        return label;
    QString n = label + QStringLiteral("-") + m_currentVersion;
    if (t == QStringLiteral("mod")) {
        for (const QString &loader : modLoaders()) {
            if (m_selectedLoaders.contains(loader))
                n += QStringLiteral("-") + loaderLabel(loader);
        }
    }
    return n;
}

void CreateServerController::regenerateNameIfAuto()
{
    const QString oldDefault = m_baseName;
    const QString newDefault = defaultName();
    // 用户未自定义时（m_name 为空或等于旧默认名），自动同步为新默认名
    // 解决 reset 时版本未加载 → 之后版本到了 m_name 不再更新的问题
    if (m_name.isEmpty() || m_name == oldDefault) {
        m_name = newDefault;
        m_baseName = newDefault;
        refreshSaveDir();
        emit nameChanged();
        return;
    }
    m_baseName = newDefault;
    refreshSaveDir();
}

void CreateServerController::setSaveDir(const QString &d)
{
    m_userSetDir = true;
    // 规范化：Windows 上目录可能以反斜杠/ file:// 前缀/多余斜杠传入，统一成正斜杠绝对路径，
    // 避免盘符前叠加出多余的 “\”。
    QString norm = d.trimmed();
    norm = QDir::fromNativeSeparators(norm);
    if (norm.startsWith(QStringLiteral("file:///")))
        norm = norm.mid(8);
    else if (norm.startsWith(QStringLiteral("file://")))
        norm = norm.mid(7);
    norm = QDir::cleanPath(norm);
    if (m_saveDir != norm) {
        m_saveDir = norm;
        emit saveDirChanged();
    }
}

void CreateServerController::setEulaAccepted(bool b)
{
    if (m_eulaAccepted != b) {
        m_eulaAccepted = b;
        emit eulaAcceptedChanged();
    }
}

void CreateServerController::reset()
{
    m_currentType = QStringLiteral("Paper");
    m_currentVersion.clear();
    m_userSetName = false;
    m_userSetDir = false;
    m_name.clear();
    m_saveDir.clear();
    m_baseName.clear();
    m_eulaAccepted = false;
    m_busy = false;
    m_done = false;
    m_progress = 0;
    m_taskId.clear();

    // 清理分阶段进度状态
    m_useStages = false;
    m_stageWeights.clear();
    m_totalWeight = 0;
    m_stageIdx = 0;
    m_stageProg = 0;
    if (m_installTimer) m_installTimer->stop();
    m_installAnim = 0;
    if (!m_stageText.isEmpty()) { m_stageText.clear(); emit stageTextChanged(); }
    emit stageProgressChanged();
    m_status.clear();

    // 清理模组服相关状态
    if (!m_selectedLoaders.isEmpty()) { m_selectedLoaders.clear(); emit selectedLoadersChanged(); }
    m_loaderQueue.clear();
    m_activeLoader.clear();
    m_loaderByTask.clear();
    m_tempJava.clear();
    m_loaderTotal = 0;
    m_loaderDone = 0;

    emit currentTypeChanged();
    emit currentVersionChanged();
    emit eulaAcceptedChanged();
    emit busyChanged();
    emit doneChanged();
    emit progressChanged();
    emit statusTextChanged();

    regenerateNameIfAuto();
    loadVersions();
}

void CreateServerController::loadVersions()
{
    const QString t = typeKey();
    m_versions.clear();
    emit versionsChanged();

    if (t == QStringLiteral("paper"))        fetchPaper();
    else if (t == QStringLiteral("vanilla"))  fetchVanilla();
    else if (t == QStringLiteral("mod"))      fetchModVersions();
    else                                       fetchPaper();
}

void CreateServerController::fetchJson(const QString &url,
                                       std::function<void(const QJsonDocument &)> onOk,
                                       std::function<void(const QString &)> onErr)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MinecraftServerManager/1.0 (https://github.com/MinecraftServerManager; contact: msm@example.com)"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(20000);   // 解析阶段：避免慢/不稳网络下长时间无响应卡死
    QNetworkReply *reply = m_nam->get(req);
    qDebug() << "[MSM] fetchJson GET" << url;
    // 硬兜底：Qt 的 TransferTimeout 仅覆盖“已连接但无数据”阶段，不覆盖“握手阶段被墙”的卡死
    // （被墙域名 SYN 黑洞会令 finished 永不触发，界面永远停在“解析下载地址”）。
    // 关键：兜底计时器到时【直接调用 onErr】，不依赖 abort() 后 finished 一定触发——
    // 否则在 Qt6+MinGW 下 TLS 握手阶段 abort() 有可能不发出 finished，造成永久卡死。
    // 用 std::shared_ptr<bool> 作“只回调一次”的闸门，finished 与计时器谁先到都安全。
    auto done = std::make_shared<bool>(false);
    auto finish = [done, onOk, onErr](bool ok, const QByteArray &data, const QString &err) {
        if (*done) return;
        *done = true;
        if (ok) {
            QJsonParseError pe;
            const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
            if (doc.isNull()) { onErr(QStringLiteral("JSON 解析失败: ") + pe.errorString()); return; }
            onOk(doc);
        } else {
            onErr(err);
        }
    };
    QTimer *guard = new QTimer(reply);
    guard->setSingleShot(true);
    guard->setInterval(20000);
    QObject::connect(guard, &QTimer::timeout, reply, [reply, done, finish]() {
        if (*done) return;
        qDebug() << "[MSM] fetch TIMEOUT" << reply->url().toString();
        if (reply->isRunning()) reply->abort();
        finish(false, QByteArray(), QStringLiteral("请求超时（20s 无响应）"));
    });
    guard->start();
    connect(reply, &QNetworkReply::finished, reply, [reply, finish]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString err = reply->errorString();
        qDebug() << "[MSM] fetch finished ok=" << ok << "err=" << err.left(120)
                 << "bytes=" << data.size();
        reply->deleteLater();
        finish(ok, data, err);
    });
}

void CreateServerController::fetchText(const QString &url,
                                       std::function<void(const QString &)> onOk,
                                       std::function<void(const QString &)> onErr)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MinecraftServerManager/1.0 (https://github.com/MinecraftServerManager; contact: msm@example.com)"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(20000);   // 解析阶段：避免慢/不稳网络下长时间无响应卡死
    QNetworkReply *reply = m_nam->get(req);
    qDebug() << "[MSM] fetchText GET" << url;
    // 同上：硬兜底，计时器到时直接回调 onErr，不依赖 abort()->finished。
    auto done = std::make_shared<bool>(false);
    auto finish = [done, onOk, onErr](bool ok, const QByteArray &data, const QString &err) {
        if (*done) return;
        *done = true;
        if (ok)
            onOk(QString::fromUtf8(data));
        else
            onErr(err);
    };
    QTimer *guard = new QTimer(reply);
    guard->setSingleShot(true);
    guard->setInterval(20000);
    QObject::connect(guard, &QTimer::timeout, reply, [reply, done, finish]() {
        if (*done) return;
        qDebug() << "[MSM] fetch TIMEOUT" << reply->url().toString();
        if (reply->isRunning()) reply->abort();
        finish(false, QByteArray(), QStringLiteral("请求超时（20s 无响应）"));
    });
    guard->start();
    connect(reply, &QNetworkReply::finished, reply, [reply, finish]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString err = reply->errorString();
        qDebug() << "[MSM] fetch finished ok=" << ok << "err=" << err.left(120)
                 << "bytes=" << data.size();
        reply->deleteLater();
        finish(ok, data, err);
    });
}

void CreateServerController::fetchPaper()
{
    // PaperMC 旧 v2 API（api.papermc.io）已停用，迁移到 Fill v3（fill.papermc.io）。
    fetchJson(QStringLiteral("https://fill.papermc.io/v3/projects/paper/versions"),
        [this](const QJsonDocument &d) {
            QStringList vers;
            const QJsonArray vs = d.object().value(QStringLiteral("versions")).toArray();
            for (const auto &e : vs) {
                const QString id = e.toObject().value(QStringLiteral("version"))
                                        .toObject().value(QStringLiteral("id")).toString();
                if (!id.isEmpty()) vers << id;
            }
            // 语义版本降序：最新版本在前（1.21.1 > 1.20.4）
            std::sort(vers.begin(), vers.end(), [](const QString &a, const QString &b) {
                return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
            });
            m_versions = vers.mid(0, 15);
            emit versionsChanged();
            if (!m_versions.isEmpty()) setCurrentVersion(m_versions.first());
        },
        [this](const QString &e) { setStatus(QStringLiteral("获取版本列表失败（网络异常）：") + e); });
}

// 官方启动器版本清单（Mojang launchermeta）：返回按发布时间倒序的最近若干个 release 版本。
// Vanilla 与模组服共用同一套官方 MC 版本列表（各加载器均面向官方版本）。
void CreateServerController::fetchVanilla()
{
    // 注意：清单含全部历史版本，体积较大；仅取 release 并取最近 15 个。
    fetchJson(QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest_v2.json"),
        [this](const QJsonDocument &d) {
            QStringList vers;
            const QJsonArray arr = d.object().value(QStringLiteral("versions")).toArray();
            for (const auto &e : arr) {
                const QJsonObject o = e.toObject();
                if (o.value(QStringLiteral("type")).toString() == QStringLiteral("release"))
                    vers << o.value(QStringLiteral("id")).toString();
            }
            // 语义版本降序：最新版本在前，取前 15 个即最近发布
            std::sort(vers.begin(), vers.end(), [](const QString &a, const QString &b) {
                return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
            });
            m_versions = vers.mid(0, 15);
            emit versionsChanged();
            if (!m_versions.isEmpty()) setCurrentVersion(m_versions.first());
        },
        [this](const QString &e) { setStatus(QStringLiteral("获取版本列表失败（网络异常）：") + e); });
}

// 模组服版本列表：与 Vanilla 共用官方 release 列表（各加载器均面向官方版本）
void CreateServerController::fetchModVersions()
{
    fetchVanilla();
}

// 解析某加载器（forge/fabric/neoforge）对应 MC 版本的 installer 下载地址
void CreateServerController::resolveLoaderUrl(const QString &loader, const QString &version,
                                              std::function<void(const QString &, const QString &)> cb)
{
    qDebug() << "[MSM] resolveLoaderUrl loader=" << loader << "version=" << version;
    if (loader == QStringLiteral("fabric")) {
        // Fabric installer 固定版本，运行安装器时再指定 -mcversion
        const QString primary = QStringLiteral("https://maven.fabricmc.net/net/fabricmc/fabric-installer/1.1.1/fabric-installer-1.1.1.jar");
        const QString mirror = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/fabricmc/fabric-installer/1.1.1/fabric-installer-1.1.1.jar");
        cb(primary, mirror);
    } else if (loader == QStringLiteral("forge")) {
        // Forge 安装器路径需构建号。
        // 主源：官方 promotions_slim.json 只托管在 files.minecraftforge.net（maven.minecraftforge.net 无此文件，会 404）；
        //       installer 从 maven.minecraftforge.net 下载。
        // 镜像：官方源被墙时降级到 BMCLAPI 专用 Forge API（/forge/minecraft/{ver} 返回该版本全部构建，数据保持最新，
        //       而 BMCLAPI 的 promotions_slim.json 镜像是旧数据，不能用）；installer 走 BMCLAPI maven 镜像。
        const QString mcver = version;
        // 官方可达但该版本无 Forge 时也尝试镜像（镜像收录可能更全）
        const std::function<void()> tryForgeMirror = [this, mcver, cb]() {
            setStatus(QStringLiteral("Forge 官方源不可达，正在改用 BMCLAPI 镜像…"));
            fetchJson(QStringLiteral("https://bmclapi2.bangbang93.com/forge/minecraft/") + mcver,
                [mcver, cb](const QJsonDocument &d) {
                    // 数组每项含 version（如 "52.1.15"），取版本号最高者
                    QString best;
                    for (const auto &e : d.array()) {
                        const QString v = e.toObject().value(QStringLiteral("version")).toString();
                        if (v.isEmpty()) continue;
                        if (best.isEmpty() || QVersionNumber::fromString(v) > QVersionNumber::fromString(best))
                            best = v;
                    }
                    if (best.isEmpty()) { cb(QString(), QString()); return; }
                    cb(QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/")
                       + mcver + QLatin1Char('-') + best + QStringLiteral("/forge-")
                       + mcver + QLatin1Char('-') + best + QStringLiteral("-installer.jar"), QString());
                },
                [this, cb](const QString &e) { setStatus(QStringLiteral("获取下载地址失败：") + e); cb(QString(), QString()); });
        };
        fetchJson(QStringLiteral("https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json"),
            [this, mcver, cb, tryForgeMirror](const QJsonDocument &d) {
                const QJsonObject promos = d.object().value(QStringLiteral("promos")).toObject();
                QString build;
                const QString rec = mcver + QStringLiteral("-recommended");
                const QString lat = mcver + QStringLiteral("-latest");
                if (promos.contains(rec)) build = promos.value(rec).toString();
                else if (promos.contains(lat)) build = promos.value(lat).toString();
                else for (auto it = promos.begin(); it != promos.end(); ++it)
                    if (it.key().section(QLatin1Char('-'), 0, 0) == mcver) { build = it.value().toString(); break; }
                if (build.isEmpty()) { tryForgeMirror(); return; }
                const QString primary = QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/")
                   + mcver + QLatin1Char('-') + build + QStringLiteral("/forge-")
                   + mcver + QLatin1Char('-') + build + QStringLiteral("-installer.jar");
                const QString mirror = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/")
                   + mcver + QLatin1Char('-') + build + QStringLiteral("/forge-")
                   + mcver + QLatin1Char('-') + build + QStringLiteral("-installer.jar");
                cb(primary, mirror);
            },
            [tryForgeMirror](const QString &) { tryForgeMirror(); });
    } else if (loader == QStringLiteral("neoforge")) {
        // NeoForge 版本号形如 "21.1.242"（主.次.修订），其中"主.次"对应 MC 版本去掉前导 "1."
        // 例如 MC 1.21.1 -> 分支 "21.1"。
        // 优先用 BMCLAPI（国内可达，且直接返回精确的 installerPath）；官方 maven-metadata 作为兜底。
        const QStringList vp = version.split(QLatin1Char('.'));
        const QString branch = (vp.size() >= 3 && vp.at(0) == QStringLiteral("1"))
                                   ? vp.at(1) + QLatin1Char('.') + vp.at(2)
                                   : version;
        const QString mcver = version;

        // 兜底：官方 maven-metadata.xml，按分支筛选；稳定版优先，否则保留 -beta/-alpha 后缀（否则 jar 404）
        const std::function<void()> resolveByMetadata = [this, branch, cb]() {
            fetchText(QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml"),
                [this, branch, cb](const QString &xml) {
                    const QRegularExpression re(QStringLiteral("<version>([^<]+)</version>"));
                    QRegularExpressionMatchIterator it = re.globalMatch(xml);
                    QString bestBase, bestFull, stableBase, stableFull;
                    while (it.hasNext()) {
                        const QString full = it.next().captured(1);           // 可能含 -beta/-alpha
                        const QString base = full.section(QLatin1Char('-'), 0, 0);
                        const QString br = base.section(QLatin1Char('.'), 0, 1);
                        if (br != branch) continue;
                        const QVersionNumber vb = QVersionNumber::fromString(base);
                        if (!full.contains(QLatin1Char('-'))) {
                            if (stableBase.isEmpty() || vb > QVersionNumber::fromString(stableBase)) {
                                stableBase = base; stableFull = full;
                            }
                        }
                        if (bestBase.isEmpty() || vb > QVersionNumber::fromString(bestBase)) {
                            bestBase = base; bestFull = full;
                        }
                    }
                    const QString chosen = stableBase.isEmpty() ? bestFull : stableFull;
                    if (chosen.isEmpty()) { cb(QString(), QString()); return; }
                    const QString primary = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/") + chosen
                       + QStringLiteral("/neoforge-") + chosen + QStringLiteral("-installer.jar");
                    const QString mirror = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/neoforged/neoforge/") + chosen
                       + QStringLiteral("/neoforge-") + chosen + QStringLiteral("-installer.jar");
                    cb(primary, mirror);
                },
                [this, cb](const QString &e) { setStatus(QStringLiteral("获取 NeoForge 下载地址失败：") + e); cb(QString(), QString()); });
        };

        fetchJson(QStringLiteral("https://bmclapi2.bangbang93.com/neoforge/list/") + mcver,
            [this, mcver, cb, resolveByMetadata](const QJsonDocument &d) {
                // 取 version 最高的一项，直接用其 installerPath 拼接 BMCLAPI 下载地址；兜底回官方 installer
                QString best, bestPath;
                for (const auto &e : d.array()) {
                    const QJsonObject o = e.toObject();
                    const QString v = o.value(QStringLiteral("version")).toString();
                    if (v.isEmpty()) continue;
                    if (best.isEmpty() || QVersionNumber::fromString(v) > QVersionNumber::fromString(best)) {
                        best = v;
                        bestPath = o.value(QStringLiteral("installerPath")).toString();
                    }
                }
                if (bestPath.isEmpty()) { resolveByMetadata(); return; }
                const QString primary = QStringLiteral("https://bmclapi2.bangbang93.com") + bestPath;
                const QString mirror = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/") + best
                   + QStringLiteral("/neoforge-") + best + QStringLiteral("-installer.jar");
                cb(primary, mirror);
            },
            [resolveByMetadata](const QString &) { resolveByMetadata(); });
    } else {
        cb(QString(), QString());
    }
}

void CreateServerController::resolveUrl(const QString &type, const QString &version,
                                        std::function<void(const QString &)> cb)
{
    if (type == QStringLiteral("paper")) {
        // PaperMC Fill v3：builds 为数组（按 id 降序），下载地址内嵌在 downloads["server:default"].url
        fetchJson(QStringLiteral("https://fill.papermc.io/v3/projects/paper/versions/") + version + QStringLiteral("/builds"),
            [this, version, cb](const QJsonDocument &bd) {
                const QJsonArray builds = bd.array();
                if (builds.isEmpty()) { cb(QString()); return; }
                const QJsonObject latest = builds.first().toObject();
                const QJsonObject dl = latest.value(QStringLiteral("downloads"))
                                            .toObject().value(QStringLiteral("server:default")).toObject();
                const QString url = dl.value(QStringLiteral("url")).toString();
                cb(url);
            },
            [this, cb](const QString &e) { setStatus(QStringLiteral("获取下载地址失败：") + e); cb(QString()); });
    } else if (type == QStringLiteral("vanilla")) {
        // 官方原版：先取版本清单找到该版本的版本 JSON，再取 downloads.server.url。
        // 策略：每一步都"主源→镜像→失败则用官方 piston-*.mojang.com 兜底"——
        // BMCLAPI 时段性 502/503/超时等问题时回退到官方源。
        const QString mirrorHost = QStringLiteral("bmclapi2.bangbang93.com");
        const std::function<void(const QJsonDocument &)> resolveVanilla =
            [this, version, cb, mirrorHost](const QJsonDocument &md) {
                QString verUrl;
                for (const auto &e : md.object().value(QStringLiteral("versions")).toArray()) {
                    const QJsonObject o = e.toObject();
                    if (o.value(QStringLiteral("id")).toString() == version) {
                        verUrl = o.value(QStringLiteral("url")).toString();
                        break;
                    }
                }
                if (verUrl.isEmpty()) { cb(QString()); return; }
                // 保留官方源 URL 作为兜底
                const QString officialVerUrl = verUrl;
                // 优先用 BMCLAPI 镜像（已替换过域名）
                verUrl.replace(QStringLiteral("piston-meta.mojang.com"), mirrorHost);
                // 取出最终 server.jar URL 并返回
                auto extract = [this, cb, mirrorHost](const QJsonDocument &vd) {
                    QString url = vd.object().value(QStringLiteral("downloads")).toObject()
                                      .value(QStringLiteral("server")).toObject()
                                      .value(QStringLiteral("url")).toString();
                    QString fbUrl = url;
                    fbUrl.replace(QStringLiteral("piston-data.mojang.com"), mirrorHost);
                    m_serverFallbackUrl = url;  // 官方源作为下载兜底
                    cb(fbUrl);                   // 先走 BMCLAPI
                };
                // 先 BMCLAPI 镜像；失败回退到官方 piston-meta.mojang.com
                fetchJson(verUrl,
                    [extract](const QJsonDocument &vd) { extract(vd); },
                    [this, cb, officialVerUrl, extract](const QString &e) {
                        setStatus(QStringLiteral("镜像版本信息不可达，改用官方源…"));
                        fetchJson(officialVerUrl,
                            [extract](const QJsonDocument &vd) { extract(vd); },
                            [this, cb, e](const QString &e2) {
                                setStatus(QStringLiteral("获取下载地址失败：") + e + QStringLiteral(" / ") + e2);
                                cb(QString());
                            });
                    });
            };
        const QString primary = QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest_v2.json");
        const QString mirror  = QStringLiteral("https://bmclapi2.bangbang93.com/mc/game/version_manifest_v2.json");
        fetchJson(primary,
            [resolveVanilla](const QJsonDocument &md) { resolveVanilla(md); },
            [=](const QString &) {
                setStatus(QStringLiteral("官方源不可达，正在改用 BMCLAPI 镜像…"));
                fetchJson(mirror,
                    [resolveVanilla](const QJsonDocument &md) { resolveVanilla(md); },
                    [this, cb](const QString &e) { setStatus(QStringLiteral("获取下载地址失败：") + e); cb(QString()); });
            });
    } else if (type == QStringLiteral("fabric")) {
        // Fabric 无直接可运行的服务端核心，下载官方 installer（version 形如 loader@installer）
        const QStringList parts = version.split(QStringLiteral("@"));
        const QString installerVer = parts.value(1);
        if (installerVer.isEmpty()) { cb(QString()); return; }
        cb(QStringLiteral("https://maven.fabricmc.net/net/fabricmc/fabric-installer/") + installerVer +
           QStringLiteral("/fabric-installer-") + installerVer + QStringLiteral(".jar"));
    } else if (type == QStringLiteral("forge")) {
        // Forge installer 路径必须包含构建号，从 promotions 中取推荐/最新构建
        fetchJson(QStringLiteral("https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json"),
            [this, version, cb](const QJsonDocument &d) {
                const QJsonObject promos = d.object().value(QStringLiteral("promos")).toObject();
                QString build;
                const QString rec = version + QStringLiteral("-recommended");
                const QString lat = version + QStringLiteral("-latest");
                if (promos.contains(rec)) build = promos.value(rec).toString();
                else if (promos.contains(lat)) build = promos.value(lat).toString();
                else for (auto it = promos.begin(); it != promos.end(); ++it)
                    if (it.key().section(QLatin1Char('-'), 0, 0) == version) { build = it.value().toString(); break; }
                if (build.isEmpty()) { cb(QString()); return; }
                cb(QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/") + version + QStringLiteral("-") + build +
                   QStringLiteral("/forge-") + version + QStringLiteral("-") + build + QStringLiteral("-installer.jar"));
            },
            [this, cb](const QString &e) { setStatus(QStringLiteral("获取下载地址失败：") + e); cb(QString()); });
    } else {
        cb(QString());
    }
}

void CreateServerController::create()
{
    if (m_busy || m_done)
        return;
    if (m_name.trimmed().isEmpty()) { setStatus(QStringLiteral("请填写服务器名称")); return; }
    if (!m_eulaAccepted) { setStatus(QStringLiteral("请先阅读并勾选同意 EULA")); return; }
    if (m_currentVersion.isEmpty()) { setStatus(QStringLiteral("请选择游戏版本")); return; }

    if (!m_dm->ensureDir(m_saveDir)) {
        setStatus(QStringLiteral("无法创建目录：") + m_saveDir);
        return;
    }

    // 模组服：逐个加载器安装（forge/fabric/neoforge），使用临时 Java，完成后清理
    if (typeKey() == QStringLiteral("mod")) {
        if (m_selectedLoaders.isEmpty()) {
            setStatus(QStringLiteral("请至少勾选一个加载器（Forge / Fabric / NeoForge）"));
            return;
        }
        startModCreate();
        return;
    }

    setBusy(true);
    setDone(false);
    setProgress(0);
    m_useStages = false;   // 非模组服：不使用分阶段进度
    setStatus(QStringLiteral("正在解析下载地址…"));

    resolveUrl(typeKey(), m_currentVersion, [this](const QString &url) {
        if (url.isEmpty()) {
            setBusy(false);
            return;   // 具体错误（TLS/超时/连接被重置）已由 resolveUrl 内部 setStatus 显示
        }
        setStatus(QStringLiteral("正在下载服务端…"));
        const QString t = typeKey();
        QString file = QStringLiteral("server.jar");
        if (t == QStringLiteral("fabric")) file = QStringLiteral("fabric-installer.jar");
        else if (t == QStringLiteral("forge")) file = QStringLiteral("forge-installer.jar");
        const QString id = m_dm->download(url, m_saveDir, file);
        if (id.isEmpty()) {
            setBusy(false);
            setStatus(QStringLiteral("无法开始下载"));
            return;
        }
        m_taskId = id;
    });
}

void CreateServerController::setPackageOnly(bool b)
{
    m_packageOnly = b;
}

void CreateServerController::importZip(const QString &zipPath)
{
    if (m_busy || m_done)
        return;
    if (zipPath.trimmed().isEmpty()) {
        setStatus(QStringLiteral("请先选择要导入的压缩包"));
        return;
    }
    if (m_name.trimmed().isEmpty()) {
        setStatus(QStringLiteral("请填写服务器名称"));
        return;
    }
    if (!m_eulaAccepted) {
        setStatus(QStringLiteral("请先阅读并勾选同意 EULA"));
        return;
    }
    if (!QFile::exists(zipPath)) {
        setStatus(QStringLiteral("压缩包不存在：") + zipPath);
        return;
    }
    if (!m_dm->ensureDir(m_saveDir)) {
        setStatus(QStringLiteral("无法创建服务器目录：") + m_saveDir);
        return;
    }
    // 导入压缩包不需要运行安装器，直接解压 + 探测 + 加入列表
    importZipCore(zipPath, m_name, m_saveDir, QString());
}

void CreateServerController::importZipCore(const QString &zipPath, const QString &name,
                                           const QString &saveDir, const QString &java)
{
    setBusy(true);
    setDone(false);
    setProgress(10);
    m_useStages = false;   // 导入压缩包：不使用分阶段进度
    setStatus(QStringLiteral("正在解压压缩包…"));

    QProcess *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p, name, saveDir, java, zipPath](int exitCode, QProcess::ExitStatus) {
        p->deleteLater();
        if (exitCode != 0 || !QDir(saveDir).exists()) {
            setBusy(false);
            setStatus(QStringLiteral("解压失败（退出码 ") + QString::number(exitCode)
                      + QStringLiteral("）。请确认压缩包为有效的服务器目录打包：") + zipPath);
            return;
        }
        setProgress(70);

        // 探测类型：优先读取打包时写入的 .msm/loaders.txt，否则根据 jar 名判断
        QString version, type;
        QFile lf(saveDir + QStringLiteral("/.msm/loaders.txt"));
        if (lf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!lf.atEnd()) {
                if (!QString::fromUtf8(lf.readLine()).trimmed().isEmpty()) {
                    type = QStringLiteral("mod");
                    break;
                }
            }
            lf.close();
        }
        if (type.isEmpty()) {
            const QStringList jars = QDir(saveDir).entryList(QStringList() << QStringLiteral("*.jar"),
                                                             QDir::Files);
            for (const QString &j : jars) {
                if (j.startsWith(QStringLiteral("forge-"), Qt::CaseInsensitive)
                        || j.startsWith(QStringLiteral("neoforge-"), Qt::CaseInsensitive)
                        || j.contains(QStringLiteral("fabric-server"), Qt::CaseInsensitive)) {
                    type = QStringLiteral("mod");
                    break;
                }
            }
        }

        // 写入 EULA（导入即视为已同意）+ Java 记录
        m_dm->writeTextFile(saveDir + QStringLiteral("/eula.txt"),
                            QStringLiteral("eula=true\n# 由 MSM 导入压缩包时自动生成（已同意 Minecraft EULA）\n"));
        if (!java.isEmpty())
            m_dm->writeTextFile(saveDir + QStringLiteral("/.msm/java.txt"), java + QStringLiteral("\n"));

        if (m_sm)
            m_sm->addServer(name, version, type, saveDir);

        setBusy(false);
        setDone(true);
        setProgress(100);
        setStatus(QStringLiteral("导入完成，已加入服务器列表：") + saveDir);
    });

    // Windows：PowerShell Expand-Archive（与打包用的 Compress-Archive 互逆）
    // 通过 $args 传参，避免把路径拼进 PowerShell 命令字符串（路径含单引号会导致命令注入）
    const QString script = QStringLiteral("Expand-Archive -Force -LiteralPath $args[0] -DestinationPath $args[1]");
    p->start(QStringLiteral("powershell"),
             QStringList() << QStringLiteral("-NoProfile") << QStringLiteral("-Command") << script
                           << zipPath << saveDir);
    if (!p->waitForStarted(10000)) {
        p->deleteLater();
        setBusy(false);
        setStatus(QStringLiteral("无法启动解压进程（PowerShell 不可用）"));
    }
}

void CreateServerController::onDownloadProgress(const QString &id, qreal percent, qint64, qint64)
{
    if (id == m_taskId) {
        // 模组服下载阶段：下载占当前加载器阶段的 70%，运行安装器占剩余 30%，
        // 避免进度条在下载完成后直接跳到 100%。非模组服则直接映射总进度。
        if (m_useStages && !m_activeLoader.isEmpty()) {
            setStageText(QStringLiteral("正在下载 ")
                          + loaderLabel(m_activeLoader)
                          + QStringLiteral(" 安装器… ")
                          + QString::number(int(percent)) + QStringLiteral("%"));
            setStageProgress(percent * 0.7);
        } else {
            setProgress(percent);
        }
        if (!m_activeLoader.isEmpty()) {
            setStatus(QStringLiteral("正在下载 ")
                      + loaderLabel(m_activeLoader)
                      + QStringLiteral(" 安装器… ")
                      + QString::number(int(percent)) + QStringLiteral("%"));
        }
    }
}

void CreateServerController::onDownloadFinished(const QString &id, const QString &)
{
    // 模组服：某个加载器安装器下载完成 -> 运行该加载器安装器
    if (m_loaderByTask.contains(id)) {
        const QString loader = m_loaderByTask.take(id);
        runModInstaller(loader);
        return;
    }
    if (id != m_taskId)
        return;
    const QString t = typeKey();
    // Paper / Vanilla：服务端 jar 已是可运行核心，确保有合适的 Java（缺失时按版本自动安装），
    // 并写入 java.txt 便于后续启动。
    Q_UNUSED(t)
    resolveJava([this](const QString &java) {
        if (java.isEmpty())
            return; // resolveJava 已设置错误状态并解除 busy
        m_resolvedJava = java;
        finalizeCreate();
    });
}

void CreateServerController::finalizeCreate()
{
    // 写入 EULA（已同意），并把服务器加入管理列表
    m_dm->writeTextFile(m_saveDir + QStringLiteral("/eula.txt"),
                        QStringLiteral("eula=true\n# 由 MSM 创建服务器时自动生成（已同意 Minecraft EULA）\n"));
    // 记录本服务器使用的 Java 路径，便于后续启动时使用对应版本
    if (!m_resolvedJava.isEmpty())
        m_dm->writeTextFile(m_saveDir + QStringLiteral("/.msm/java.txt"), m_resolvedJava + QStringLiteral("\n"));
    if (m_sm)
        m_sm->addServer(m_name, m_currentVersion, typeKey(), m_saveDir);

    setBusy(false);
    setDone(true);
    setStatus(QStringLiteral("安装完成，已加入服务器列表"));
}

QString CreateServerController::findJava() const
{
    QString j = QStandardPaths::findExecutable(QStringLiteral("java"));
    if (!j.isEmpty())
        return j;
    const QString home = qEnvironmentVariable("JAVA_HOME");
    if (!home.isEmpty()) {
        j = QStandardPaths::findExecutable(QStringLiteral("java"), { home + QStringLiteral("/bin") });
        if (!j.isEmpty())
            return j;
    }
    return QString();
}

QString CreateServerController::findServerJar(const QString &dir, const QString &type) const
{
    QDir d(dir);
    const QStringList jars = d.entryList(QStringList() << QStringLiteral("*.jar"), QDir::Files);
    QStringList candidates;
    for (const QString &j : jars) {
        if (j.contains(QStringLiteral("installer"), Qt::CaseInsensitive))
            continue;
        candidates << j;
    }
    if (candidates.isEmpty())
        return QString();
    if (type == QStringLiteral("forge")) {
        for (const QString &j : candidates)
            if (j.startsWith(QStringLiteral("forge-"), Qt::CaseInsensitive))
                return j;
    } else if (type == QStringLiteral("fabric")) {
        for (const QString &j : candidates)
            if (j.contains(QStringLiteral("fabric-server"), Qt::CaseInsensitive))
                return j;
    }
    return candidates.first();
}

// ============ 模组服流程：临时 Java + 逐个加载器安装 + 完成后清理 ============

void CreateServerController::startModCreate()
{
    setBusy(true);
    setDone(false);
    setProgress(0);
    m_loaderTotal = m_selectedLoaders.size();
    // 阶段：准备 Java(1) + 每个加载器(3) + 打包(1)，按权重分配到总进度
    QVector<qreal> weights;
    weights << 1;
    for (int i = 0; i < m_loaderTotal; ++i) weights << 3;
    weights << 1;
    beginStages(weights);
    setStageText(QStringLiteral("正在准备 Java 运行环境（临时，用于运行安装器）…"));
    prepareTempJava([this](bool ok, QString java) {
        if (!ok) {
            setBusy(false);
            setStageText(QString());
            setStatus(QStringLiteral("准备临时 Java 失败，无法运行加载器安装器。"));
            return;
        }
        m_tempJava = java;
        m_loaderQueue = m_selectedLoaders;
        m_loaderDone = 0;
        setStage(1);   // 进入第一个加载器阶段
        processNextLoader();
    });
}

void CreateServerController::prepareTempJava(std::function<void(bool, QString)> cb)
{
    // 若系统/PATH/已安装已有可用 Java，直接复用，无需临时下载
    if (m_java) {
        const QString have = m_java->javaPathFor(m_currentVersion);
        if (!have.isEmpty()) { cb(true, have); return; }
        const int feature = JavaManager::requiredFeature(m_currentVersion);
        m_java->ensureTemp(feature, [cb](bool ok, QString path) { cb(ok, path); });
        return;
    }
    const QString j = findJava();
    if (j.isEmpty()) { cb(false, QString()); return; }
    cb(true, j);
}

void CreateServerController::processNextLoader()
{
    if (m_loaderQueue.isEmpty()) {
        finalizeModCreate();
        return;
    }
    const QString loader = m_loaderQueue.takeFirst();
    m_activeLoader = loader;
    setStage(1 + m_loaderDone);   // 进入当前加载器阶段（阶段 0 为准备 Java）
    setStageProgress(0);
    qDebug() << "[MSM] processNextLoader: 开始解析加载器" << loader;
    setStageText(QStringLiteral("正在解析 ") + loaderLabel(loader) + QStringLiteral(" 下载地址…"));
    setStatus(QStringLiteral("正在解析 ") + loaderLabel(loader) + QStringLiteral(" 下载地址…"));
    resolveLoaderUrl(loader, m_currentVersion, [this, loader](const QString &url, const QString &fallback) {
        qDebug() << "[MSM] resolve cb loader=" << loader
                 << "url=" << (url.isEmpty() ? QStringLiteral("<empty>") : url.left(90))
                 << "fallback=" << (fallback.isEmpty() ? QStringLiteral("<none>") : fallback.left(90));
        if (url.isEmpty()) {
            setStatus(QStringLiteral("跳过 ") + loaderLabel(loader)
                      + QStringLiteral("：当前版本不兼容或无法获取下载地址"));
            m_activeLoader.clear();
            m_loaderDone++;
            processNextLoader();
            return;
        }
        m_loaderFallbackUrl = fallback;
        startLoaderDownload(loader, url);
    });
}

void CreateServerController::startLoaderDownload(const QString &loader, const QString &url)
{
    qDebug() << "[MSM] startLoaderDownload loader=" << loader << "url=" << url.left(90);
    setStageText(QStringLiteral("正在下载 ") + loaderLabel(loader) + QStringLiteral(" 安装器…"));
    setStageProgress(0);
    setStatus(QStringLiteral("正在下载 ") + loaderLabel(loader) + QStringLiteral(" 安装器…"));
    const QString file = loader + QStringLiteral("-installer.jar");
    const QString id = m_dm->download(url, m_saveDir, file);
    if (id.isEmpty()) {
        // 下载管理器不可用：再尝试镜像兜底
        if (!m_loaderFallbackUrl.isEmpty()) {
            const QString fb = m_loaderFallbackUrl;
            m_loaderFallbackUrl.clear();
            setStatus(QStringLiteral("下载管理器不可用，正在改用镜像下载 ") + loaderLabel(loader) + QStringLiteral(" 安装器…"));
            startLoaderDownload(loader, fb);
            return;
        }
        cleanupTempJava();
        setBusy(false);
        setStatus(QStringLiteral("无法开始下载 ") + loaderLabel(loader) + QStringLiteral(" 安装器"));
        return;
    }
    m_loaderByTask[id] = loader;
    m_taskId = id;
}

void CreateServerController::runModInstaller(const QString &loader)
{
    qDebug() << "[MSM] runModInstaller loader=" << loader;
    const QString installer = loader + QStringLiteral("-installer.jar");
    const QString installerPath = m_saveDir + QStringLiteral("/") + installer;
    if (!QFile::exists(installerPath)) {
        setStatus(QStringLiteral("安装器缺失，跳过 ") + loaderLabel(loader) + QStringLiteral("：") + installerPath);
        m_loaderDone++;
        processNextLoader();
        return;
    }
    if (m_tempJava.isEmpty()) {
        cleanupTempJava();
        setBusy(false);
        setStatus(QStringLiteral("临时 Java 不可用，无法运行 ") + loaderLabel(loader) + QStringLiteral(" 安装器。"));
        return;
    }
    m_activeLoader = loader;
    setStageText(QStringLiteral("正在运行 ") + loaderLabel(loader)
                  + QStringLiteral(" 安装器生成服务端核心…（首次可能需下载 Minecraft，进度不可读）"));
    setStageProgress(75);   // 下载已完成（70%），进入安装阶段
    // 安装器运行期间其自身下载进度无法读取：用缓慢爬升的动画表示“进行中”，
    // 避免进度条在安装阶段长时间冻结。完成时由 onModInstallerFinished 停止并置 100%。
    m_installAnim = 75;
    if (!m_installTimer) {
        m_installTimer = new QTimer(this);
        connect(m_installTimer, &QTimer::timeout, this, [this]() {
            if (m_installAnim < 98) {
                m_installAnim += 1.2;
                setStageProgress(m_installAnim);
            }
        });
    }
    m_installTimer->start(350);
    startInstallerProcess(m_tempJava, loader);
}

void CreateServerController::onModInstallerFinished(const QString &loader, int exitCode, QProcess::ExitStatus)
{
    if (m_installTimer) m_installTimer->stop();
    m_installAnim = 0;
    if (exitCode != 0) {
        cleanupTempJava();
        setBusy(false);
        setStatus(QStringLiteral("安装器 ") + loaderLabel(loader) + QStringLiteral(" 执行失败（退出码 ")
                  + QString::number(exitCode) + QStringLiteral("），请检查网络后重试。\n安装器输出：\n")
                  + installerErrorTail());
        return;
    }
    // 清理安装器 jar（生成的核心保留）
    QFile::remove(m_saveDir + QStringLiteral("/") + loader + QStringLiteral("-installer.jar"));
    m_loaderDone++;
    setStageProgress(100);   // 当前加载器阶段完成
    processNextLoader();
}

void CreateServerController::finalizeModCreate()
{
    // 找到第一个成功生成核心的加载器，作为默认 server.jar（其余加载器核心也保留在目录中）
    QString serverJar;
    for (const QString &loader : m_selectedLoaders) {
        const QString jar = findLoaderJar(m_saveDir, loader);
        if (jar.isEmpty())
            continue;
        if (serverJar.isEmpty())
            serverJar = jar;
    }
    if (serverJar.isEmpty()) {
        cleanupTempJava();
        setBusy(false);
        setStatus(QStringLiteral("安装完成但未找到任何加载器生成的服务端核心，请检查目录：") + m_saveDir);
        return;
    }
    // 先确定压缩包路径（须在删除原始加载器 jar 之前取出加载器版本号）
    const QString zipPath = computeServerZipName();
    const QString dst = m_saveDir + QStringLiteral("/server.jar");
    QFile::remove(dst);
    if (!QFile::copy(m_saveDir + QStringLiteral("/") + serverJar, dst)) {
        cleanupTempJava();
        setBusy(false);
        setStatus(QStringLiteral("无法复制生成的服务端核心到 server.jar：") + m_saveDir);
        return;
    }
    // 原始的加载器核心（如 forge-…jar / neoforge-…jar / fabric-…jar）已复制为 server.jar，
    // 删除冗余原件；并清理安装器生成的启动脚本/元数据，使目录呈现为单个 server.jar。
    // 注意：libraries/ 与 minecraft_server-*.jar 必须保留——Forge/NeoForge 通用 jar 运行期依赖它们。
    QFile::remove(m_saveDir + QStringLiteral("/") + serverJar);
    cleanupInstallerLeftovers();
    // 记录本服务器使用的 Java（优先已安装/PATH；否则留空，启动时使用系统 Java）
    m_resolvedJava = m_java ? m_java->javaPathFor(m_currentVersion) : QString();
    m_dm->writeTextFile(m_saveDir + QStringLiteral("/eula.txt"),
                        QStringLiteral("eula=true\n# 由 MSM 创建服务器时自动生成（已同意 Minecraft EULA）\n"));
    if (!m_resolvedJava.isEmpty())
        m_dm->writeTextFile(m_saveDir + QStringLiteral("/.msm/java.txt"), m_resolvedJava + QStringLiteral("\n"));
    QStringList lines;
    for (const QString &l : m_selectedLoaders)
        lines << l;
    m_dm->writeTextFile(m_saveDir + QStringLiteral("/.msm/loaders.txt"),
                        lines.join(QStringLiteral("\n")) + QStringLiteral("\n"));
    if (!m_packageOnly && m_sm)
        m_sm->addServer(m_name, m_currentVersion, typeKey(), m_saveDir);

    if (zipPath.isEmpty()) {
        cleanupTempJava();   // 不打包：配置完模组服即清理临时 Java
        setProgress(100);
        setStageText(QString());
        setBusy(false);
        setDone(true);
        if (m_packageOnly)
            setStatus(QStringLiteral("服务端目录已就绪（未加入服务器列表）：") + m_saveDir);
        else
            setStatus(QStringLiteral("安装完成，已加入服务器列表（加载器：")
                      + m_selectedLoaders.join(QStringLiteral("/")) + QStringLiteral("）"));
    } else {
        setStage(1 + m_loaderTotal);   // 进入打包阶段
        setStageText(m_packageOnly
                      ? QStringLiteral("正在打包压缩包到下载文件夹…")
                      : QStringLiteral("正在打包压缩包…"));
        setStageProgress(20);
        setStatus(m_packageOnly
                  ? QStringLiteral("配置完成，正在打包压缩包到下载文件夹…")
                  : QStringLiteral("安装完成，正在打包压缩包…"));
        beginServerZip(zipPath);   // 打包完成（zipDone）后再清理临时 Java
    }
}

void CreateServerController::cleanupTempJava()
{
    if (m_java)
        m_java->cleanupTemp();
    m_tempJava.clear();
}

void CreateServerController::cleanupInstallerLeftovers()
{
    // 安装器额外生成的“启动脚本 / 元数据”文件，仅手动双击脚本时用到；
    // MSM 以 `java -jar server.jar` 直接从目录启动，这些文件无用，移除以保持目录整洁。
    // 仅按白名单删除，绝不触碰 .jar 与 libraries/，避免影响启动。
    static const QStringList clutter = {
        QStringLiteral("run.bat"), QStringLiteral("run.sh"), QStringLiteral("run.nogui.bat"),
        QStringLiteral("version.json"), QStringLiteral("user_jvm_args.txt"),
        QStringLiteral("win_args.txt"), QStringLiteral("unix_args.txt")
    };
    for (const QString &f : clutter) {
        const QString p = m_saveDir + QStringLiteral("/") + f;
        if (QFile::exists(p))
            QFile::remove(p);
    }
}

QString CreateServerController::loaderVersion(const QString &loader) const
{
    if (loader == QStringLiteral("forge")) {
        QDir d(m_saveDir);
        for (const QString &f : d.entryList(QStringList() << QStringLiteral("forge-*.jar"), QDir::Files)) {
            const auto m = QRegularExpression(QStringLiteral("forge-[^-]+-([^-]+)\\.jar")).match(f);
            if (m.hasMatch())
                return m.captured(1);
        }
    } else if (loader == QStringLiteral("neoforge")) {
        QDir d(m_saveDir);
        for (const QString &f : d.entryList(QStringList() << QStringLiteral("neoforge-*.jar"), QDir::Files)) {
            const auto m = QRegularExpression(QStringLiteral("neoforge-([^-]+)\\.jar")).match(f);
            if (m.hasMatch())
                return m.captured(1);
        }
    } else if (loader == QStringLiteral("fabric")) {
        // Fabric 安装器文件名不含加载器版本，版本号在 libraries/net/fabricmc/fabric-loader/<ver>/ 目录名中
        QDir ld(m_saveDir + QStringLiteral("/libraries/net/fabricmc/fabric-loader"));
        QString best;
        for (const QString &v : ld.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (best.isEmpty() || QVersionNumber::fromString(v) > QVersionNumber::fromString(best))
                best = v;
        }
        return best;
    }
    return QString();
}

QString CreateServerController::computeServerZipName() const
{
    QStringList types, vers;
    for (const QString &loader : m_selectedLoaders) {
        if (findLoaderJar(m_saveDir, loader).isEmpty())
            continue;
        types << loader;                       // 加载器类型（forge/neoforge/fabric）
        vers << loaderVersion(loader);        // 加载器版本
    }
    if (types.isEmpty())
        return QString();
    const QString name = m_currentVersion + QStringLiteral("-") + types.join(QStringLiteral("+"))
                       + QStringLiteral("-") + vers.join(QStringLiteral("+"));
    return QDir::cleanPath(QFileInfo(m_saveDir).dir().absoluteFilePath(name + QStringLiteral(".zip")));
}

void CreateServerController::beginServerZip(const QString &zipPath)
{
    m_zipPath = zipPath;
    m_zipAttempt = 0;
    QFile::remove(zipPath); // 覆盖旧包
    QDir().mkpath(QFileInfo(zipPath).dir().path());
    launchZipAttempt();
}

void CreateServerController::launchZipAttempt()
{
    if (m_zipProc)
        m_zipProc->deleteLater();
    m_zipProc = new QProcess(this);
    connect(m_zipProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                m_zipProc->deleteLater();
        m_zipProc = nullptr;
        const bool ok = (exitCode == 0) && QFile::exists(m_zipPath);
        if (ok) {
            zipDone(true);
            return;
        }
        // 第一次（PowerShell）失败则改用 tar 兜底；都失败则报非致命警告
        if (m_zipAttempt == 0) {
            ++m_zipAttempt;
            launchZipAttempt();
        } else {
            zipDone(false);
        }
    });
    if (m_zipAttempt == 0) {
        // Windows：PowerShell Compress-Archive（与解压用的 Expand-Archive 一致）
        // 通过 $args 传参，避免把路径拼进 PowerShell 命令字符串（路径含单引号会导致命令注入）
        const QString script = QStringLiteral("Compress-Archive -Force -Path ($args[0] + '\\*') -DestinationPath $args[1]");
        m_zipProc->start(QStringLiteral("powershell"),
                         QStringList() << QStringLiteral("-NoProfile") << QStringLiteral("-Command") << script
                                       << m_saveDir << m_zipPath);
    } else {
        // 兜底：Windows 10+ 自带 tar（bsdtar）支持 -a 自动按扩展名压缩，长路径更稳
        m_zipProc->start(QStringLiteral("tar"),
                         QStringList() << QStringLiteral("-a") << QStringLiteral("-cf") << m_zipPath
                                       << QStringLiteral("-C") << m_saveDir << QStringLiteral("."));
    }
}

void CreateServerController::zipDone(bool ok)
{
    setProgress(100);
    setStageText(QString());
    cleanupTempJava();   // 模组服配置并打包完成后，删除临时 Java 目录（不留下环境改动）
    setBusy(false);
    setDone(true);
    if (ok) {
        if (m_packageOnly) {
            // 仅打包模式：压缩包已自包含，删除下载文件夹中的临时构建目录，只保留 .zip
            QDir(m_saveDir).removeRecursively();
            setStatus(QStringLiteral("打包完成，已生成压缩包（未加入服务器列表）：") + m_zipPath);
        } else {
            setStatus(QStringLiteral("安装完成，已加入服务器列表并打包压缩包：") + m_zipPath);
        }
    } else {
        if (m_packageOnly)
            setStatus(QStringLiteral("服务端目录已就绪，但压缩包生成失败（可手动打包目录：")
                      + m_saveDir + QStringLiteral("）"));
        else
            setStatus(QStringLiteral("安装完成，已加入服务器列表，但压缩包生成失败（可手动打包目录：")
                      + m_saveDir + QStringLiteral("）"));
    }
}

QString CreateServerController::findLoaderJar(const QString &dir, const QString &loader) const
{
    // 关键：NeoForge/Forge 安装器把"补丁后的服务端核心"放到 libraries/ 的深层
    // （如 libraries/net/neoforged/neoforge/21.1.242/neoforge-21.1.242-server.jar），
    // 而不是保存目录根。仅扫根目录会漏掉，导致"安装完成但未找到任何加载器生成的服务端核心"。
    auto match = [&](const QString &baseName) -> bool {
        if (baseName.contains(QStringLiteral("installer"), Qt::CaseInsensitive))
            return false;
        if (loader == QStringLiteral("forge")) {
            return baseName.startsWith(QStringLiteral("forge-"), Qt::CaseInsensitive);
        } else if (loader == QStringLiteral("neoforge")) {
            // 匹配 neoforge-X.Y.Z-server.jar / neoforge-X.Y.Z.jar 等
            return baseName.startsWith(QStringLiteral("neoforge-"), Qt::CaseInsensitive);
        } else if (loader == QStringLiteral("fabric")) {
            return baseName.contains(QStringLiteral("fabric-server"), Qt::CaseInsensitive);
        }
        return false;
    };

    QStringList good;
    // 1) 根目录
    QDir d(dir);
    if (d.exists()) {
        const QStringList roots = d.entryList(QStringList() << QStringLiteral("*.jar"),
                                              QDir::Files, QDir::Name);
        for (const QString &j : roots) {
            if (match(j))
                good << j;
        }
    }
    // 2) libraries/ 递归扫描（NeoForge/Forge 安装器的实际产物位置）
    const QString libDir = dir + QStringLiteral("/libraries");
    if (QFileInfo::exists(libDir)) {
        QDirIterator it(libDir, QStringList() << QStringLiteral("*.jar"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString abs = it.next();
            const QString base = QFileInfo(abs).fileName();
            if (match(base)) {
                // 记录相对 dir 的路径（含 libraries/ 前缀）
                good << d.relativeFilePath(abs);
            }
        }
    }
    if (good.isEmpty())
        return QString();
    std::sort(good.begin(), good.end(), std::greater<QString>());
    return good.first();
}

// 统一解析/按需安装 Java：优先复用已安装 Java，缺失时由 JavaManager 按 MC 版本自动下载安装。
void CreateServerController::resolveJava(std::function<void(const QString &)> cb)
{
    const QString have = m_java ? m_java->javaPathFor(m_currentVersion) : QString();
    if (!have.isEmpty()) {
        cb(have);
        return;
    }
    if (m_java) {
        setStatus(QStringLiteral("正在准备 Java 运行环境（按版本自动下载）…"));
        // 让 Java 装到本服务器目录下的 jvm-{feature}/，与服务端核心同目录、不需要管理层
        const int feature = JavaManager::requiredFeature(m_currentVersion);
        m_java->setInstallBase(m_saveDir + QStringLiteral("/jvm-") + QString::number(feature));
        m_java->ensure(m_currentVersion, [this, cb](bool ok, QString path) {
            if (!ok) {
                setBusy(false);
                setStatus(QStringLiteral("自动准备 Java 失败：") + m_java->errorText()
                          + QStringLiteral("。可手动安装 JRE 后重试。"));
                return;
            }
            cb(path);
        });
        return;
    }
    const QString java = findJava();
    if (java.isEmpty()) {
        setBusy(false);
        setStatus(QStringLiteral("未找到 Java 运行环境，无法完成安装。请安装 JRE 并加入 PATH 后重试。"));
        return;
    }
    cb(java);
}

void CreateServerController::startInstallerProcess(const QString &java, const QString &loader)
{
    const QString installer = loader + QStringLiteral("-installer.jar");
    m_installerLog.clear();

    QStringList args;
    // 让独立的 java 安装器进程继承系统代理：主程序通过 Qt 使用系统代理
    // (setUseSystemConfiguration)，但派生的 java 进程默认不继承。
    args.append(systemProxyJvmArgs());
    // 安装器自身访问 maven.neoforged.net / libraries.minecraft.net 的
    // 连接超时；网络不稳时默认 15 s 可能不够，拉到 90 s 减少间歇性失败。
    args << QStringLiteral("-Dsun.net.client.defaultConnectTimeout=90000");
    args << QStringLiteral("-Dsun.net.client.defaultReadTimeout=90000");
    args << QStringLiteral("-Djava.awt.headless=true"); // 无显示器下避免 AWT 初始化崩溃
    args << QStringLiteral("-jar") << installer;
    if (loader == QStringLiteral("fabric")) {
        args << QStringLiteral("server")
             << QStringLiteral("-mcversion") << m_currentVersion
             << QStringLiteral("-downloadMinecraft");
    } else {
        args << QStringLiteral("--installServer");
    }

    setStatus(QStringLiteral("正在运行安装器生成服务端核心…（首次可能需下载 Minecraft，请稍候）"));
    m_installer = new QProcess(this);
    m_installer->setWorkingDirectory(m_saveDir);
    m_installer->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_installer, &QProcess::readyReadStandardOutput, this, [this]() {
        // 累积安装器输出，失败时取末尾展示真实原因（Java 版本不匹配、网络被墙等），
        // 同时消费管道避免缓冲区打满导致子进程阻塞。
        m_installerLog += QString::fromLocal8Bit(m_installer->readAllStandardOutput());
        if (m_installerLog.size() > 65536)
            m_installerLog.remove(0, m_installerLog.size() - 65536);
    });
    connect(m_installer, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CreateServerController::onInstallerFinished);
    m_installer->start(java, args);
    if (!m_installer->waitForStarted(10000)) {
        m_installer->deleteLater();
        m_installer = nullptr;
        setBusy(false);
        setStatus(QStringLiteral("启动安装器失败：") + java);
    }
}

QStringList CreateServerController::systemProxyJvmArgs() const
{
    // 把系统代理转换为 JVM 参数，使派生的 java 安装器进程能走与主程序相同的代理网络。
    QNetworkProxyQuery query(QUrl(QStringLiteral("https://maven.neoforged.net/releases/")),
                             QNetworkProxyQuery::UrlRequest);
    const QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(query);
    for (const QNetworkProxy &px : proxies) {
        if (px.type() == QNetworkProxy::NoProxy || px.hostName().isEmpty())
            continue;
        const QString h = px.hostName();
        const QString p = QString::number(px.port());
        return QStringList{
            QStringLiteral("-Dhttp.proxyHost=") + h,
            QStringLiteral("-Dhttp.proxyPort=") + p,
            QStringLiteral("-Dhttps.proxyHost=") + h,
            QStringLiteral("-Dhttps.proxyPort=") + p,
            QStringLiteral("-Dhttp.nonProxyHosts=")
        };
    }
    return {};   // 无系统代理：java 直连
}

QString CreateServerController::installerErrorTail() const
{
    if (m_installerLog.isEmpty())
        return QStringLiteral("（无输出，可能是安装器未启动或被安全软件拦截）");
    QString s = m_installerLog;
    // 仅保留末尾约 2000 字符 / 至多 25 行，聚焦真正的错误
    if (s.size() > 2000)
        s = s.right(2000);
    const QStringList lines = s.split(QLatin1Char('\n'));
    if (lines.size() <= 25)
        return s.trimmed();
    return QStringList(lines.mid(lines.size() - 25)).join(QLatin1Char('\n')).trimmed();
}

void CreateServerController::onInstallerFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_installer)
        return;
    m_installer->deleteLater();
    m_installer = nullptr;

    // 模组服：某个加载器安装完成 -> 进入下一加载器或收尾
    if (typeKey() == QStringLiteral("mod") && !m_activeLoader.isEmpty()) {
        const QString loader = m_activeLoader;
        m_activeLoader.clear();
        onModInstallerFinished(loader, exitCode, status);
        return;
    }

    if (exitCode != 0) {
        setBusy(false);
        setStatus(QStringLiteral("安装器执行失败（退出码 ") + QString::number(exitCode)
                  + QStringLiteral("），请检查 Java 与网络后重试。\n安装器输出：\n")
                  + installerErrorTail());
        return;
    }

    const QString jar = findServerJar(m_saveDir, typeKey());
    if (jar.isEmpty()) {
        setBusy(false);
        setStatus(QStringLiteral("安装完成但未找到生成的服务端核心 jar，请手动检查目录：") + m_saveDir);
        return;
    }
    const QString dst = m_saveDir + QStringLiteral("/server.jar");
    QFile::remove(dst);
    if (!QFile::copy(m_saveDir + QStringLiteral("/") + jar, dst)) {
        setBusy(false);
        setStatus(QStringLiteral("无法复制生成的服务端核心到 server.jar：") + m_saveDir);
        return;
    }
    // 清理安装器
    QFile::remove(m_saveDir + QStringLiteral("/forge-installer.jar"));
    QFile::remove(m_saveDir + QStringLiteral("/fabric-installer.jar"));

    finalizeCreate();
}

void CreateServerController::onDownloadError(const QString &id, const QString &message)
{
    qDebug() << "[MSM] onDownloadError id=" << id << "msg=" << message.left(160);
    // 模组服：某个加载器下载失败 -> 先尝试镜像兜底，仍失败则清理并报错
    if (m_loaderByTask.contains(id)) {
        m_loaderByTask.remove(id);
        const QString loader = m_activeLoader;
        if (!m_loaderFallbackUrl.isEmpty()) {
            const QString fb = m_loaderFallbackUrl;
            m_loaderFallbackUrl.clear();
            setStatus(QStringLiteral("主源下载失败，正在改用镜像下载 ") + loaderLabel(loader) + QStringLiteral(" 安装器…"));
            startLoaderDownload(loader, fb);
            return;
        }
        cleanupTempJava();
        setBusy(false);
        setStatus(QStringLiteral("下载失败：") + (message.isEmpty() ? QStringLiteral("未知错误") : message));
        return;
    }
    if (id != m_taskId)
        return;
    m_taskId.clear();
    // Vanilla/Paper：主下载源失败，用备用镜像重试
    if (!m_serverFallbackUrl.isEmpty()) {
        const QString fb = m_serverFallbackUrl;
        m_serverFallbackUrl.clear();
        setStatus(QStringLiteral("主源下载失败，正在改用镜像下载…"));
        const QString newId = m_dm->download(fb, m_saveDir, QStringLiteral("server.jar"));
        if (!newId.isEmpty()) {
            m_taskId = newId;
            return;
        }
    }
    setBusy(false);
    setStatus(QStringLiteral("下载失败：") + (message.isEmpty() ? QStringLiteral("未知错误") : message));
}
