#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QMap>

class ServerManager;
class ServerController;
class BackupController;

// 定时启停 / 定时备份（B4）
// 维护一组计划任务：每台受管服务器可配置“在 HH:MM 执行 启动/停止/备份”。
// 每分钟检查一次当前时间是否落入任务时刻窗口，且当天尚未执行，则触发对应动作。
class SchedulerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList tasks READ tasks NOTIFY tasksChanged)
public:
    explicit SchedulerController(ServerManager *sm, ServerController *sc,
                                 BackupController *bc, QObject *parent = nullptr);

    QVariantList tasks() const { return m_tasks; }

    // 新增任务，返回其索引；task 需含 action/server/time（enabled 默认 true）
    Q_INVOKABLE int addTask(const QVariantMap &task);
    // 删除指定索引的任务
    Q_INVOKABLE void removeTask(int index);
    // 更新指定索引的任务
    Q_INVOKABLE void updateTask(int index, const QVariantMap &task);
    // 立即执行某索引任务（手动触发测试）
    Q_INVOKABLE void runTask(int index);

signals:
    void tasksChanged();
    void taskFired(const QString &server, const QString &action);

private:
    void load();
    void save();
    void fire(const QVariantMap &task);
    // 任务 key：server + action，用于“当天是否已执行”去重
    QString dayKey(const QString &server, const QString &action) const;

    ServerManager *m_sm = nullptr;
    ServerController *m_sc = nullptr;
    BackupController *m_bc = nullptr;
    QVariantList m_tasks;
    QTimer *m_timer = nullptr;
    QMap<QString, QString> m_lastFiredDate;   // dayKey -> yyyy-MM-dd
};
