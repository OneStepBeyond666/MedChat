#include "chatserver.h"
#include "clienthandler.h"
#include "usermanager.h"
#include "common/protocol.h"
#include <QDebug>
#include <QJsonArray>

ChatServer::ChatServer(quint16 port, QObject *parent)
    : QObject(parent), m_port(port)
{
    m_tcpServer = new QTcpServer(this);
    m_userManager = new UserManager("users.json", this);
    connect(m_tcpServer, &QTcpServer::newConnection, this, &ChatServer::onNewConnection);
}

ChatServer::~ChatServer()
{
    stop();
}

bool ChatServer::start()
{
    if (!m_tcpServer->listen(QHostAddress::Any, m_port)) {
        qCritical() << "[服务器] 监听失败，端口" << m_port
                    << ":" << m_tcpServer->errorString();
        return false;
    }
    qDebug() << "[服务器] 成功监听端口" << m_port;
    return true;
}

void ChatServer::stop()
{
    m_tcpServer->close();
    for (ClientHandler *h : m_clients) {
        h->socket()->disconnectFromHost();
    }
    m_clients.clear();
}

void ChatServer::onNewConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        ClientHandler *handler = new ClientHandler(socket, this, this);
        connect(handler, &ClientHandler::messageReceived, this, &ChatServer::onMessageReceived);
        connect(handler, &ClientHandler::disconnected, this, &ChatServer::onClientDisconnected);
        m_clients.append(handler);
        QString ip = socket->peerAddress().toString();
        qDebug() << QString("[连接] 新客户端连接 IP=%1 (当前连接数: %2)")
                        .arg(ip).arg(m_clients.size());
    }
}

void ChatServer::onMessageReceived(ClientHandler *handler, const QJsonObject &msg)
{
    QString type = msg["type"].toString();

    if (type == MsgType::Register) {
        handleRegister(handler, msg);
    } else if (type == MsgType::Login) {
        handleLogin(handler, msg);
    } else if (type == MsgType::Message) {
        handleMessage(handler, msg);
    } else if (type == MsgType::FileOffer) {
        handleFileOffer(handler, msg);
    } else if (type == MsgType::FileAccept) {
        handleFileAccept(handler, msg);
    } else if (type == MsgType::FileData) {
        handleFileData(handler, msg);
    } else if (type == MsgType::FileEnd) {
        handleFileEnd(handler, msg);
    } else if (type == MsgType::FileReject) {
        handleFileReject(handler, msg);
    } else {
        qWarning() << "[未知消息] 类型:" << type;
    }
}

void ChatServer::onClientDisconnected(ClientHandler *handler)
{
    QString username = handler->username();
    QString ip = handler->socket()->peerAddress().toString();
    m_clients.removeAll(handler);
    handler->deleteLater();
    if (!username.isEmpty()) {
        qDebug() << QString("[下线] %1 (%2) 断开连接").arg(username, handler->role() == "doctor" ? "医生" : "患者");
        broadcastOnlineStatus();
    } else {
        qDebug() << QString("[断开] 未登录客户端 IP=%1").arg(ip);
    }

    // 统计当前在线用户
    int onlineCount = 0;
    for (ClientHandler *h : m_clients) {
        if (!h->username().isEmpty()) onlineCount++;
    }
    qDebug() << QString("[统计] 当前在线用户: %1 人").arg(onlineCount);
}

void ChatServer::handleRegister(ClientHandler *handler, const QJsonObject &msg)
{
    QString username = msg["username"].toString();
    QString password = msg["password"].toString();
    QString role = msg["role"].toString();

    if (username.isEmpty() || password.isEmpty() || role.isEmpty()) {
        qDebug() << "[注册失败] 参数不完整";
        handler->sendMessage(Protocol::makeAuthResult(false, "用户名、密码和角色不能为空"));
        return;
    }
    if (role != Role::Doctor && role != Role::Patient) {
        qDebug() << "[注册失败] 无效角色:" << role;
        handler->sendMessage(Protocol::makeAuthResult(false, "无效的角色类型"));
        return;
    }
    if (m_userManager->userExists(username)) {
        qDebug() << "[注册失败] 用户名已存在:" << username;
        handler->sendMessage(Protocol::makeAuthResult(false, "用户名已存在"));
        return;
    }
    if (m_userManager->registerUser(username, password, role)) {
        handler->setUsername(username);
        handler->setRole(role);
        handler->sendMessage(Protocol::makeAuthResult(true, "注册成功", role));
        qDebug() << QString("[注册成功] %1 (%2) 已注册并上线")
                        .arg(username, role == "doctor" ? "医生" : "患者");
        broadcastOnlineStatus();
        sendContactList(handler);
    } else {
        qDebug() << "[注册失败] 存储失败:" << username;
        handler->sendMessage(Protocol::makeAuthResult(false, "注册失败，请重试"));
    }
}

