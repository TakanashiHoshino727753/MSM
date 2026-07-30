#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QProcess>
#include <QTimer>
#include <QMap>
#include <QDateTime>

// 定时备份 + 滚动保留（B3）
// 通过 Windows 自带 tar.exe 将服务端目录打包为 zip，存入集中备份目录，
// 并按保留份数滚动删除最旧备份。支持手动“立即备份”与按小时间隔的自动备份。
class BackupController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int intervalHours READ intervalHours WRITE setIntervalHours NOTIFY intervalHoursChanged)
    Q_PROPERTY(int retain READ retain WRITE setRetain NOTIFY retainChanged)
    Q_PROPERTY(bool onStart READ onStart WRITE setOnStart NOTIFY onStartChanged)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY lastResultChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(qint64 lastBackup READ lastBackup NOTIFY lastBackupChanged)
public:
    explicit BackupController(QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool v);
    int intervalHours() const { return m_intervalHours; }
    void setIntervalHours(int v);
    int retain() const { return m_retain; }
    void setRetain(int v);
    bool onStart() const { return m_onStart; }
    void setOnStart(bool v);
    QString lastResult() const { return m_lastResult; }
    QString lastError() const { return m_lastError; }
    qint64 lastBackup() const { return m_lastBackup; }

    // 由上层（main.cpp）在服务器列表变化时调用，供自动备份遍历
    Q_INVOKABLE void setServerList(const QVariantList &servers);

    // 立即备份单台服务器（path 为服务端根目录）
    Q_INVOKABLE void backupNow(const QString &name, const QString &path);

signals:
    void enabledChanged();
    void intervalHoursChanged();
    void retainChanged();
    void onStartChanged();
    void lastResultChanged();
    void lastErrorChanged();
    void lastBackupChanged();
    // 单次备份完成：ok 是否成功，message 为结果或错误，dest 为备份文件路径
    void backupFinished(const QString &name, bool ok, const QString &message, const QString &dest);

private:
    void doBackup(const QString &name, const QString &path, bool fromAuto);
    // 备份完成后滚动删除最旧备份，仅保留 m_retain 份
    void applyRetention(const QString &name);
    static QString backupDir();
    static QString stamp();

    QVariantList m_servers;
    QMap<QString, QProcess *> m_running;

    bool m_enabled = false;
    int m_intervalHours = 6;
    int m_retain = 5;
    bool m_onStart = false;
    QString m_lastResult;
    QString m_lastError;
    qint64 m_lastBackup = 0;
    qint64 m_nextAuto = 0;
    QTimer *m_timer = nullptr;
};
