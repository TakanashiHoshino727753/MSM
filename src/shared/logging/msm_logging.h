// msm_logging.h —— 统一日志处理器（desktop / remote 两端共用）
// 由 main.cpp 调 msmInstallLogging() 安装；所有级别写入 <程序目录>/logs/<name>，
// Debug 构建额外回显控制台并写 debug 日志。日志文件超 5MB 自动轮转为 .1。
#pragma once
#include <QByteArray>
#include <QString>

// 安装统一日志消息处理器。normalLogName / debugLogName 为日志文件名（置于 logs/ 子目录）。
void msmInstallLogging(const QString &normalLogName, const QString &debugLogName);

// 追加一行到日志文件（已含轮转逻辑）。供诊断模块（runConsoleDiagnostics 等）直接写日志。
void msmAppendLog(const QByteArray &line);
