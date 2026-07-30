/*
 * backupcontroller.cpp —— 服务端目录定时备份 + 滚动保留（B3）
 * 使用 Windows 自带的 tar.exe 打包，避免引入第三方压缩库。
 */
#include "backupcontroller.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDebug>

BackupController::BackupController(QObject *parent) : QObject(parent)
{
    QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
    m_enabled = s.value(QStringLiteral("backup/enabled"), false).toBool();
    m_intervalHours = s.value(QStringLiteral("backup/intervalHours"), 6).toInt();
    m_retain = s.value(QStringLiteral("backup/retain"), 5).toInt();
    m_onStart = s.value(QStringLiteral("backup/onStart"), false).toBool();
    m_lastBackup = s.value(QStringLiteral("backup/lastBackup"), 0).toLongLong();
    m_lastResult = s.value(QStringLiteral("backup/lastResult"), QString()).toString();
    m_nextAuto = m_lastBackup + m_intervalHours * 3600LL * 1000LL;
    if (m_nextAuto < QDateTime::currentMSecsSinceEpoch())
        m_nextAuto = QDateTime::currentMSecsSinceEpoch();

    m_timer = new QTimer(this);
    m_timer->setInterval(60 * 1000);   // 每分钟检查一次是否到达自动备份时刻
    connect(m_timer, &QTimer::timeout, this, [this]() {
        if (!m_enabled)
            return;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now >= m_nextAuto) {
            m_nextAuto = now + m_intervalHours * 3600LL * 1000LL;
            for (const QVariant &v : m_servers) {
                const QVariantMap m = v.toMap();
                const QString name = m.value(QStringLiteral("name")).toString();
                const QString path = m.value(QStringLiteral("path")).toString();
                if (!name.isEmpty() && !path.isEmpty())
                    doBackup(name, path, true);
            }
        }
    });
    m_timer->start();
}

void BackupController::setEnabled(bool v)
{
    if (v == m_enabled)
        return;
    m_enabled = v;
    QSettings(QStringLiteral("MSM"), QStringLiteral("MSM")).setValue(QStringLiteral("backup/enabled"), v);
    emit enabledChanged();
}

void BackupController::setIntervalHours(int v)
{
    if (v == m_intervalHours)
        return;
    m_intervalHours = v;
    QSettings(QStringLiteral("MSM"), QStringLiteral("MSM")).setValue(QStringLiteral("backup/intervalHours"), v);
    m_nextAuto = QDateTime::currentMSecsSinceEpoch() + v * 3600LL * 1000LL;
    emit intervalHoursChanged();
}

void BackupController::setRetain(int v)
{
    if (v == m_retain)
        return;
    m_retain = v;
    QSettings(QStringLiteral("MSM"), QStringLiteral("MSM")).setValue(QStringLiteral("backup/retain"), v);
    emit retainChanged();
}

void BackupController::setOnStart(bool v)
{
    if (v == m_onStart)
        return;
    m_onStart = v;
    QSettings(QStringLiteral("MSM"), QStringLiteral("MSM")).setValue(QStringLiteral("backup/onStart"), v);
    emit onStartChanged();
}

void BackupController::setServerList(const QVariantList &servers)
{
    m_servers = servers;
}

void BackupController::backupNow(const QString &name, const QString &path)
{
    doBackup(name, path, false);
}

void BackupController::doBackup(const QString &name, const QString &path, bool fromAuto)
{
    if (path.isEmpty() || !QDir(path).exists()) {
        emit backupFinished(name, false, QStringLiteral("服务端路径不存在"), QString());
        return;
    }
    QDir().mkpath(backupDir());
    const QString dest = backupDir() + QStringLiteral("/") + name + QStringLiteral("-") + stamp() + QStringLiteral(".zip");
    const QString srcDir = QDir(path).absolutePath();
    // tar.exe（Windows 10+ 自带）：将 srcDir 下全部内容打包为 zip
    auto *proc = new QProcess(this);
    m_running.insert(name, proc);
    connect(proc, &QProcess::finished, this, [this, name, dest, fromAuto](int code, QProcess::ExitStatus) {
        auto it = m_running.find(name);
        QProcess *p = (it != m_running.end()) ? it.value() : nullptr;
        const bool ok = (code == 0) && QFile::exists(dest);
        QString msg;
        if (ok) {
            const qint64 sz = QFileInfo(dest).size();
            msg = QStringLiteral("已备份（%1）").arg(sz > 1048576
                    ? QString::number(sz / 1048576.0, 'f', 1) + QStringLiteral(" MB")
                    : QString::number(sz / 1024) + QStringLiteral(" KB"));
            m_lastResult = QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("MM-dd HH:mm"))).arg(msg);
            m_lastBackup = QDateTime::currentMSecsSinceEpoch();
            QSettings(QStringLiteral("MSM"), QStringLiteral("MSM")).setValue(QStringLiteral("backup/lastBackup"), m_lastBackup);
            QSettings(QStringLiteral("MSM"), QStringLiteral("MSM")).setValue(QStringLiteral("backup/lastResult"), m_lastResult);
            emit lastResultChanged();
            emit lastBackupChanged();
            applyRetention(name);
        } else {
            msg = QStringLiteral("备份失败（tar 退出码 %1）").arg(code);
            m_lastError = msg;
            emit lastErrorChanged();
        }
        if (p) {
            p->deleteLater();
            m_running.remove(name);
        }
        emit backupFinished(name, ok, msg, ok ? dest : QString());
    });
    proc->start(QStringLiteral("tar"),
                QStringList() << QStringLiteral("-a") << QStringLiteral("-cf") << dest
                              << QStringLiteral("-C") << srcDir << QStringLiteral("."));
    if (!proc->waitForStarted(5000)) {
        emit backupFinished(name, false, QStringLiteral("无法启动 tar.exe（请确认系统为 Windows 10+）"), QString());
        proc->deleteLater();
        m_running.remove(name);
    }
}

void BackupController::applyRetention(const QString &name)
{
    QDir dir(backupDir());
    QStringList files = dir.entryList(QStringList() << (name + QStringLiteral("-*.zip")),
                                      QDir::Files, QDir::Name);
    // entryList 按名称升序（时间戳在文件名中保证旧->新），保留末尾 m_retain 份
    while (files.size() > m_retain) {
        const QString old = files.takeFirst();
        QFile::remove(dir.absoluteFilePath(old));
    }
}

QString BackupController::backupDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/backups");
}

QString BackupController::stamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
}
