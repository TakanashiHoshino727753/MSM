/*
 * schedulercontroller.cpp —— 定时启停 / 定时备份（B4）
 */
#include "schedulercontroller.h"
#include "servermanager.h"
#include "servercontroller.h"
#include "backupcontroller.h"
#include <QSettings>
#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QDebug>

SchedulerController::SchedulerController(ServerManager *sm, ServerController *sc,
                                         BackupController *bc, QObject *parent)
    : QObject(parent), m_sm(sm), m_sc(sc), m_bc(bc)
{
    load();
    m_timer = new QTimer(this);
    m_timer->setInterval(30 * 1000);   // 每 30 秒检查一次
    connect(m_timer, &QTimer::timeout, this, [this]() {
        const QTime now = QTime::currentTime();
        const QString today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
        for (const QVariant &v : m_tasks) {
            QVariantMap t = v.toMap();
            if (!t.value(QStringLiteral("enabled"), true).toBool())
                continue;
            const QString timeStr = t.value(QStringLiteral("time")).toString();
            const QTime tt = QTime::fromString(timeStr, QStringLiteral("HH:mm"));
            if (!tt.isValid())
                continue;
            // 落入 [taskTime, taskTime+1min) 窗口即视为到点
            const int diff = now.msecsSinceStartOfDay() - tt.msecsSinceStartOfDay();
            if (diff < 0 || diff > 60 * 1000)
                continue;
            const QString key = dayKey(t.value(QStringLiteral("server")).toString(),
                                       t.value(QStringLiteral("action")).toString());
            if (m_lastFiredDate.value(key) == today)
                continue;
            m_lastFiredDate.insert(key, today);
            fire(t);
        }
    });
    m_timer->start();
}

int SchedulerController::addTask(const QVariantMap &task)
{
    QVariantMap t = task;
    if (!t.contains(QStringLiteral("enabled")))
        t.insert(QStringLiteral("enabled"), true);
    if (!t.contains(QStringLiteral("action")))
        t.insert(QStringLiteral("action"), QStringLiteral("start"));
    m_tasks.append(t);
    save();
    emit tasksChanged();
    return m_tasks.size() - 1;
}

void SchedulerController::removeTask(int index)
{
    if (index < 0 || index >= m_tasks.size())
        return;
    m_tasks.removeAt(index);
    save();
    emit tasksChanged();
}

void SchedulerController::updateTask(int index, const QVariantMap &task)
{
    if (index < 0 || index >= m_tasks.size())
        return;
    m_tasks[index] = task;
    save();
    emit tasksChanged();
}

void SchedulerController::runTask(int index)
{
    if (index < 0 || index >= m_tasks.size())
        return;
    fire(m_tasks.at(index).toMap());
}

void SchedulerController::fire(const QVariantMap &task)
{
    const QString action = task.value(QStringLiteral("action")).toString();
    const QString server = task.value(QStringLiteral("server")).toString();
    if (server.isEmpty() || !m_sc)
        return;
    if (action == QStringLiteral("start")) {
        Server *s = m_sm ? m_sm->serverByName(server) : nullptr;
        const QString path = s ? s->path() : QString();
        m_sc->start(server, path);
    } else if (action == QStringLiteral("stop")) {
        m_sc->stop(server);
    } else if (action == QStringLiteral("backup")) {
        Server *s = m_sm ? m_sm->serverByName(server) : nullptr;
        const QString path = s ? s->path() : QString();
        if (m_bc && !path.isEmpty())
            m_bc->backupNow(server, path);
    }
    emit taskFired(server, action);
}

QString SchedulerController::dayKey(const QString &server, const QString &action) const
{
    return server + QStringLiteral("|") + action;
}

void SchedulerController::load()
{
    QSettings s;
    const QByteArray raw = s.value(QStringLiteral("sched/tasks")).toByteArray();
    if (raw.isEmpty())
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isArray())
        m_tasks = doc.array().toVariantList();
}

void SchedulerController::save()
{
    QSettings s;
    s.setValue(QStringLiteral("sched/tasks"), QJsonDocument(QJsonArray::fromVariantList(m_tasks)).toJson(QJsonDocument::Compact));
}
