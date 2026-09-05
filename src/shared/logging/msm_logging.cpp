// msm_logging.cpp —— 见 msm_logging.h
#include "msm_logging.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QDateTime>
#include <QtCore/QMessageLogContext>
#include <QtCore/QDebug>

static QString g_normalName = QStringLiteral("msm.log");
static QString g_debugName  = QStringLiteral("msm-debug.log");
static QFile g_normalFile;
static QFile g_debugFile;
// 单文件上限：超过则轮转为 .1（保留一份历史），避免日志无限增长占满磁盘。
static constexpr qint64 kMsmLogMaxBytes = 5 * 1024 * 1024;

static QString msmLogDir()
{
    QString dir = QCoreApplication::instance()
                      ? QCoreApplication::applicationDirPath()
                      : QDir::currentPath();
    return dir + QStringLiteral("/logs");
}

// 确保日志文件已打开（首次调用时创建 logs 子目录并打开）。线程不安全，但 QML/Qt 主线程单线调用足够。
static void msmEnsureLogFile(QFile &f, const QString &name)
{
    if (f.isOpen())
        return;
    QDir().mkpath(msmLogDir());
    f.setFileName(msmLogDir() + QLatin1Char('/') + name);
    f.open(QIODevice::Append | QIODevice::Text);
}

// 把一行（不含换行）追加到日志文件；写入前检查体积，超阈值则轮转（仅保留最近一份备份）。
static void msmWriteRolling(QFile &f, const QByteArray &line)
{
    if (!f.isOpen())
        return;
    if (f.size() + line.size() + 1 > kMsmLogMaxBytes) {
        f.close();
        const QString path = f.fileName();
        QFile::remove(path + QStringLiteral(".1"));          // 丢弃上一轮备份
        f.rename(path + QStringLiteral(".1"));                // 当前日志转为 .1
        f.open(QIODevice::Append | QIODevice::Text);
        if (!f.isOpen())
            return;
    }
    f.write(line);
    f.write("\n");
    f.flush();
}

void msmAppendLog(const QByteArray &line)
{
    msmEnsureLogFile(g_normalFile, g_normalName);
    msmWriteRolling(g_normalFile, line);
#ifdef QT_DEBUG
    // Debug 构建：同时写入 debug 文件，便于单独排查调试输出
    msmEnsureLogFile(g_debugFile, g_debugName);
    msmWriteRolling(g_debugFile, line);
#endif
}

static void msmMessageOutput(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const char *level = "?";
    switch (type) {
        case QtDebugMsg: level = "DBG"; break;
        case QtInfoMsg:  level = "INF"; break;
        case QtWarningMsg: level = "WRN"; break;
        case QtCriticalMsg: level = "CRT"; break;
        case QtFatalMsg: level = "FTL"; break;
    }
    QByteArray line = QDateTime::currentDateTime()
                          .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")).toLocal8Bit();
    line += ' '; line += level; line += ' ';
    if (ctx.category && ctx.category[0]) { line += ctx.category; line += ' '; }
    line += msg.toLocal8Bit();
    msmAppendLog(line);
    // 控制台回显：仅 Debug 构建输出到 cmd；Release 构建不污染控制台，日志只进 normal 文件。
    // 用 Qt 自带的 QT_DEBUG 宏做编译期限制。
#ifdef QT_DEBUG
    qt_message_output(type, ctx, msg);
#endif
}

void msmInstallLogging(const QString &normalLogName, const QString &debugLogName)
{
    g_normalName = normalLogName;
    g_debugName = debugLogName;
    qInstallMessageHandler(msmMessageOutput);
}
