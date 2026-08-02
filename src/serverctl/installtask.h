#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>

class CreateServerController; // 仅持有指针，避免暴露 93KB 头部，降低编译耦合

// 单个安装任务的状态容器（纯数据 + 序列化），不含协调逻辑。
// 由 InstallCoordinator 创建并负责生命周期，对外通过 toJson() 暴露快照。
class InstallTask : public QObject
{
    Q_OBJECT
public:
    explicit InstallTask(QObject *parent = nullptr) : QObject(parent) {}

    // 任务标识与请求参数
    QString id;
    QString type;          // paper / vanilla / mod
    QString version;       // MC 版本
    QStringList loaders;   // 加载器列表（mod 时非空）
    QString label;         // 自定义服务端名
    bool addToList = true; // 完成后是否加入服务器列表

    // 运行时状态
    CreateServerController *controller = nullptr;
    qreal progress = 0;        // 0-100（内部存储），快照时 /100.0
    QString status;            // 状态文案
    QString stageText;         // 当前阶段文案
    qreal stageProgressVal = 0;// 阶段内部进度 0-100
    bool finished = false;
    bool ok = false;
    QString message;

    // 对外快照（progress / stageProgress 已转为 0-1 比例，与前端渲染一致）。
    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QStringLiteral("id")] = id;
        o[QStringLiteral("type")] = type;
        o[QStringLiteral("version")] = version;
        o[QStringLiteral("loaders")] = QJsonArray::fromStringList(loaders);
        o[QStringLiteral("label")] = label;
        o[QStringLiteral("addToList")] = addToList;
        o[QStringLiteral("progress")] = progress / 100.0;
        o[QStringLiteral("status")] = status;
        o[QStringLiteral("stage")] = stageText;
        o[QStringLiteral("stageProgress")] = stageProgressVal / 100.0;
        o[QStringLiteral("finished")] = finished;
        o[QStringLiteral("ok")] = ok;
        o[QStringLiteral("message")] = message;
        return o;
    }
};
