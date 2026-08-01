/*
 * downloadcatalog.cpp —— 下载目录（资源/模组/整合包数据源）
 * -------------------------------------------------
 * 作为 QAbstractListModel 暴露给 QML 的 ListView：
 *   * 从 Modrinth / 官方接口拉取可下载条目并缓存。
 *   * 通过 DownloadManager 触发真实下载，并同步进度/完成/错误到模型。
 *   * status() 依据当前语言做轻量翻译（ts()），无需整表重排。
 */
#include "downloadcatalog.h"
#include "downloadmanager.h"
#include "javamanager.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QPointer>
#include <algorithm>
#include <QVersionNumber>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDir>

DownloadCatalog::DownloadCatalog(DownloadManager *dm, QObject *parent)
    : QAbstractListModel(parent), m_dm(dm)
{
    m_nam = new QNetworkAccessManager(this);
    m_saveDir = m_dm ? m_dm->defaultDownloadDir() : QString();

    if (m_dm) {
        connect(m_dm, &DownloadManager::progress, this, &DownloadCatalog::onDownloadProgress);
        connect(m_dm, &DownloadManager::finished, this, &DownloadCatalog::onDownloadFinished);
        connect(m_dm, &DownloadManager::error, this, &DownloadCatalog::onDownloadError);
    }
}

int DownloadCatalog::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_items.size();
}

QHash<int, QByteArray> DownloadCatalog::roleNames() const
{
    static const QHash<int, QByteArray> r = {
        {TitleRole, "title"},
        {SubtitleRole, "subtitle"},
        {UrlRole, "url"},
        {FilenameRole, "filename"},
        {ModrinthIdRole, "modrinthId"},
        {TaskIdRole, "taskId"},
        {DownloadingRole, "downloading"},
        {PercentRole, "percent"},
        {DoneRole, "done"},
        {FilePathRole, "filePath"},
        {ErrorRole, "errorText"}
    };
    return r;
}

QVariant DownloadCatalog::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const DownloadItem *it = m_items.at(index.row());
    switch (role) {
    case TitleRole:     return it->title();
    case SubtitleRole:  return it->subtitle();
    case UrlRole:       return it->url();
    case FilenameRole:  return it->filename();
    case ModrinthIdRole:return it->modrinthId();
    case TaskIdRole:    return it->taskId();
    case DownloadingRole:return it->downloading();
    case PercentRole:   return it->percent();
    case DoneRole:      return it->done();
    case FilePathRole:  return it->filePath();
    case ErrorRole:     return it->errorText();
    }
    return {};
}

QVariantList DownloadCatalog::items() const
{
    QVariantList out;
    for (const DownloadItem *it : m_items) {
        QVariantMap m;
        m[QStringLiteral("title")] = it->title();
        m[QStringLiteral("subtitle")] = it->subtitle();
        m[QStringLiteral("url")] = it->url();
        m[QStringLiteral("filename")] = it->filename();
        m[QStringLiteral("modrinthId")] = it->modrinthId();
        m[QStringLiteral("taskId")] = it->taskId();
        m[QStringLiteral("downloading")] = it->downloading();
        m[QStringLiteral("percent")] = it->percent();
        m[QStringLiteral("done")] = it->done();
        m[QStringLiteral("error")] = it->errorText();
        out << m;
    }
    return out;
}

void DownloadCatalog::setStatus(const QString &s)
{
    if (m_statusRaw != s) {
        m_statusRaw = s;
        emit statusChanged();
    }
}

QString DownloadCatalog::status() const
{
    return ts(m_statusRaw);
}

void DownloadCatalog::setLanguage(const QString &v)
{
    if (m_language != v) {
        m_language = v;
        emit languageChanged();
        // 语言切换后状态栏需实时重译
        emit statusChanged();
    }
}

