#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

#include "installtask.h"

class DownloadManager;
class ServerManager;
class JavaManager;
class CreateServerController;

// 安装协调器：把“服务器类型 + 版本 + 加载器”封装为可并发执行的安装任务。
// 每个任务独立创建一个 CreateServerController，互不阻塞（去掉了旧版单 busy 串行锁），
// 这样下载中心、WebUI、机器人插件都能同时下发多个安装请求。
//
// 统一 API：installServer(type, version, loaders, label, addToList) -> taskId
//   - type:     paper / vanilla / mod（模组服）等 CreateServerController 支持的类型
//   - version:  MC 版本，如 1.21.1 / 1.26.2 / 26.2
//   - loaders:  模组服加载器列表（forge/fabric/neoforge），非模组服可空
//   - label:    服务器名称（空则用默认名）
//   - addToList: 完成后是否加入服务器列表（true=创建服务器，false=下载中心式仅准备产物）
//
// 进度通过 taskProgress / taskStatus / taskStage / taskFinished 信号上报，也可用 tasks() 轮询快照。
// 为兼容旧下载中心 QML（单进度条），额外暴露 install() + progress/stageText/stageProgress/busy
// 等投影属性，指向“焦点任务”（最近一个活动任务）。
//
// 注意：本类依赖 serverctl（CreateServerController），故放在 msm_serverctl 层，
// 避免 download 模块反向依赖 serverctl 造成循环。
class InstallCoordinator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress NOTIFY taskProgress)
    Q_PROPERTY(QString stageText READ stageText NOTIFY taskStage)
    Q_PROPERTY(qreal stageProgress READ stageProgress NOTIFY taskStage)
    Q_PROPERTY(bool busy READ busy NOTIFY taskStatus)
    Q_PROPERTY(QString statusText READ statusText NOTIFY taskStatus)
public:
    explicit InstallCoordinator(DownloadManager *dm, ServerManager *sm,
                                JavaManager *java, QObject *parent = nullptr);
    ~InstallCoordinator();

    // 提交一个安装任务，立即返回任务 id（Q_INVOKABLE，供 QML / 远程 API 调用）。
    Q_INVOKABLE QString installServer(const QString &type,
                                      const QString &version,
                                      const QStringList &loaders,
                                      const QString &label = QString(),
                                      bool addToList = true);

    // 兼容旧下载中心 QML 的便捷入口：模组服（addToList=false）。
    Q_INVOKABLE QString installModServer(const QString &version,
                                         const QStringList &loaders,
                                         const QString &label = QString());

    // 兼容旧下载中心 QML：install(loader, version, label)
    Q_INVOKABLE QString install(const QString &loader,
                                const QString &version,
                                const QString &label = QString());

    // 查询所有任务当前快照（含进度/状态），供 WebUI / 机器人轮询。
    Q_INVOKABLE QJsonArray tasks() const;

    // 查询单个任务快照；不存在返回空对象。
    Q_INVOKABLE QJsonObject task(const QString &id) const;

    // 任务总数（进行中 + 已完成未清理）。
    Q_INVOKABLE int activeCount() const;

    // 取消某个任务（仅标记，controller 会在可中断点停止）。
    Q_INVOKABLE void cancel(const QString &id);

    // 清理所有已完成任务（释放簿记）。
    Q_INVOKABLE void clearFinished();

    // ---- 投影属性（焦点任务），兼容旧 QML 单进度条 ----
    qreal progress() const;
    QString stageText() const;
    qreal stageProgress() const;
    bool busy() const;
    QString statusText() const;

signals:
    void taskProgress(const QString &id, qreal percent);
    void taskStatus(const QString &id, const QString &text);
    void taskStage(const QString &id, const QString &text, qreal percent);
    void taskFinished(const QString &id, bool ok, const QString &message);
    // 供 UI 弹 toast（失败/成功汇总）。
    void toast(const QString &title, const QString &text);

private:
    InstallTask *focusTask() const;
    void forwardSignals(InstallTask *t);
    void cleanup(const QString &id);
    QJsonObject snapshot(const InstallTask *t) const;

    DownloadManager *m_dm = nullptr;
    ServerManager *m_sm = nullptr;
    JavaManager *m_java = nullptr;
    QHash<QString, InstallTask *> m_tasks;
    QList<QString> m_order;           // 保序：任务的提交顺序
    QString m_focusId;                // 焦点任务：最近提交且未结束者
};
