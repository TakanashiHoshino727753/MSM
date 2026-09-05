/*
 * modpackimporter.cpp —— 整合包导入逻辑
 * -------------------------------------------------
 * 选择 .zip 整合包 -> 解压 -> 识别类型(CurseForge/Modrinth) -> 解析加载器与
 * 游戏版本 -> 拉取服务端核心 -> 写 eula -> 加入服务器列表。全异步，状态对外暴露。
 */
#include "modpackimporter.h"
#include "downloadmanager.h"
#include "servermanager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>

ModpackImporter::ModpackImporter(DownloadManager *dm, ServerManager *sm, QObject *parent)
    : QObject(parent), m_dm(dm), m_sm(sm)
{
    if (m_dm) {
        connect(m_dm, &DownloadManager::progress, this, &ModpackImporter::onDownloadProgress);
        connect(m_dm, &DownloadManager::finished, this, &ModpackImporter::onDownloadFinished);
        connect(m_dm, &DownloadManager::error, this, &ModpackImporter::onDownloadError);
    }
}

void ModpackImporter::setStatus(const QString &s)
{
    if (m_status != s) { m_status = s; emit statusTextChanged(); }
}
void ModpackImporter::setBusy(bool b)
{
    if (m_busy != b) { m_busy = b; emit busyChanged(); }
}
void ModpackImporter::setDone(bool d)
{
    if (m_done != d) { m_done = d; emit doneChanged(); }
}
void ModpackImporter::setProgress(qreal p)
{
    if (m_progress != p) { m_progress = p; emit progressChanged(); }
}

void ModpackImporter::setZipPath(const QString &v)
{
    if (m_zipPath != v) { m_zipPath = v; emit zipPathChanged(); }
}
void ModpackImporter::setTargetDir(const QString &v)
{
    // 规范化：Windows 上可能以反斜杠 / file:// 前缀 / 多余斜杠传入，统一成正斜杠绝对路径
    QString norm = v.trimmed();
    norm = QDir::fromNativeSeparators(norm);
    if (norm.startsWith(QStringLiteral("file:///")))
        norm = norm.mid(8);
    else if (norm.startsWith(QStringLiteral("file://")))
        norm = norm.mid(7);
    norm = QDir::cleanPath(norm);
    if (m_targetDir != norm) { m_targetDir = norm; emit targetDirChanged(); }
}

void ModpackImporter::reset()
{
    m_zipPath.clear();
    m_targetDir.clear();
    m_name.clear();
    m_type.clear();
    m_loader.clear();
    m_gameVersion.clear();
    m_taskId.clear();
    m_busy = false;
    m_done = false;
    m_progress = 0;
    m_status.clear();
    emit zipPathChanged();
    emit targetDirChanged();
    emit modpackNameChanged();
    emit detectedTypeChanged();
    emit loaderChanged();
    emit gameVersionChanged();
    emit busyChanged();
    emit doneChanged();
    emit progressChanged();
    emit statusTextChanged();
}

QString ModpackImporter::findFile(const QString &dir, const QString &fileName) const
{
    // 在 dir 及其下一级子目录中寻找 fileName
    QDir d(dir);
    if (d.exists(fileName))
        return d.filePath(fileName);
    for (const QString &sub : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDir sd(d.filePath(sub));
        if (sd.exists(fileName))
            return sd.filePath(fileName);
    }
    return QString();
}

