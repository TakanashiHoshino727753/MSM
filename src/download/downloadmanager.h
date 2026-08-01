#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QSaveFile>
#include <QUrl>
#include <QTimer>
#include <QDir>
#include <QHash>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>
#include "downloadlistmodel.h"

// 通用下载管理器：每个任务独立、可并行下载；通过 downloadList 暴露全局下载列表。
// 任务ID为短UUID；下载进度通过 progress 信号与 downloadList 模型双向反映。
// 支持：暂停（保留断点）/ 继续（HTTP Range 断点续传）/ 取消 / 取消后重新下载。
class DownloadManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(DownloadListModel *downloadList READ downloadList CONSTANT)
public:
    explicit DownloadManager(QObject *parent = nullptr) : QObject(parent) {
        m_nam = new QNetworkAccessManager(this);
        m_list = new DownloadListModel(this);
        connect(m_list, &DownloadListModel::cancelRequested, this, &DownloadManager::cancel);
        connect(m_list, &DownloadListModel::pauseRequested, this, &DownloadManager::pause);
        connect(m_list, &DownloadListModel::resumeRequested, this, &DownloadManager::resume);
        connect(m_list, &DownloadListModel::restartRequested, this, &DownloadManager::restart);
    }
    DownloadListModel *downloadList() const { return m_list; }

    // 下载中心默认保存目录：系统“下载”文件夹下的 MSM 子目录（如 %USERPROFILE%\Downloads\MSM）。
    // 用户可在界面更改保存路径；若曾持久化过自定义路径（QSettings path/downloadDir）则优先采用。
    // Linux：直接固定为 ~/MSM/Downloads，避开 QStandardPaths 在中文 locale 下解析到“下载”等
    // 含中文的目录导致的问题（且某些环境返回空/根目录无权写入）。
    static QString defaultDownloadDir() {
#ifdef Q_OS_LINUX
        QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
        const QString custom = s.value(QStringLiteral("path/downloadDir")).toString();
        if (!custom.isEmpty())
            return custom;
        return QDir::cleanPath(QDir::homePath() + QStringLiteral("/MSM/Downloads"));
#else
        QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
        const QString custom = s.value(QStringLiteral("path/downloadDir")).toString();
        if (!custom.isEmpty())
            return custom;
        return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + QStringLiteral("/MSM");
#endif
    }

    // 创建服务器默认保存目录：系统“文档”文件夹下的 MSM 子目录（如 %USERPROFILE%\Documents\MSM）。
    // 用户可在界面更改保存路径；若曾持久化过自定义路径（QSettings path/serverDir）则优先采用。
    // Linux：直接固定为 ~/MSM/Servers，避开 QStandardPaths 在中文 locale 下解析到“文档”等
    // 含中文的目录导致的问题（且某些环境返回空/根目录无权写入）。
    static QString defaultServerDir() {
#ifdef Q_OS_LINUX
        QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
        const QString custom = s.value(QStringLiteral("path/serverDir")).toString();
        if (!custom.isEmpty())
            return custom;
        return QDir::cleanPath(QDir::homePath() + QStringLiteral("/MSM/Servers"));
#else
        QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
        const QString custom = s.value(QStringLiteral("path/serverDir")).toString();
        if (!custom.isEmpty())
            return custom;
        return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/MSM");
#endif
    }

    // 兼容旧调用：泛化的“默认目录”指向下载目录。
    static QString defaultDir() { return defaultDownloadDir(); }

    // 确保目录存在（供创建服务器等场景复用）。返回是否成功创建/已存在。
    Q_INVOKABLE static bool ensureDir(const QString &dir)
    {
        QDir d;
        if (d.exists(dir))
            return true;
        return d.mkpath(dir);
    }

    // 写入文本文件（供创建服务器时写 eula.txt、java.txt 等）。返回是否成功。
    Q_INVOKABLE static bool writeTextFile(const QString &path, const QString &content)
    {
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        QByteArray data = content.toUtf8();
        if (f.write(data) != data.size())
            return false;
        return f.commit();
    }

    // 开始下载（并行）。title 用于下载列表展示，缺省为文件名。
    Q_INVOKABLE QString download(const QString &url, const QString &dir, const QString &filename,
                                 const QString &title = QString())
    {
        ensureDir(dir);
        const QString name = filename.isEmpty()
            ? QUrl(url).fileName()
            : filename;
        const QString filePath = QDir(dir).filePath(name);
        const QString id = QString::number(nextSeq()) + '-' + name;

        if (m_list)
            m_list->add(id, title.isEmpty() ? name : title, filePath, url);

        startTask(id, url, filePath, title.isEmpty() ? name : title, 0);
        emit started(id, filePath);
        return id;
    }

    // 取消（删掉未完成的残留文件）
    Q_INVOKABLE void cancel(const QString &id)
    {
        auto it = m_tasks.find(id);
        if (it == m_tasks.end()) {
            if (m_list) m_list->setState(id, DownloadListModel::StCanceled, QStringLiteral("已取消"));
            return;
        }
        Task t = it.value();          // 复制（含指针），后续用副本清理
        m_tasks.erase(it);            // 先移除，避免 abort 同步触发 finished 时二次 erase
        if (t.reply) {
            t.reply->abort();
            t.reply->deleteLater();
        }
        if (t.file) {
            if (t.file->isOpen())
                t.file->close();
            t.file->remove();         // 删除未完成的残留文件
            delete t.file;
        }
        emit error(id, QStringLiteral("已取消"));
        if (m_list) m_list->setState(id, DownloadListModel::StCanceled, QStringLiteral("已取消"));
    }

    // 暂停：中断当前传输，保留磁盘上的断点（不删除文件），状态置为 paused。
    Q_INVOKABLE void pause(const QString &id)
    {
        auto it = m_tasks.find(id);
        if (it == m_tasks.end())
            return;
        Task &t = it.value();
        if (!t.reply)
            return;
        t.paused = true;
        t.reply->abort();
        t.reply->deleteLater();
        t.reply = nullptr;
        if (t.file)
            t.file->flush();
        if (m_list)
            m_list->setState(id, DownloadListModel::StPaused);
    }

    // 继续：从磁盘断点处用 HTTP Range 续传。
    Q_INVOKABLE void resume(const QString &id)
    {
        auto it = m_tasks.find(id);
        if (it == m_tasks.end())
            return;
        Task &t = it.value();
        if (!t.paused)
            return;
        qint64 offset = t.file ? t.file->size() : 0;
        t.paused = false;
        t.total = -1;
        if (!t.file)
            t.file = new QFile(t.path);
        beginDownload(id, QUrl(t.url), offset);
        if (m_list)
            m_list->setState(id, DownloadListModel::StDownloading);
    }

    // 重新下载（取消/出错后）：从模型行找回源地址，复用同一行，从头重新下载。
    Q_INVOKABLE void restart(const QString &id)
    {
        // 非本管理器管理的任务（如 JavaManager 创建的下载，其 url 不在此登记）交由对应处理器处理，
        // 这里仅处理本管理器的任务；否则会因为读不到 url 而误报“缺少下载地址”。
        if (!m_tasks.contains(id)) {
            const QString u = m_list ? m_list->urlForId(id) : QString();
            if (u.isEmpty())
                return;
        }
        QString url, path, title;
        if (m_list) {
            url = m_list->urlForId(id);
            path = m_list->pathForId(id);
            title = m_list->titleForId(id);
        }
        if (url.isEmpty()) {
            if (m_list) m_list->setState(id, DownloadListModel::StError,
                                         QStringLiteral("无法重试：缺少下载地址"));
            return;
        }
        startTask(id, url, path, title, 0);
        emit started(id, path);
    }

    Q_INVOKABLE bool isActive(const QString &id) const
    {
        return m_tasks.contains(id);
    }

