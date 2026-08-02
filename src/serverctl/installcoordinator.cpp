#include "installcoordinator.h"
#include "createservercontroller.h"
#include "../download/downloadmanager.h"
#include "../server/servermanager.h"
#include "../java/javamanager.h"

InstallCoordinator::InstallCoordinator(DownloadManager *dm, ServerManager *sm,
                                       JavaManager *java, QObject *parent)
    : QObject(parent), m_dm(dm), m_sm(sm), m_java(java)
{
}

InstallCoordinator::~InstallCoordinator()
{
    // 释放所有尚未清理的任务（controller 随 InstallTask 一起析构）。
    qDeleteAll(m_tasks);
    m_tasks.clear();
    m_order.clear();
}

QString InstallCoordinator::installServer(const QString &type,
                                           const QString &version,
                                           const QStringList &loaders,
                                           const QString &label,
                                           bool addToList)
{
    if (type.isEmpty() || version.isEmpty()) {
        emit toast(tr("安装失败"), tr("类型和版本不能为空"));
        return QString();
    }

    auto *t = new InstallTask();
    t->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t->type = type;
    t->version = version;
    t->loaders = loaders;
    t->label = label;
    t->addToList = addToList;
    t->status = tr("排队中");

    auto *ctl = new CreateServerController(m_dm, m_sm, m_java, this);
    t->controller = ctl;
    m_tasks.insert(t->id, t);
    m_order.append(t->id);
    m_focusId = t->id;          // 焦点切到最新任务（投影属性指向它）

    forwardSignals(t);

    // 配置 controller：类型 + 版本 + EULA + 加载器 + 是否进列表。
    ctl->setCurrentType(type);
    ctl->setCurrentVersion(version);
    ctl->setEulaAccepted(true);
    ctl->setSkipAddList(!addToList);
    if (!loaders.isEmpty())
        ctl->setSelectedLoaders(loaders);

    // 桌面端/机器人入口通过本协调器生成“下载中心”模组服：必须打包成压缩包，并显式把
    // 产物目录钉在“下载目录”（如 Downloads\MSM），否则 CreateServerController 会回退到
    // defaultServerDir()（Documents\MSM）。与 WebUI 的 packager 路径行为保持一致：
    // 先 setPackaged，再 setSaveDir(完整目标目录)，最后才 setName（避免 refreshSaveDir
    // 在 m_saveDir 仍为空时回退到 Documents）。
    ctl->setPackaged(true);
    if (m_dm) {
        const QString parent = QDir::fromNativeSeparators(m_dm->defaultDownloadDir());
        const QString folder = QStringLiteral("Mod-") + version + QStringLiteral("-")
                               + loaders.join(QStringLiteral("-"));
        ctl->setSaveDir(QDir::cleanPath(parent + QStringLiteral("/") + folder));
    }
    if (!label.isEmpty())
        ctl->setName(label);

    // controller 销毁时若任务未结束，标记失败避免悬挂。
    connect(ctl, &QObject::destroyed, this, [this, id = t->id]() {
        auto it = m_tasks.find(id);
        if (it != m_tasks.end() && !(*it)->finished) {
            (*it)->finished = true;
            (*it)->ok = false;
            (*it)->message = tr("任务进程已终止");
            emit taskFinished(id, false, (*it)->message);
        }
    });

    t->status = tr("已提交，开始安装");
    emit taskStatus(t->id, t->status);
    ctl->create();   // 异步：进度通过信号转发
    return t->id;
}

QString InstallCoordinator::installModServer(const QString &version,
                                              const QStringList &loaders,
                                              const QString &label)
{
    return installServer(QStringLiteral("mod"), version, loaders, label, false);
}

QString InstallCoordinator::install(const QString &loader,
                                     const QString &version,
                                     const QString &label)
{
    return installModServer(version, QStringList() << loader, label);
}

