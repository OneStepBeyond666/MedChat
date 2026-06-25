#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QTimer>

#include "server/chatserver.h"
#include "client/chatclient.h"
#include "client/loginwindow.h"
#include "client/mainwindow.h"
#include "ui/avatarcropper.h"

#include <cstdio>

int runServer(int argc, char *argv[], quint16 port);
int runClient(int argc, char *argv[]);

// 服务端日志：统一走 QString → toLocal8Bit (UTF-8→GBK)，保证中文不乱码
static void serverLog(const QString &msg)
{
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    fprintf(stdout, "[%s] %s\n", ts.toLocal8Bit().constData(), msg.toLocal8Bit().constData());
    fflush(stdout);
}

// 自定义 qDebug 处理器，走 serverLog
static void serverMessageHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    serverLog(msg);
}

int main(int argc, char *argv[])
{
    bool isServer = false;
    quint16 port = 9527;

    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--server" || arg == "-s") {
            isServer = true;
        } else if (arg == "--port" || arg == "-p") {
            if (i + 1 < argc) {
                port = QString::fromLocal8Bit(argv[++i]).toUShort();
            }
        }
    }

    if (isServer) {
        return runServer(argc, argv, port);
    } else {
        return runClient(argc, argv);
    }
}

// ===========================
// 纯服务端模式（命令行终端）
// ===========================
int runServer(int argc, char *argv[], quint16 port)
{
    // 行缓冲，确保每行立刻输出
    setvbuf(stdout, nullptr, _IOLBF, 0);

    QCoreApplication app(argc, argv);

    // 先不装 handler，用 fprintf 打 banner（保证一定能看到）
    serverLog("================================================");
    serverLog("   MedChat Server v3.2  -  远程问诊服务器");
    serverLog("================================================");
    serverLog(QString("  监听端口: %1").arg(port));
    serverLog(QString("  启动时间: %1").arg(
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")));
    serverLog("------------------------------------------------");
    serverLog("  终端日志: 实时显示用户上线/消息/文件/下线");
    serverLog("  按 Ctrl+C 停止服务器");
    serverLog("================================================");

    // 安装 handler，后续 qDebug 都走 serverLog
    qInstallMessageHandler(serverMessageHandler);

    ChatServer server(port);
    if (!server.start()) {
        serverLog(QString("[错误] 无法启动服务器，端口 %1 可能被占用!").arg(port));
        return 1;
    }

    serverLog("[系统] 服务器已就绪，等待客户端连接...");
    serverLog("");

    // 用定时器每秒打一个心跳点，证明进程还活着
    QTimer heartbeat;
    QObject::connect(&heartbeat, &QTimer::timeout, []() {
        // 不输出，只是保持事件循环活跃
    });
    heartbeat.start(1000);

    return app.exec();
}

// ===========================
// 客户端模式（无内嵌服务器）
// ===========================
int runClient(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("MedChat");
    app.setApplicationVersion("1.0");
    app.setWindowIcon(QIcon(":/app_icon.png"));

    // 清空头像缓存，确保使用最新的圆形直径
    AvatarCropper::clearCache();

    // 不再内嵌服务器 — 需要先通过 --server 参数启动独立服务器
    ChatClient *client = new ChatClient(&app);

    LoginWindow *loginWin = new LoginWindow(client);

    QObject::connect(loginWin, &LoginWindow::loginSuccess,
        [loginWin, client](const QString &username, const QString &role, const QString &nickname, const QByteArray &avatarData) {
            loginWin->hide();

            MainWindow *mainWin = new MainWindow(client, username, role, nickname, avatarData);
            mainWin->show();

            QObject::connect(mainWin, &MainWindow::destroyed, &QApplication::quit);
        }
    );

    loginWin->show();
    return app.exec();
}