void ModpackImporter::import()
{
    if (m_busy || m_done)
        return;
    if (m_zipPath.isEmpty() || !QFile::exists(m_zipPath)) {
        setStatus(QStringLiteral("请选择有效的整合包 .zip 文件"));
        return;
    }

    // 名称：默认取 zip 文件名；用户可在后续步骤看到
    const QString base = QFileInfo(m_zipPath).completeBaseName()
                             .replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    m_name = base;

    const QString dir = m_targetDir.isEmpty()
                            ? QDir::cleanPath(QDir::fromNativeSeparators(m_dm ? m_dm->defaultServerDir()
                                                : (QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                                   + QStringLiteral("/MSM")))
                                                + QStringLiteral("/") + m_name)
                            : m_targetDir;
    m_targetDir = dir;

    if (!m_dm->ensureDir(dir)) {
        setStatus(QStringLiteral("无法创建目录：") + dir);
        return;
    }

    setBusy(true);
    setDone(false);
    setProgress(0);
    setStatus(QStringLiteral("正在解压整合包…"));

    // Windows：使用 PowerShell 的 Expand-Archive 解压
    m_proc = new QProcess(this);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ModpackImporter::onExtractFinished);
    // 通过 $args 传参，避免把路径拼进 PowerShell 命令字符串（路径含单引号会导致命令注入）
    const QString script = QStringLiteral("Expand-Archive -Force -LiteralPath $args[0] -DestinationPath $args[1]");
    m_proc->start(QStringLiteral("powershell"),
                  QStringList() << QStringLiteral("-NoProfile") << QStringLiteral("-Command") << script << m_zipPath << dir);
    if (!m_proc->waitForStarted(5000)) {
        setStatus(QStringLiteral("无法启动解压进程（请确认系统支持 PowerShell）"));
        setBusy(false);
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}

void ModpackImporter::onExtractFinished(int exitCode, QProcess::ExitStatus status)
{
    QString err;
    if (m_proc) {
        err = QString::fromLocal8Bit(m_proc->readAllStandardError());
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    if (status != QProcess::NormalExit || exitCode != 0) {
        setBusy(false);
        setStatus(QStringLiteral("解压失败：") + (err.isEmpty() ? QStringLiteral("未知错误") : err.left(200)));
        return;
    }

    // CurseForge 解压后常把文件放在 overrides/ 下，将其内容提到目标根目录
    QDir od(m_targetDir + QStringLiteral("/overrides"));
    if (od.exists()) {
        QDir(m_targetDir).rename(od.path(), m_targetDir + QStringLiteral("/__ov_tmp"));
        QDir tmp(m_targetDir + QStringLiteral("/__ov_tmp"));
        for (const QString &f : tmp.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            QDir(m_targetDir).rename(tmp.filePath(f), m_targetDir + "/" + f);
        }
        tmp.rmdir(m_targetDir + QStringLiteral("/__ov_tmp"));
    }

    parseManifest(m_targetDir);
}

void ModpackImporter::parseManifest(const QString &dir)
{
    const QString cf = findFile(dir, QStringLiteral("manifest.json"));
    const QString mr = findFile(dir, QStringLiteral("modrinth.index.json"));

    if (!cf.isEmpty()) {
        m_type = QStringLiteral("curseforge");
        QFile f(cf);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
            m_gameVersion = o.value(QStringLiteral("minecraft")).toObject().value(QStringLiteral("version")).toString();
            const QJsonArray loaders = o.value(QStringLiteral("minecraft")).toObject().value(QStringLiteral("modLoaders")).toArray();
            if (!loaders.isEmpty()) {
                const QString id = loaders.first().toObject().value(QStringLiteral("id")).toString(); // 形如 forge-47.1.47
                m_loader = id.section(QLatin1Char('-'), 0, 0);
                m_loaderVersion = id.section(QLatin1Char('-'), 1);
            }
            if (o.contains(QStringLiteral("name")))
                m_name = o.value(QStringLiteral("name")).toString();
        }
    } else if (!mr.isEmpty()) {
        m_type = QStringLiteral("modrinth");
        QFile f(mr);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
            m_gameVersion = o.value(QStringLiteral("game")).toObject().value(QStringLiteral("version")).toString();
            for (const auto &dep : o.value(QStringLiteral("dependencies")).toArray()) {
                const QJsonObject d = dep.toObject();
                const QString id = d.value(QStringLiteral("id")).toString();
                if (id != QStringLiteral("minecraft")) {
                    m_loader = id;                 // fabric / forge / quilt / neoforge
                    m_loaderVersion = d.value(QStringLiteral("version")).toString();
                    break;
                }
            }
            if (o.contains(QStringLiteral("name")))
                m_name = o.value(QStringLiteral("name")).toString();
        }
    } else {
        m_type = QStringLiteral("unknown");
        setStatus(QStringLiteral("未能识别整合包类型（缺少 manifest.json / modrinth.index.json）"));
    }

    emit detectedTypeChanged();
    emit loaderChanged();
    emit gameVersionChanged();
    emit modpackNameChanged();

    downloadCore();
}