void InstallCoordinator::forwardSignals(InstallTask *t)
{
    CreateServerController *ctl = t->controller;
    const QString id = t->id;

    connect(ctl, &CreateServerController::progressChanged, this, [this, t, id]() {
        t->progress = t->controller->progress();
        emit taskProgress(id, t->progress);
    });
    connect(ctl, &CreateServerController::statusTextChanged, this, [this, t, id]() {
        t->status = t->controller->statusText();
        emit taskStatus(id, t->status);
    });
    connect(ctl, &CreateServerController::stageTextChanged, this, [this, t, id]() {
        t->stageText = t->controller->stageText();
        t->stageProgressVal = t->controller->stageProgress();
        emit taskStage(id, t->stageText, t->stageProgressVal);
    });
    connect(ctl, &CreateServerController::doneChanged, this, [this, t, id]() {
        if (t->controller->done()) {
            t->finished = true;
            t->ok = true;
            t->message = tr("安装完成：%1").arg(t->controller->name());
            emit taskFinished(id, true, t->message);
            emit toast(tr("安装完成"), t->message);
            // 任务保留在簿记中，供 tasks()/投影属性显示完成态；用 clearFinished() 清理。
            if (m_focusId == id)
                m_focusId.clear();
        }
    });
    // CreateServerController 没有独立的 error 信号，失败时仅 setBusy(false) + setStatus(失败文案)。
    // 这里用状态文案关键词 + “busy 结束且未完成”双重判定失败，触发 taskFinished(false)。
    const QStringList failKeys = { tr("失败"), tr("错误"), tr("超时"),
                                   tr("无法"), tr("出错"), tr("缺失") };
    connect(ctl, &CreateServerController::busyChanged, this, [this, t, id, failKeys]() {
        if (t->finished) return;
        if (t->controller->busy()) return;          // 仅关心“结束忙碌”
        if (t->controller->done()) return;          // 成功已由 doneChanged 处理
        const QString s = t->controller->statusText();
        bool bad = false;
        for (const QString &k : failKeys)
            if (s.contains(k)) { bad = true; break; }
        if (!bad) return;                            // 非失败的正常阶段切换，忽略
        t->finished = true;
        t->ok = false;
        t->message = s;
        emit taskFinished(id, false, t->message);
        emit toast(tr("安装失败"), t->message);
        if (m_focusId == id)
            m_focusId.clear();
    });
}

InstallTask *InstallCoordinator::focusTask() const
{
    if (!m_focusId.isEmpty()) {
        auto it = m_tasks.find(m_focusId);
        if (it != m_tasks.end())
            return *it;
    }
    // 焦点为空（或已结束）：返回最后一个任务，使完成态仍可见。
    if (!m_order.isEmpty()) {
        auto it = m_tasks.find(m_order.last());
        if (it != m_tasks.end())
            return *it;
    }
    return nullptr;
}

void InstallCoordinator::cleanup(const QString &id)
{
    auto it = m_tasks.find(id);
    if (it == m_tasks.end())
        return;
    InstallTask *t = *it;
    m_tasks.remove(id);
    m_order.removeAll(id);
    delete t;
}

void InstallCoordinator::clearFinished()
{
    QStringList ids;
    for (const QString &id : m_order) {
        auto it = m_tasks.find(id);
        if (it != m_tasks.end() && (*it)->finished)
            ids.append(id);
    }
    for (const QString &id : ids)
        cleanup(id);
}

QJsonArray InstallCoordinator::tasks() const
{
    QJsonArray arr;
    for (const QString &id : m_order) {
        auto it = m_tasks.find(id);
        if (it != m_tasks.end())
            arr.append(snapshot(*it));
    }
    return arr;
}

QJsonObject InstallCoordinator::task(const QString &id) const
{
    auto it = m_tasks.find(id);
    if (it == m_tasks.end())
        return QJsonObject();
    return snapshot(*it);
}

QJsonObject InstallCoordinator::snapshot(const InstallTask *t) const
{
    return t->toJson();
}

int InstallCoordinator::activeCount() const
{
    int n = 0;
    for (const QString &id : m_order) {
        auto it = m_tasks.find(id);
        if (it != m_tasks.end() && !(*it)->finished) ++n;
    }
    return n;
}

void InstallCoordinator::cancel(const QString &id)
{
    auto it = m_tasks.find(id);
    if (it == m_tasks.end())
        return;
    InstallTask *t = *it;
    if (t->finished)
        return;
    t->finished = true;
    t->ok = false;
    t->message = tr("已取消");
    emit taskFinished(id, false, t->message);
    if (m_focusId == id)
        m_focusId.clear();
}

qreal InstallCoordinator::progress() const
{
    InstallTask *t = focusTask();
    return t ? t->progress : 0;
}

QString InstallCoordinator::stageText() const
{
    InstallTask *t = focusTask();
    return t ? t->stageText : QString();
}

qreal InstallCoordinator::stageProgress() const
{
    InstallTask *t = focusTask();
    if (!t || t->finished) return 0;
    return t->stageProgressVal;
}

bool InstallCoordinator::busy() const
{
    InstallTask *t = focusTask();
    if (!t || t->finished) return false;
    return t->controller ? t->controller->busy() : true;
}

QString InstallCoordinator::statusText() const
{
    InstallTask *t = focusTask();
    return t ? t->status : QString();
}