QString DownloadCatalog::ts(const QString &zh) const
{
    if (m_language != QStringLiteral("English"))
        return zh;
    static const QHash<QString, QString> map = {
        {QStringLiteral("暂无可下载的服务端版本"), QStringLiteral("No downloadable server versions")},
        {QStringLiteral("加载服务端列表失败（网络异常）：%1"), QStringLiteral("Failed to load server list (network error): %1")},
        {QStringLiteral("加载官方原版服务端列表…"), QStringLiteral("Loading official vanilla server list…")},
        {QStringLiteral("获取版本列表失败"), QStringLiteral("Failed to fetch version list")},
        {QStringLiteral("获取版本列表失败：%1"), QStringLiteral("Failed to fetch version list: %1")},
        {QStringLiteral("共 %1 个版本"), QStringLiteral("Total %1 versions")},
        {QStringLiteral("选择 Minecraft 版本与加载器后，批量下载安装器"), QStringLiteral("Select a Minecraft version and loader, then batch-download the installer")},
        {QStringLiteral("获取下载地址失败：%1"), QStringLiteral("Failed to resolve download URL: %1")},
        {QStringLiteral("请先选择 Minecraft 版本"), QStringLiteral("Please select a Minecraft version first")},
        {QStringLiteral("请先勾选至少一个加载器"), QStringLiteral("Please check at least one loader first")},
        {QStringLiteral("无法解析 %1 下载链接"), QStringLiteral("Cannot resolve download link for %1")},
        {QStringLiteral("%1 与所选版本 %2 不兼容"), QStringLiteral("%1 is incompatible with the selected version %2")},
        {QStringLiteral("%1 安装器已加入下载列表（见右下角下载面板）"), QStringLiteral("%1 installer added to the download list (see the download panel at bottom-right)")},
        {QStringLiteral("已清理临时 Java 环境"), QStringLiteral("Temporary Java environment cleaned")},
        {QStringLiteral("Java 管理器不可用"), QStringLiteral("Java manager unavailable")},
        {QStringLiteral("无结果"), QStringLiteral("No results")},
        {QStringLiteral("加载失败（网络异常）：%1"), QStringLiteral("Failed to load (network error): %1")},
        {QStringLiteral("Java 管理器未初始化"), QStringLiteral("Java manager not initialized")},
        {QStringLiteral("Java %1 已准备（临时）"), QStringLiteral("Java %1 ready (temporary)")},
        {QStringLiteral("正在临时准备 Java %1 …"), QStringLiteral("Preparing temporary Java %1 …")},
        {QStringLiteral("Java %1 准备失败"), QStringLiteral("Java %1 preparation failed")},
        {QStringLiteral("Java %1 已临时准备：%2"), QStringLiteral("Java %1 temporarily prepared: %2")},
        {QStringLiteral("正在解析 Minecraft %1 下载地址…"), QStringLiteral("Resolving Minecraft %1 download URL…")},
        {QStringLiteral("无法解析 Minecraft %1 下载地址"), QStringLiteral("Cannot resolve Minecraft %1 download URL")},
        {QStringLiteral("无可用版本: %1"), QStringLiteral("No available version: %1")},
        {QStringLiteral("无可用文件: %1"), QStringLiteral("No available file: %1")},
        {QStringLiteral("获取版本失败: %1"), QStringLiteral("Failed to fetch version: %1")},
        {QStringLiteral("下载地址为空"), QStringLiteral("Download URL is empty")},
        {QStringLiteral("无法开始下载: %1"), QStringLiteral("Cannot start download: %1")},
        {QStringLiteral("开始下载：%1"), QStringLiteral("Started download: %1")},
        {QStringLiteral("已取消"), QStringLiteral("Cancelled")},
        {QStringLiteral("下载完成"), QStringLiteral("Download complete")},
        {QStringLiteral("没有可打开的文件"), QStringLiteral("No file to open")},
        {QStringLiteral("下载失败"), QStringLiteral("Download failed")},
        {QStringLiteral("JSON 解析失败: %1"), QStringLiteral("JSON parse failed: %1")},
        {QStringLiteral("所有镜像源均不可用"), QStringLiteral("All mirror sources are unavailable")},
        {QStringLiteral("响应解析失败"), QStringLiteral("Response parse failed")}
    };
    auto it = map.constFind(zh);
    return it != map.constEnd() ? it.value() : zh;
}

void DownloadCatalog::setLoading(bool b)
{
    if (m_loading != b) {
        m_loading = b;
        emit loadingChanged();
    }
}

void DownloadCatalog::clearItems()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    qDeleteAll(m_items);
    m_items.clear();
    endResetModel();
    emit countChanged();
}

void DownloadCatalog::appendItem(DownloadItem *it)
{
    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.append(it);
    endInsertRows();
    connect(it, &DownloadItem::stateChanged, this, [this, it]() {
        const int row = m_items.indexOf(it);
        if (row >= 0)
            emit dataChanged(index(row), index(row));
    });
    emit countChanged();
}

void DownloadCatalog::setCurrentKey(const QString &k)
{
    if (m_currentKey != k) {
        m_currentKey = k;
        emit currentKeyChanged();
    }
}

void DownloadCatalog::setServerType(const QString &t)
{
    if (m_serverType != t) {
        m_serverType = t;
        emit serverTypeChanged();
    }
}

void DownloadCatalog::setSearchText(const QString &t)
{
    if (m_searchText != t) {
        m_searchText = t;
        emit searchTextChanged();
    }
}

