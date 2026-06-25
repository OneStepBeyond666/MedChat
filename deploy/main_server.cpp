// MedChat Server - Linux 独立服务端
// 仅依赖 Qt5Core + Qt5Network，无 GUI 组件

#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>
#include <cstdio>

#include "chatserver.h"

static void serverLog(const QString &msg)
{
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    fprintf(stdout, "[%s] %s\n",
            ts.toLocal8Bit().constData(),
            msg.toLocal8Bit().constData());
    fflush(stdout);
}

static void serverMessageHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    serverLog(msg);
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, nullptr, _IOLBF, 0);

    quint16 port = 9527;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = QString::fromLocal8Bit(argv[++i]).toUShort();
        }
    }

    QCoreApplication app(argc, argv);

    serverLog("================================================");
    serverLog("   MedChat Server v3.2  -  远程问诊服务器 (Linux)");
    serverLog("================================================");
    serverLog(QString("  监听端口: %1").arg(port));
    serverLog(QString("  启动时间: %1").arg(
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")));
    serverLog("------------------------------------------------");
    serverLog("  终端日志: 实时显示用户上线/消息/文件/下线");
    serverLog("  按 Ctrl+C 停止服务器");
    serverLog("================================================");

    qInstallMessageHandler(serverMessageHandler);

    ChatServer server(port);
    if (!server.start()) {
        serverLog(QString("[错误] 无法启动服务器，端口 %1 可能被占用!").arg(port));
        return 1;
    }

    serverLog("[系统] 服务器已就绪，等待客户端连接...");
    serverLog("");

    QTimer heartbeat;
    QObject::connect(&heartbeat, &QTimer::timeout, []() {});
    heartbeat.start(1000);

    return app.exec();
}