void ModpackImporter::downloadCore()
{
    QString url;
    QString filename = QStringLiteral("server.jar");
    if (m_loader == QStringLiteral("forge") && !m_loaderVersion.isEmpty() && !m_gameVersion.isEmpty()) {
        // 兼容不同平台/作者填写的版本号写法：
        //  纯构建号 "47.4.10"           -> 拼上 MC 版本
        //  带 MC 前缀 "1.20.1-47.4.10"   -> 直接使用
        //  带 loader 前缀 "forge-47.4.10" -> 去前缀后判定
        QString v = m_loaderVersion;
        if (v.startsWith(QStringLiteral("forge-"))) v = v.mid(6);
        if (!v.startsWith(m_gameVersion + QStringLiteral("-")))
            v = m_gameVersion + QStringLiteral("-") + v;
        url = QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/") + v +
              QStringLiteral("/forge-") + v + QStringLiteral("-installer.jar");
    } else if (m_loader == QStringLiteral("fabric") && !m_gameVersion.isEmpty()) {
        // Fabric 无直接可运行核心，下载官方 installer（fabric-server-launcher 已废弃）
        url = QStringLiteral("https://maven.fabricmc.net/net/fabricmc/fabric-installer/1.1.1/fabric-installer-1.1.1.jar");
    } else if (m_loader == QStringLiteral("neoforge") && !m_loaderVersion.isEmpty()) {
        // 兼容 "1.21.1-21.1.242" 这类带 MC 前缀的写法；保留 -beta/-alpha 后缀
        QString v = m_loaderVersion;
        if (v.startsWith(QStringLiteral("neoforge-"))) v = v.mid(9);
        const int idx = v.indexOf(QLatin1Char('-'));
        if (idx > 0 && v.left(idx).startsWith(QStringLiteral("1.")))
            v = v.mid(idx + 1);
        url = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/") + v +
              QStringLiteral("/neoforge-") + v + QStringLiteral("-installer.jar");
    } else {
        // 无法自动拉取核心（如 quilt 或未知类型）：直接生成实例，由用户手动补全核心
        finishImport(true);
        return;
    }

    setStatus(QStringLiteral("正在下载服务端核心（") + m_loader + QStringLiteral("）…"));
    const QString id = m_dm->download(url, m_targetDir, filename);
    if (id.isEmpty()) {
        setBusy(false);
        setStatus(QStringLiteral("无法开始下载服务端核心"));
        return;
    }
    m_taskId = id;
}

void ModpackImporter::finishImport(bool coreOk)
{
    // 写入 EULA 并加入服务器列表
    m_dm->writeTextFile(m_targetDir + QStringLiteral("/eula.txt"),
                        QStringLiteral("eula=true\n# 由 MSM 导入整合包时自动生成（已同意 Minecraft EULA）\n"));
    if (m_sm)
        m_sm->addServer(m_name, m_gameVersion, m_loader, m_targetDir);

    setBusy(false);
    setDone(true);
    setStatus(coreOk
              ? QStringLiteral("导入完成，已加入服务器列表（") + m_loader + QStringLiteral(" / ") + m_gameVersion + QStringLiteral("）")
              : QStringLiteral("已加入服务器列表；服务端核心需手动放置（当前加载器：") + (m_loader.isEmpty() ? QStringLiteral("未知") : m_loader) + QStringLiteral("）"));
}

void ModpackImporter::onDownloadProgress(const QString &id, qreal percent, qint64, qint64)
{
    if (id == m_taskId)
        setProgress(percent);
}

void ModpackImporter::onDownloadFinished(const QString &id, const QString &)
{
    if (id != m_taskId)
        return;
    finishImport(true);
}

void ModpackImporter::onDownloadError(const QString &id, const QString &message)
{
    if (id != m_taskId)
        return;
    // 核心下载失败也尽量保留实例（用户可手动补全）
    finishImport(false);
    setStatus(QStringLiteral("服务端核心下载失败：") + (message.isEmpty() ? QStringLiteral("未知错误") : message));
}