void ChatServer::handleLogin(ClientHandler *handler, const QJsonObject &msg)
{
    QString username = msg["username"].toString();
    QString password = msg["password"].toString();

    if (username.isEmpty() || password.isEmpty()) {
        qDebug() << "[登录失败] 参数不完整";
        handler->sendMessage(Protocol::makeAuthResult(false, "用户名和密码不能为空"));
        return;
    }

    // 检查是否已登录
    for (ClientHandler *h : m_clients) {
        if (h != handler && h->username() == username) {
            qDebug() << "[登录拒绝] " << username << "已在其他设备登录";
            handler->sendMessage(Protocol::makeAuthResult(false, "该账号已在其他设备登录"));
            return;
        }
    }

    QString role;
    if (m_userManager->authenticate(username, password, role)) {
        handler->setUsername(username);
        handler->setRole(role);
        handler->sendMessage(Protocol::makeAuthResult(true, "登录成功", role));
        qDebug() << QString("[上线] %1 (%2) 登录成功")
                        .arg(username, role == "doctor" ? "医生" : "患者");

        // 统计在线人数
        int onlineCount = 0;
        for (ClientHandler *h : m_clients) {
            if (!h->username().isEmpty()) onlineCount++;
        }
        qDebug() << QString("[统计] 当前在线用户: %1 人").arg(onlineCount);

        broadcastOnlineStatus();
        sendContactList(handler);
    } else {
        qDebug() << "[登录失败] " << username << " 用户名或密码错误";
        handler->sendMessage(Protocol::makeAuthResult(false, "用户名或密码错误"));
    }
}

void ChatServer::handleMessage(ClientHandler *handler, const QJsonObject &msg)
{
    QString to = msg["to"].toString();
    QString text = msg["text"].toString();
    ClientHandler *target = findClient(to);
    if (!target) {
        qDebug() << QString("[消息失败] %1 -> %2 : 接收方不在线")
                        .arg(handler->username(), to);
        handler->sendMessage(Protocol::makeError("用户 " + to + " 不在线"));
        return;
    }
    // 转发消息给接收方
    QJsonObject fwd = msg;
    fwd["from"] = handler->username();
    target->sendMessage(fwd);

    // 日志：显示消息内容
    QString preview = text.length() > 80 ? text.left(80) + "..." : text;
    qDebug() << QString("[消息] %1 -> %2 : %3")
                    .arg(handler->username(), to, preview);

    // 回传确认给发送方
    QJsonObject ack = Protocol::makeMsg("message_ack");
    ack["to"] = to;
    ack["time"] = msg["time"];
    handler->sendMessage(ack);
}

void ChatServer::handleFileOffer(ClientHandler *handler, const QJsonObject &msg)
{
    QString to = msg["to"].toString();
    QString fileName = msg["file_name"].toString();
    qint64 fileSize = static_cast<qint64>(msg["file_size"].toDouble());
    ClientHandler *target = findClient(to);
    if (!target) {
        qDebug() << QString("[文件失败] %1 -> %2 : 接收方不在线 (文件: %3)")
                        .arg(handler->username(), to, fileName);
        handler->sendMessage(Protocol::makeError("用户 " + to + " 不在线"));
        return;
    }
    QJsonObject fwd = msg;
    fwd["from"] = handler->username();
    target->sendMessage(fwd);
    qDebug() << QString("[文件传输] %1 -> %2 : %3 (%4 bytes)")
                    .arg(handler->username(), to, fileName).arg(fileSize);
}

void ChatServer::handleFileAccept(ClientHandler *handler, const QJsonObject &msg)
{
    QString to = msg["to"].toString();
    ClientHandler *target = findClient(to);
    if (target) {
        QJsonObject fwd = msg;
        fwd["from"] = handler->username();
        target->sendMessage(fwd);
        qDebug() << QString("[文件接受] %1 接受了 %2 的文件传输请求")
                        .arg(handler->username(), to);
    }
}

void ChatServer::handleFileData(ClientHandler *handler, const QJsonObject &msg)
{
    // file_data 需要转发给接收方，通过 file_id 查找接收方
    // 这里简单处理：file_data 中包含 to 字段
    QString to = msg["to"].toString();
    ClientHandler *target = findClient(to);
    if (target) {
        target->sendMessage(msg);
    }
}

void ChatServer::handleFileEnd(ClientHandler *handler, const QJsonObject &msg)
{
    QString to = msg["to"].toString();
    ClientHandler *target = findClient(to);
    if (target) {
        target->sendMessage(msg);
    }
}

void ChatServer::handleFileReject(ClientHandler *handler, const QJsonObject &msg)
{
    QString to = msg["to"].toString();
    ClientHandler *target = findClient(to);
    if (target) {
        QJsonObject fwd = msg;
        fwd["from"] = handler->username();
        target->sendMessage(fwd);
        qDebug() << QString("[文件拒绝] %1 拒绝了 %2 的文件传输请求")
                        .arg(handler->username(), to);
    }
}

void ChatServer::broadcastOnlineStatus()
{
    for (ClientHandler *h : m_clients) {
        if (h->username().isEmpty())
            continue;

        QJsonArray users;
        for (ClientHandler *other : m_clients) {
            if (other != h && !other->username().isEmpty()) {
                QJsonObject u;
                u["username"] = other->username();
                u["role"] = other->role();
                u["online"] = true;
                users.append(u);
            }
        }

        QJsonObject statusMsg = Protocol::makeMsg(MsgType::OnlineStatus);
        statusMsg["users"] = users;
        h->sendMessage(statusMsg);
    }
}

void ChatServer::sendContactList(ClientHandler *handler)
{
    QJsonArray users;
    for (ClientHandler *h : m_clients) {
        if (h != handler && !h->username().isEmpty()) {
            QJsonObject u;
            u["username"] = h->username();
            u["role"] = h->role();
            u["online"] = true;
            users.append(u);
        }
    }

    QJsonObject listMsg = Protocol::makeMsg(MsgType::ContactList);
    listMsg["contacts"] = users;
    handler->sendMessage(listMsg);
}

ClientHandler* ChatServer::findClient(const QString &username) const
{
    for (ClientHandler *h : m_clients) {
        if (h->username() == username)
            return h;
    }
    return nullptr;
}