void DownloadCatalog::setSaveDir(const QString &d)
{
    // 规范化：Windows 上 QStandardPaths 返回反斜杠，FolderDialog 可能带回 file:// 前缀或多余斜杠，
    // 统一为正斜杠的绝对路径，避免盘符前叠加出多余的 “\”。
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

void DownloadCatalog::setJavaManager(JavaManager *j)
{
    if (m_java == j)
        return;
    if (m_java)
        disconnect(m_java, &JavaManager::hasTempJavaChanged, this, &DownloadCatalog::hasTempJavaChanged);
    m_java = j;
    if (m_java)
        connect(m_java, &JavaManager::hasTempJavaChanged, this, &DownloadCatalog::hasTempJavaChanged);
}

bool DownloadCatalog::hasTempJava() const
{
    return m_java ? m_java->hasTempJava() : false;
}

void DownloadCatalog::setModLoaderType(const QString &t)
{
    if (m_modLoaderType != t) {
        m_modLoaderType = t;
        emit modLoaderTypeChanged();
    }
}

void DownloadCatalog::refresh()
{
    if (m_currentKey == QStringLiteral("java"))
        loadJava();
    else if (m_currentKey == QStringLiteral("server"))
        loadServer();
    else
        loadModrinth();
}

void DownloadCatalog::fetchJson(const QString &url,
                                std::function<void(const QJsonDocument &)> onOk,
                                std::function<void(const QString &)> onErr)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MinecraftServerManager/1.0 (https://github.com/MinecraftServerManager; contact: msm@example.com)"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(45000); // 卡死（无数据）时 45s 后失败，避免 WebUI 列表永久“加载中”
    QNetworkReply *reply = m_nam->get(req);
    // 总超时兜底：Qt 的 TransferTimeout 不覆盖“连接建立阶段”卡死（被墙域名），用 QTimer 强制结束。
    QTimer *guard = new QTimer(reply);
    guard->setSingleShot(true);
    guard->setInterval(30000);
    QObject::connect(guard, &QTimer::timeout, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
    guard->start();
    ++m_inflight;
    setLoading(m_inflight > 0);
    connect(reply, &QNetworkReply::finished, this, [this, reply, onOk, onErr]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString err = reply->errorString();
        reply->deleteLater();
        if (!ok) {
            onErr(err);
            --m_inflight;
            setLoading(m_inflight > 0);
            return;
        }
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (doc.isNull()) {
            onErr(ts("JSON 解析失败: %1").arg(pe.errorString()));
            --m_inflight;
            setLoading(m_inflight > 0);
            return;
        }
        onOk(doc);   // 回调中可能再次发起 fetchJson（子请求会让计数先增后减，避免 loading 闪烁）
        --m_inflight;
        setLoading(m_inflight > 0);
    });
}

QStringList DownloadCatalog::modrinthBases()
{
    // 直接使用官方 Modrinth（源可用）
    return { QStringLiteral("https://api.modrinth.com/v2") };
}

void DownloadCatalog::fetchFirst(const QStringList &urls,
                                 const std::function<void(const QJsonDocument &)> &onOk,
                                 const std::function<void(const QString &)> &onErr,
                                 int idx)
{
    if (idx >= urls.size()) {
        onErr(ts("所有镜像源均不可用"));
        return;
    }
    QNetworkRequest req{QUrl(urls.at(idx))};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MSM"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(45000);
    QNetworkReply *reply = m_nam->get(req);
    // 总超时兜底：Qt 的 TransferTimeout 不覆盖“连接建立阶段”卡死（被墙域名），用 QTimer 强制结束。
    QTimer *guard = new QTimer(reply);
    guard->setSingleShot(true);
    guard->setInterval(30000);
    QObject::connect(guard, &QTimer::timeout, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
    guard->start();
    ++m_inflight;
    setLoading(m_inflight > 0);
    connect(reply, &QNetworkReply::finished, this, [this, reply, urls, onOk, onErr, idx]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString err = reply->errorString();
        reply->deleteLater();
        if (ok) {
            QJsonParseError pe;
            const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
            if (!doc.isNull()) {
                --m_inflight; setLoading(m_inflight > 0);
                onOk(doc);
                return;
            }
            // 响应无法解析，尝试下一个镜像源
        }
        --m_inflight; setLoading(m_inflight > 0);
        if (idx + 1 < urls.size())
            fetchFirst(urls, onOk, onErr, idx + 1);
        else
            onErr(ok ? ts("响应解析失败") : err);
    });
}

void DownloadCatalog::loadJava()
{
    setStatus(QString());
    clearItems();
    setLoading(true);
    // 给出常用 JDK 版本列表，下载走 java:// 内部标记，点击后由 JavaManager::resolveDownloadUrl 解析
    // 安装器（exe/msi）直链，再交统一下载管理器下载到本地（任务进入“下载任务”面板）。不解压、不安装，
    // 由用户用“打开文件”按钮自行运行安装器。压缩包 Java（可移植 JDK）仅用于“创建服务器/模组服”。
    // 8/11 为旧 LTS（均不走甲骨文：8 强制登录、11 otn 微下载易崩，统一改走 Adoptium Temurin 公开 latest 直链）；
    // 22/23 对 Minecraft 无意义，不列出；25 为当前 LTS，26 为最新特性版。
    const QList<int> versions = { 25, 21, 17, 11, 8, 26 };
    for (const int v : versions) {
        // 8/11/17/21/25 为 LTS，26 为半年期特性版，标签据此区分
        const bool isLts = (v == 8 || v == 11 || v == 17 || v == 21 || v == 25);
        const QString tag = isLts ? QStringLiteral("LTS") : QStringLiteral("特性版");
#ifdef Q_OS_WIN
        // Windows：提供 exe/msi 安装器，下载后由用户运行安装
        appendItem(new DownloadItem(QStringLiteral("OpenJDK ") + QString::number(v),
                                     tag + QStringLiteral(" · Windows x64 · JDK（下载到本地，运行安装）"),
                                     QStringLiteral("java://%1").arg(v),
                                     QStringLiteral("OpenJDK%1-installer.msi").arg(v),
                                     QString(), this));
#else
        // Linux/macOS：无安装器，提供可移植压缩包，下载中心会自动解压安装为可移植 JDK。
        // 按真实架构显示/命名（避免 aarch64/arm64 机器上误标 x64）；压缩包内容按魔数自动识别解压。
        const QString arch = JavaManager::hostArchitecture();
        appendItem(new DownloadItem(QStringLiteral("OpenJDK ") + QString::number(v),
                                     tag + QStringLiteral(" · Linux %1 · JDK（下载后自动解压安装）").arg(arch),
                                     QStringLiteral("java://%1").arg(v),
                                     QStringLiteral("OpenJDK%1.tar.gz").arg(v),
                                     QString(), this));
#endif
    }
    setLoading(false);
    setStatus(QString());
}

void DownloadCatalog::loadServer()
{
    clearItems();
    if (m_serverType == QStringLiteral("paper"))        loadPaper();
    else if (m_serverType == QStringLiteral("vanilla")) loadVanilla();
    else if (m_serverType == QStringLiteral("mod"))     loadMod();
    else                                                 loadPaper();
}

void DownloadCatalog::loadPaper()
{
    setStatus(QString());
    // PaperMC 旧 v2 API（api.papermc.io）已于 2026-07-01 停用，迁移到 Fill v3（fill.papermc.io）。
    // 下载键由 application 改为 server:default，下载地址直接内嵌在响应里。
    fetchJson(QStringLiteral("https://fill.papermc.io/v3/projects/paper/versions"),
        [this](const QJsonDocument &d) {
            QStringList versions;
            const QJsonArray vs = d.object().value(QStringLiteral("versions")).toArray();
            for (const auto &e : vs) {
                const QString id = e.toObject().value(QStringLiteral("version"))
                                        .toObject().value(QStringLiteral("id")).toString();
                if (!id.isEmpty()) versions << id;
            }
            // 语义版本降序：最新版本在前，取前 15 个
            std::sort(versions.begin(), versions.end(), [](const QString &a, const QString &b) {
                return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
            });
            QStringList sel;
            const qsizetype count = std::min(qsizetype(15), versions.size());
            for (int i = 0; i < count; ++i) sel << versions.at(i);

            clearItems();
            if (sel.isEmpty()) { setStatus(ts("暂无可下载的服务端版本")); return; }

            int *pending = new int(sel.size());
            for (const QString &v : sel) {
                fetchJson(QStringLiteral("https://fill.papermc.io/v3/projects/paper/versions/") + v + QStringLiteral("/builds"),
                    [this, v, pending](const QJsonDocument &bd) {
                        const QJsonArray builds = bd.array();
                        if (!builds.isEmpty()) {
                            // v3 builds 按 id 降序排列，首个即最新构建
                            const QJsonObject latest = builds.first().toObject();
                            const int buildNo = latest.value(QStringLiteral("id")).toInt();
                            const QJsonObject dl = latest.value(QStringLiteral("downloads"))
                                                        .toObject().value(QStringLiteral("server:default")).toObject();
                            const QString url = dl.value(QStringLiteral("url")).toString();
                            const QString name = dl.value(QStringLiteral("name")).toString();
                            if (!url.isEmpty()) {
                                appendItem(new DownloadItem(QStringLiteral("Paper ") + v,
                                                            QStringLiteral("build ") + QString::number(buildNo),
                                                            url, name, QString(), this));
                            }
                        }
                        if (--(*pending) == 0) { delete pending; setStatus(QString()); }
                    },
                    [pending](const QString &) {
                        if (--(*pending) == 0) { delete pending; }
                    });
            }
        },
        [this](const QString &e) { setStatus(ts("加载服务端列表失败（网络异常）：%1").arg(e)); });
}



void DownloadCatalog::loadVanilla()
{
    clearItems();
    setLoading(true);
    setStatus(ts("加载官方原版服务端列表…"));
    fetchMcReleases([this](const QStringList &rels) {
        setLoading(false);
        if (rels.isEmpty()) { setStatus(ts("获取版本列表失败")); return; }
        for (const QString &ver : rels) {
            appendItem(new DownloadItem(QStringLiteral("Minecraft ") + ver,
                                        QStringLiteral("官方原版服务端 (Vanilla)"),
                                        QStringLiteral("vanilla://") + ver,
                                        QStringLiteral("minecraft_server.") + ver + QStringLiteral(".jar"),
                                        QString(), this));
        }
        setStatus(ts("共 %1 个版本").arg(rels.size()));
    });
}

void DownloadCatalog::loadMod()
{
    clearItems();
    setLoading(false);
    setStatus(ts("选择 Minecraft 版本与加载器后，批量下载安装器"));
    if (m_mcReleases.isEmpty()) {
        fetchMcReleases([this](const QStringList &rels) {
            emit mcReleasesChanged();
            if (m_modVersion.isEmpty() && !rels.isEmpty())
                setModVersion(rels.first());   // 列表降序，首个为最新版本
        });
    } else if (m_modVersion.isEmpty()) {
        setModVersion(m_mcReleases.first());   // 列表降序，首个为最新版本
    }
}

void DownloadCatalog::fetchText(const QString &url,
                                std::function<void(const QString &)> onOk,
                                std::function<void(const QString &)> onErr)
{
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(45000);
    QNetworkReply *reply = m_nam->get(req);
    // 总超时兜底：Qt 的 TransferTimeout 不覆盖“连接建立阶段”卡死（被墙域名），用 QTimer 强制结束。
    QTimer *guard = new QTimer(reply);
    guard->setSingleShot(true);
    guard->setInterval(30000);
    QObject::connect(guard, &QTimer::timeout, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
    guard->start();
    connect(reply, &QNetworkReply::finished, this, [this, reply, onOk, onErr]() {
        if (reply->error() == QNetworkReply::NoError)
            onOk(QString::fromUtf8(reply->readAll()));
        else
            onErr(reply->errorString());
        reply->deleteLater();
    });
}

void DownloadCatalog::fetchMcReleases(std::function<void(const QStringList &)> cb)
{
    if (m_mcReleasesFetching) { cb(m_mcReleases); return; }
    m_mcReleasesFetching = true;
    fetchJson(QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest_v2.json"),
        [this, cb](const QJsonDocument &doc) {
            QStringList rels;
            const QJsonArray versions = doc.object().value(QStringLiteral("versions")).toArray();
            for (const QJsonValue &v : versions) {
                const QJsonObject o = v.toObject();
                if (o.value(QStringLiteral("type")).toString() == QStringLiteral("release"))
                    rels.append(o.value(QStringLiteral("id")).toString());
            }
            // 语义版本降序：最新版本在前，展示全部正式版
            std::sort(rels.begin(), rels.end(), [](const QString &a, const QString &b) {
                return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
            });
            m_mcReleases = rels;
            m_mcReleasesFetching = false;
            emit mcReleasesChanged();   // 统一在此发信号：无论哪个入口触发拉取，QML 绑定都能刷新
            if (m_modVersion.isEmpty() && !rels.isEmpty())
                setModVersion(rels.first());   // 兜底默认版本（并发拉取时调用方可能拿到空列表）
            cb(rels);
        },
        [this, cb](const QString &err) {
            m_mcReleasesFetching = false;
            setStatus(ts("获取版本列表失败：%1").arg(err));
            cb(QStringList());
        });
}

void DownloadCatalog::resolveVanillaUrl(const QString &version, std::function<void(const QString &)> cb)
{
    fetchJson(QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest_v2.json"),
        [this, version, cb](const QJsonDocument &doc) {
            QString url;
            const QJsonArray versions = doc.object().value(QStringLiteral("versions")).toArray();
            for (const QJsonValue &v : versions) {
                const QJsonObject o = v.toObject();
                if (o.value(QStringLiteral("id")).toString() == version) {
                    url = o.value(QStringLiteral("url")).toString();
                    break;
                }
            }
            if (url.isEmpty()) { cb(QString()); return; }
            fetchJson(url, [cb](const QJsonDocument &doc2) {
                const QJsonObject d = doc2.object()
                                          .value(QStringLiteral("downloads")).toObject()
                                          .value(QStringLiteral("server")).toObject();
                cb(d.value(QStringLiteral("url")).toString());
            }, [this, cb](const QString &e) { setStatus(ts("获取下载地址失败：%1").arg(e)); cb(QString()); });
        },
        [this, cb](const QString &e) { setStatus(ts("获取下载地址失败：%1").arg(e)); cb(QString()); });
}

static QString neoForgeBranch(const QString &mc)
{
    // NeoForge 版本号 "主.次.修订" 中的 "主.次" 直接对应 Minecraft 版本去掉前导 "1."
    // 例如 MC 1.20.4 -> "20.4"，1.21.1 -> "21.1"
    const QStringList p = mc.split(QLatin1Char('.'));
    if (p.size() >= 3 && p.at(0) == QStringLiteral("1"))
        return p.at(1) + QLatin1Char('.') + p.at(2);
    if (p.size() == 2 && p.at(0) == QStringLiteral("1"))
        return p.at(1);
    return mc;
}

void DownloadCatalog::resolveLoaderUrl(const QString &loader, const QString &version,
                                       std::function<void(const QString &)> cb)
{
    if (loader == QStringLiteral("fabric")) {
        cb(QStringLiteral("https://maven.fabricmc.net/net/fabricmc/fabric-installer/1.1.1/fabric-installer-1.1.1.jar"));
        return;
    }
    if (loader == QStringLiteral("forge")) {
        fetchJson(QStringLiteral("https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json"),
            [version, cb](const QJsonDocument &doc) {
                const QJsonObject promos = doc.object().value(QStringLiteral("promos")).toObject();
                // promotions_slim.json 的 promos 值是版本字符串（如 "47.4.10"），
                // 必须保留完整字符串，不能用 toInt() 截断（否则拼出不存在的 1.x-47 路径）。
                QString build;
                auto pick = [&](const QString &key) {
                    if (!promos.contains(key)) return;
                    const QString b = promos.value(key).toString();
                    if (b.isEmpty()) return;
                    if (build.isEmpty() ||
                        QVersionNumber::fromString(b) > QVersionNumber::fromString(build))
                        build = b;
                };
                pick(version + QStringLiteral("-recommended"));
                pick(version + QStringLiteral("-latest"));
                if (build.isEmpty()) { cb(QString()); return; }
                cb(QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/") + version + QStringLiteral("-") + build +
                   QStringLiteral("/forge-") + version + QStringLiteral("-") + build + QStringLiteral("-installer.jar"));
            },
            [this, cb](const QString &e) { setStatus(ts("获取下载地址失败：%1").arg(e)); cb(QString()); });
        return;
    }
    if (loader == QStringLiteral("neoforge")) {
        const QString branch = neoForgeBranch(version);
        fetchText(QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml"),
            [cb, branch](const QString &xml) {
                QRegularExpression re(QStringLiteral("<version>([^<]+)</version>"));
                QString stable, fallback;
                auto it = re.globalMatch(xml);
                while (it.hasNext()) {
                    const QString v = it.next().captured(1);
                    const QString base = v.section(QLatin1Char('-'), 0, 0);  // strip -beta/-alpha suffix
                    const QString br = base.section(QLatin1Char('.'), 0, 1);
                    if (br != branch) continue;
                    fallback = v;
                    if (!v.contains(QLatin1Char('-'))) stable = v;  // prefer stable builds
                }
                const QString chosen = stable.isEmpty() ? fallback : stable;
                if (chosen.isEmpty()) { cb(QString()); return; }
                cb(QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/") + chosen +
                   QStringLiteral("/neoforge-") + chosen + QStringLiteral("-installer.jar"));
            },
            [this, cb](const QString &e) { setStatus(ts("获取下载地址失败：%1").arg(e)); cb(QString()); });
        return;
    }
    cb(QString());
}

QStringList DownloadCatalog::modLoaders() const
{
    static const QStringList list = {
        QStringLiteral("forge"), QStringLiteral("fabric"), QStringLiteral("neoforge")
    };
    return list;
}

QString DownloadCatalog::loaderLabel(const QString &loader) const
{
    if (loader == QStringLiteral("forge"))    return QStringLiteral("Forge");
    if (loader == QStringLiteral("fabric"))   return QStringLiteral("Fabric");
    if (loader == QStringLiteral("neoforge")) return QStringLiteral("NeoForge");
    return loader;
}

bool DownloadCatalog::loaderCompatible(const QString &loader, const QString &version) const
{
    if (loader == QStringLiteral("fabric"))   return true;
    if (loader == QStringLiteral("forge"))    return version >= QStringLiteral("1.1");
    if (loader == QStringLiteral("neoforge")) {
        if (version < QStringLiteral("1.20.1")) return false;
        if (version == QStringLiteral("1.20")) {
            const int pat = version.section(QLatin1Char('.'), 2, 2).toInt();
            return pat >= 1;
        }
        return true;
    }
    return false;
}

void DownloadCatalog::toggleLoader(const QString &loader)
{
    const int idx = m_modLoaders.indexOf(loader);
    if (idx >= 0) m_modLoaders.removeAt(idx);
    else          m_modLoaders.append(loader);
    emit selectedLoadersChanged();
}

void DownloadCatalog::setModVersion(const QString &v)
{
    if (m_modVersion == v) return;
    m_modVersion = v;
    emit modVersionChanged();
}

void DownloadCatalog::setSelectedLoaders(const QStringList &v)
{
    if (m_modLoaders == v) return;
    m_modLoaders = v;
    emit selectedLoadersChanged();
}

void DownloadCatalog::setPackager(const std::function<void()> &f)
{
    m_packager = f;
}

void DownloadCatalog::downloadSelectedLoaders()
{
    if (m_modVersion.isEmpty()) { setStatus(ts("请先选择 Minecraft 版本")); return; }
    if (m_modLoaders.isEmpty()) { setStatus(ts("请先勾选至少一个加载器")); return; }

    // “下载并打包选中加载器”：交由上层注入的打包委托完成（复用 CreateServerController，
    // setSkipAddList=true，与本地端“创建服务器”完全一致：准备 Java + 跑安装器 + 产出可运行
    // 服务端）。未注入委托时回退为仅下载安装器到列表。
    if (m_packager) {
        setStatus(ts("正在打包模组服（准备 Java + 运行安装器）…"));
        m_packager();
        return;
    }
    for (const QString &loader : m_modLoaders) {
        if (!loaderCompatible(loader, m_modVersion)) continue;
        resolveLoaderUrl(loader, m_modVersion, [this, loader](const QString &u) {
            if (u.isEmpty()) { setStatus(ts("无法解析 %1 下载链接").arg(loaderLabel(loader))); return; }
            DownloadItem *it = new DownloadItem(loaderLabel(loader) + QStringLiteral(" ") + m_modVersion,
                                                QStringLiteral("模组服加载器安装器"),
                                                u,
                                                loader + QStringLiteral("-installer-") + m_modVersion + QStringLiteral(".jar"),
                                                QString(), this);
            appendItem(it);
            startDownload(it, u, it->filename());
        });
    }
}

void DownloadCatalog::downloadLoader(const QString &loader)
{
    if (m_modVersion.isEmpty()) {
        setStatus(ts("请先选择 Minecraft 版本"));
        return;
    }
    if (!loaderCompatible(loader, m_modVersion)) {
        setStatus(loaderLabel(loader) + QStringLiteral(" 与所选版本 ") + m_modVersion
                  + QStringLiteral(" 不兼容"));
        return;
    }
    resolveLoaderUrl(loader, m_modVersion, [this, loader](const QString &u) {
        if (u.isEmpty()) {
            setStatus(ts("无法解析 %1 下载链接").arg(loaderLabel(loader)));
            return;
        }
        DownloadItem *it = new DownloadItem(loaderLabel(loader) + QStringLiteral(" ") + m_modVersion,
                                            QStringLiteral("模组服加载器安装器"),
                                            u,
                                            loader + QStringLiteral("-installer-") + m_modVersion + QStringLiteral(".jar"),
                                            QString(), this);
        appendItem(it);
        startDownload(it, u, it->filename());
        setStatus(ts("%1 安装器已加入下载列表（见右下角下载面板）").arg(loaderLabel(loader)));
    });
}

void DownloadCatalog::cleanupTempJava()
{
    if (m_java) {
        m_java->cleanupTemp();
        setStatus(ts("已清理临时 Java 环境"));
    } else {
        setStatus(ts("Java 管理器不可用"));
    }
}

void DownloadCatalog::loadModrinth()
{
    setStatus(QString());
    clearItems();
    const QString type = m_currentKey; // mod / modpack / resourcepack / plugin
    QStringList urls;
    for (const QString &base : modrinthBases())
        urls.append(base + QStringLiteral("/search?limit=20&facets=[[\"project_type:") + type
                        + QStringLiteral("\"]]")
                        + (m_searchText.isEmpty() ? QString()
                           : QStringLiteral("&query=") + QString::fromUtf8(QUrl::toPercentEncoding(m_searchText))));
    fetchFirst(urls,
        [this](const QJsonDocument &d) {
            const QJsonArray hits = d.object().value(QStringLiteral("hits")).toArray();
            clearItems();
            for (const auto &h : hits) {
                const QJsonObject o = h.toObject();
                QString desc = o.value(QStringLiteral("description")).toString();
                if (desc.size() > 80) desc = desc.left(80);
                appendItem(new DownloadItem(o.value(QStringLiteral("title")).toString(),
                                            desc,
                                            QString(), QString(),
                                            o.value(QStringLiteral("project_id")).toString(), this));
            }
            setStatus(hits.isEmpty() ? ts("无结果") : QString());
        },
        [this](const QString &e) { setStatus(ts("加载失败（网络异常）：%1").arg(e)); });
}

void DownloadCatalog::download(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    DownloadItem *it = m_items.at(index);
    if (it->downloading())
        return;

    // Java 条目：java:// 标记，下载“安装包（exe/msi）原文件”到本地，由用户自行运行安装器。
    // 先解析安装器直链（exe/msi），再直接交统一下载管理器下载——任务自然进入“下载任务”面板并展示进度；
    // 下载中心这一行只提示“已在下载任务面板中查看进度”，不解压、不安装（压缩包 Java 仅用于创建/模组服）。
    if (it->url().startsWith(QStringLiteral("java://"))) {
        if (!m_java) { setStatus(ts("Java 管理器未初始化")); return; }
        const int feat = it->url().mid(QStringLiteral("java://").length()).toInt();
        if (it->done()) { setStatus(ts("Java %1 安装包已下载").arg(feat)); return; }
        it->setDownloading(true); it->setPercent(0); it->setDone(false); it->setErrorText(QString());
        setStatus(ts("正在解析 Java %1 安装包直链…").arg(feat));
        QPointer<DownloadCatalog> self(this);
        QPointer<DownloadItem> item(it);
        m_java->resolveDownloadUrl(feat, [self, item, feat](bool ok, const QString &link, const QString &fname) {
            if (!self || !item) return;   // 界面/条目已销毁，安全退出
            if (!ok || link.isEmpty()) {
                item->setDownloading(false);
                item->setErrorText(self->ts("Java %1 安装包直链解析失败").arg(feat));
                self->setStatus(self->ts("Java %1 安装包直链解析失败，请检查网络或手动下载").arg(feat));
                return;
            }
            // 直接下载安装器原文件（exe/msi）到本地；进度/完成/出错由 onDownload* 统一处理并同步到面板
            const QString id = self->m_dm->download(link, self->m_saveDir, fname, item->title());
            if (id.isEmpty()) {
                item->setDownloading(false);
                item->setErrorText(self->ts("Java %1 无法开始下载").arg(feat));
                self->setStatus(self->ts("Java %1 无法开始下载").arg(feat));
                return;
            }
            item->setTaskId(id);   // 关联到统一列表，这一行与“下载任务”面板进度/状态同步
            self->setStatus(self->ts("正在下载 Java %1 安装包（进度见下载任务面板）…").arg(feat));
        });
        return;
    }

    // Vanilla 原版：vanilla://<version> 标记，下载时再解析真实 server.jar 直链
    if (it->url().startsWith(QStringLiteral("vanilla://"))) {
        const QString ver = it->url().mid(QStringLiteral("vanilla://").length());
        if (it->downloading()) return;
        it->setDownloading(true); it->setPercent(0); it->setDone(false); it->setErrorText(QString());
        setStatus(ts("正在解析 Minecraft %1 下载地址…").arg(ver));
        QPointer<DownloadCatalog> self(this);
        QPointer<DownloadItem> item(it);
        resolveVanillaUrl(ver, [self, item, ver](const QString &realUrl) {
            if (!self || !item) return;
            if (realUrl.isEmpty()) {
                item->setDownloading(false);
                item->setErrorText(self->ts("无法解析 Minecraft %1 下载地址").arg(ver));
                return;
            }
            self->startDownload(item, realUrl, item->filename());
        });
        return;
    }

    if (!it->modrinthId().isEmpty()) {
        // Modrinth：再发一次请求拿版本文件直链
        QStringList urls;
        for (const QString &base : modrinthBases())
            urls.append(base + QStringLiteral("/project/") + it->modrinthId() + QStringLiteral("/version"));
        fetchFirst(urls,
            [this, it](const QJsonDocument &d) {
                const QJsonArray versions = d.array();
                if (versions.isEmpty()) { setStatus(ts("无可用版本: %1").arg(it->title())); return; }
                QJsonObject f;
                for (const auto &x : versions.first().toObject().value(QStringLiteral("files")).toArray()) {
                    if (x.toObject().value(QStringLiteral("primary")).toBool()) { f = x.toObject(); break; }
                }
                if (f.isEmpty() && !versions.first().toObject().value(QStringLiteral("files")).toArray().isEmpty())
                    f = versions.first().toObject().value(QStringLiteral("files")).toArray().first().toObject();
                if (f.isEmpty()) { setStatus(ts("无可用文件: %1").arg(it->title())); return; }
                const QString furl = f.value(QStringLiteral("url")).toString();
                startDownload(it, furl, f.value(QStringLiteral("filename")).toString());
            },
            [this, it](const QString &) { setStatus(ts("获取版本失败: %1").arg(it->title())); });
    } else {
        startDownload(it, it->url(), it->filename());
    }
}

void DownloadCatalog::startDownload(DownloadItem *it, const QString &url, const QString &filename)
{
    if (url.isEmpty()) { setStatus(ts("下载地址为空")); return; }
    const QString id = m_dm->download(url, m_saveDir, filename, it->title());
    if (id.isEmpty()) { setStatus(ts("无法开始下载: %1").arg(filename)); return; }
    it->setTaskId(id);
    it->setDownloading(true);
    it->setPercent(0);
    it->setDone(false);
    it->setErrorText(QString());
    setStatus(ts("开始下载：%1").arg(filename));
}

void DownloadCatalog::cancel(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    DownloadItem *it = m_items.at(index);
    if (!it->taskId().isEmpty())
        m_dm->cancel(it->taskId());
    it->setDownloading(false);
    it->setErrorText(ts("已取消"));
}

void DownloadCatalog::pause(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    DownloadItem *it = m_items.at(index);
    if (!it->taskId().isEmpty())
        m_dm->pause(it->taskId());
}

void DownloadCatalog::resume(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    DownloadItem *it = m_items.at(index);
    if (!it->taskId().isEmpty())
        m_dm->resume(it->taskId());
}

DownloadItem *DownloadCatalog::findByTaskId(const QString &id) const
{
    for (DownloadItem *it : m_items)
        if (it->taskId() == id) return it;
    return nullptr;
}

void DownloadCatalog::onDownloadProgress(const QString &id, qreal percent, qint64, qint64)
{
    DownloadItem *it = findByTaskId(id);
    if (it) it->setPercent(percent);
}

void DownloadCatalog::onDownloadFinished(const QString &id, const QString &path)
{
    DownloadItem *it = findByTaskId(id);
    if (!it) return; // 不属于下载中心的任务（如创建服务器），忽略
    if (!path.isEmpty())
        it->setFilePath(path);
    it->setDownloading(false);
    it->setDone(true);
    it->setPercent(100);
    it->setErrorText(QString());

    // 无安装器的平台（Linux 等）：Java 压缩包下载完成后自动解压并登记为可移植 JDK，实现“下载即安装”
    if (!path.isEmpty() && m_java && it->url().startsWith(QStringLiteral("java://"))) {
        const int feat = it->url().mid(QStringLiteral("java://").length()).toInt();
        const QString ext = QFileInfo(path).suffix().toLower();
        const bool isInstaller = (ext == QStringLiteral("exe") || ext == QStringLiteral("msi"));
        if (!isInstaller) {
            setStatus(ts("Java %1 下载完成，正在解压安装…").arg(feat));
            QPointer<DownloadCatalog> self(this);
            QPointer<DownloadItem> item(it);
            m_java->installManagedArchive(path, feat, [self, item, feat](bool ok, const QString &javaPath) {
                if (!self || !item) return;
                if (ok) {
                    const QString home = QFileInfo(javaPath).absolutePath();
                    item->setFilePath(home);
                    self->setStatus(self->ts("Java %1 已安装为可移植 JDK（%2）").arg(feat).arg(home));
                } else {
                    self->setStatus(self->ts("Java %1 下载完成，但自动解压安装失败；请手动解压：%2")
                                        .arg(feat).arg(item->filePath()));
                }
            });
            return;
        }
    }
    setStatus(ts("下载完成"));
}

void DownloadCatalog::openFile(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    const DownloadItem *it = m_items.at(index);
    QString path = it->filePath();
    if (path.isEmpty() && it->modrinthId().isEmpty() && !it->url().startsWith(QStringLiteral("java://")))
        path = QDir(m_saveDir).filePath(it->filename());
    if (path.isEmpty() || !QFile::exists(path)) {
        setStatus(ts("没有可打开的文件"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void DownloadCatalog::onDownloadError(const QString &id, const QString &message)
{
    DownloadItem *it = findByTaskId(id);
    if (!it) return;
    it->setDownloading(false);
    it->setErrorText(message.isEmpty() ? ts("下载失败") : message);
}