signals:
    void started(const QString &id, const QString &path);
    void progress(const QString &id, qreal percent, qint64 received, qint64 total);
    void finished(const QString &id, const QString &path);
    void error(const QString &id, const QString &msg);

private:
    struct Task {
        QNetworkReply *reply = nullptr;
        QFile *file = nullptr;             // 直接写入目标路径（支持断点续传追加）
        QString path;
        QString url;                       // 当前（已重定向解析后）下载地址，用于续传
        qint64 offset = 0;                 // 已在磁盘上的字节数（续传起点）
        qint64 total = -1;                 // 已知总大小（-1 未知）
        int redirects = 0;
        bool paused = false;
    };

    // 清理并移除任务（保留模型行）。removeFile=true 时删除磁盘残留文件。
    void dropTask(const QString &id, bool removeFile)
    {
        auto it = m_tasks.find(id);
        if (it == m_tasks.end())
            return;
        Task t = it.value();          // 复制副本，后续用副本清理
        m_tasks.erase(it);            // 先移除：abort() 可能同步触发 finished 造成重入二次 erase，
                                      //          否则回到此处 Task& 会悬垂 -> SIGSEGV
        if (t.reply) {
            t.reply->abort();
            t.reply->deleteLater();
        }
        if (t.file) {
            if (t.file->isOpen())
                t.file->close();
            if (removeFile)
                t.file->remove();
            delete t.file;
        }
    }

    // 启动 / 继续 / 重启某个 id 的下载（offset>0 为断点续传起点）
    void startTask(const QString &id, const QString &url, const QString &path,
                   const QString &title, qint64 offset)
    {
        // 若已有同名任务（如重启），先清理旧 Task
        auto it = m_tasks.find(id);
        if (it != m_tasks.end()) {
            Task old = it.value();    // 复制副本
            m_tasks.erase(it);        // 先移除，避免 abort() 同步触发 finished 重入导致悬垂引用
            if (old.reply) { old.reply->abort(); old.reply->deleteLater(); }
            if (old.file) { if (old.file->isOpen()) old.file->close(); delete old.file; }
        }

        Task t;
        t.path = path;
        t.url = url;
        t.offset = offset;
        t.total = -1;
        t.redirects = 0;
        t.paused = false;
        t.file = new QFile(path);
        m_tasks.insert(id, t);

        if (m_list) {
            m_list->setState(id, DownloadListModel::StDownloading);
            m_list->setProgress(id, 0);
        }
        beginDownload(id, QUrl(url), offset);
    }

    void beginDownload(const QString &id, const QUrl &url, qint64 offset)
    {
        auto it = m_tasks.find(id);
        if (it == m_tasks.end())
            return;
        Task &t = it.value();
        t.url = url.toString();

        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(90000);   // 文件下载：90s 无数据传输则报错，避免无限卡死
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MinecraftServerManager/1.0"));
        if (offset > 0)
            req.setRawHeader("Range", "bytes=" + QByteArray::number(offset) + "-");

        QNetworkReply *reply = m_nam->get(req);
        // 总超时兜底：连接建立阶段卡死（被墙域名 SYN 黑洞）时强制结束，避免无限“连接中”。
        // 关键：计时器到时【直接 emit error + 清理任务】，不依赖 abort() 后 finished 一定触发——
        // 否则在 Qt6+MinGW 下连接建立阶段 abort() 偶尔不发出 finished，会造成下载永久挂起、
        // 上层（如模组服）始终停留在“正在解析下载地址”。dropTask 会移除任务，
        // 即便之后 finished 晚到，onFinished 也会因找不到任务而安全忽略，不会二次报错。
        QTimer *guard = new QTimer(reply);
        guard->setSingleShot(true);
        guard->setInterval(90000);
        QObject::connect(guard, &QTimer::timeout, reply, [this, reply, id, urlStr = url.toString()]() {
            if (!reply->isRunning())
                return;
            qDebug() << "[DL] TIMEOUT id=" << id << "url=" << urlStr;
            emit error(id, QStringLiteral("下载超时（90s 无响应，可能源站不可达）"));
            dropTask(id, false);
        });
        guard->start();
        qDebug() << "[DL] beginDownload id=" << id << "url=" << req.url().toString();
        t.reply = reply;
        t.offset = offset;

        // 打开文件：offset==0 截断重写；否则定位到断点续写
        bool opened = false;
        if (offset == 0) {
            if (!t.file->isOpen())
                opened = t.file->open(QIODevice::WriteOnly | QIODevice::Truncate);
            else {                          // 续传失败重来：清空已有内容
                t.file->seek(0);
                t.file->resize(0);
                opened = true;
            }
        } else {
            if (!t.file->isOpen())
                opened = t.file->open(QIODevice::ReadWrite);
            if (opened)
                opened = t.file->seek(offset);
        }
        if (!opened) {
            if (m_list) m_list->setState(id, DownloadListModel::StError, t.file->errorString());
            emit error(id, t.file->errorString());
            dropTask(id, false);
            return;
        }

        connect(reply, &QNetworkReply::downloadProgress, this, [this, id](qint64 recv, qint64 total) {
            auto it2 = m_tasks.find(id);
            if (it2 == m_tasks.end())
                return;
            Task &tk = it2.value();
            // 优先从 Content-Range 取总大小（206 续传），否则用信号给的 total
            qint64 full = -1;
            if (tk.reply && tk.reply->hasRawHeader("Content-Range")) {
                QByteArray cr = tk.reply->rawHeader("Content-Range");
                int slash = cr.lastIndexOf('/');
                if (slash >= 0)
                    full = cr.mid(slash + 1).toLongLong();
            } else if (total > 0) {
                full = total;
            }
            if (full > 0)
                tk.total = full;
            qint64 cur = tk.offset + recv;
            if (tk.total > 0) {
                const qreal percent = qMin(100.0, cur * 100.0 / tk.total);
                emit progress(id, percent, cur, tk.total);
                if (m_list) m_list->setProgress(id, percent);
            }
        });
        connect(reply, &QNetworkReply::finished, this, [this, id]() { onFinished(id); });
    }

    void onFinished(const QString &id)
    {
        qDebug() << "[DL] onFinished id=" << id;
        auto it = m_tasks.find(id);
        if (it == m_tasks.end()) {
            qDebug() << "[DL] onFinished ignored (task gone) id=" << id;
            return;                         // 已被 cancel 移除 / 暂停，忽略
        }
        Task &t = it.value();

        if (t.paused)
            return;                         // 暂停导致的中断，忽略

        const QVariant redir = t.reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        const bool noErr = t.reply->error() == QNetworkReply::NoError;
        const bool isRedir = !redir.isNull();

        if (isRedir && t.redirects < 5 && noErr) {
            const QUrl next = t.reply->url().resolved(redir.toUrl());
            t.redirects++;
            t.reply->deleteLater();
            beginDownload(id, next, t.offset);  // 同文件、断点不变地跟随重定向
            return;
        }

        const bool isPartial = t.reply->hasRawHeader("Content-Range");
        // 请求了续传但服务器未支持（返回全量 200）：从头重新全量下载
        if (t.offset > 0 && !isPartial) {
            t.reply->deleteLater();
            beginDownload(id, QUrl(t.url), 0);
            return;
        }

        if (isRedir) {                     // 重定向次数过多
            if (m_list) m_list->setState(id, DownloadListModel::StError, QStringLiteral("重定向次数过多"));
            emit error(id, QStringLiteral("重定向次数过多"));
            dropTask(id, false);
            return;
        }
        if (!noErr) {
            if (m_list) m_list->setState(id, DownloadListModel::StError, t.reply->errorString());
            emit error(id, t.reply->errorString());
            dropTask(id, false);
            return;
        }

        const QByteArray data = t.reply->readAll();
        if (!t.file->isOpen())
            t.file->open(QIODevice::ReadWrite);
        if (t.file->write(data) != data.size()) {
            if (m_list) m_list->setState(id, DownloadListModel::StError, t.file->errorString());
            emit error(id, t.file->errorString());
            dropTask(id, false);
            return;
        }
        t.file->flush();
        t.file->close();
        delete t.file;
        t.file = nullptr;
        t.reply->deleteLater();
        const QString path = t.path;   // 复制到局部：erase 后 t 悬垂，emit 不能再引用 t.path
        m_tasks.erase(it);             // 先移除任务，再对外通知（emit 的槽可能重入本类）
        if (m_list) m_list->setState(id, DownloadListModel::StDone);
        emit finished(id, path);
    }

    QNetworkAccessManager *m_nam;
    QHash<QString, Task> m_tasks;
    DownloadListModel *m_list = nullptr;

    static int nextSeq()
    {
        static int s = 0;
        return ++s;
    }
};
