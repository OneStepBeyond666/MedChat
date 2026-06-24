#include "chatserver.h"
#include "clienthandler.h"
#include "usermanager.h"
#include "serverdb.h"
#include "common/protocol.h"
#include "common/constants.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>

ChatServer::ChatServer(quint16 port, QObject *parent)
    : QObject(parent), m_port(port)
{
    m_tcpServer = new QTcpServer(this);

    // 初始化 SQLite 数据库：{程序目录}/medchat_data/
    QString dbDir = QCoreApplication::applicationDirPath() + "/" + Constants::DB_FOLDER_NAME;
    m_serverDB = new ServerDB(dbDir, this);

    // 用户管理器使用 SQLite
    m_userManager = new UserManager(m_serverDB, this);

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
    } else if (type == MsgType::OfflineSyncAck) {
        handleOfflineSyncAck(handler, msg);
    } else if (type == MsgType::UpdateProfile) {
        handleUpdateProfile(handler, msg);
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
    QString nickname = msg["nickname"].toString();

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
    if (m_userManager->registerUser(username, password, role, nickname)) {
        handler->setUsername(username);
        handler->setRole(role);
        QString actualNick = nickname.trimmed().isEmpty() ? username : nickname.trimmed();
        QJsonObject authResp = Protocol::makeAuthResult(true, "注册成功", role);
        authResp["nickname"] = actualNick;
        handler->sendMessage(authResp);
        qDebug() << QString("[注册成功] %1 (昵称: %2, %3) 已注册并上线")
                        .arg(username, actualNick, role == "doctor" ? "医生" : "患者");
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
        UserInfo info = m_userManager->getUserInfoByName(username);
        QJsonObject authResp = Protocol::makeAuthResult(true, "登录成功", role);
        authResp["nickname"] = info.nickname;
        if (!info.avatarBlob.isEmpty())
            authResp["avatar_base64"] = QString::fromLatin1(info.avatarBlob.toBase64());
        handler->sendMessage(authResp);
        qDebug() << QString("[上线] %1 (昵称: %2, %3) 登录成功")
                        .arg(username, info.nickname, role == "doctor" ? "医生" : "患者");

        // 统计在线人数
        int onlineCount = 0;
        for (ClientHandler *h : m_clients) {
            if (!h->username().isEmpty()) onlineCount++;
        }
        qDebug() << QString("[统计] 当前在线用户: %1 人").arg(onlineCount);

        // 离线消息下发（方案C：发→标记→等ACK→删）
        int uid = m_userManager->getUidByUsername(username);
        if (uid > 0) {
            QJsonArray offlineMessages = m_serverDB->getPendingOfflineMessages(uid);
            if (!offlineMessages.isEmpty()) {
                qDebug() << QString("[离线消息] %1 有 %2 条离线消息，开始下发")
                                .arg(username).arg(offlineMessages.size());
                
                // 遍历结果，通过 Socket 逐条将 payload 发送给客户端
                for (const QJsonValue &val : offlineMessages) {
                    if (val.isObject()) {
                        QJsonObject offlineMsg = val.toObject();
                        handler->sendMessage(offlineMsg);
                    }
                }
                
                // 发送完毕后标记为 status=1（已发送，待ACK）
                m_serverDB->markOfflineMessagesAsSent(uid);
                
                qDebug() << QString("[离线消息] %1 离线消息下发完成，等待ACK")
                                .arg(username);
            }
        }

        // 发送离线同步结束标志
        QJsonObject syncDone = Protocol::makeMsg(MsgType::OfflineSync);
        syncDone["done"] = true;
        handler->sendMessage(syncDone);

        broadcastOnlineStatus();
        sendContactList(handler);
    } else {
        qDebug() << "[登录失败] " << username << " 用户名或密码错误";
        handler->sendMessage(Protocol::makeAuthResult(false, "用户名或密码错误"));
    }
}

void ChatServer::handleMessage(ClientHandler *handler, const QJsonObject &msg)
{
    QString from = handler->username();
    QString to = msg["to"].toString();

    // 好友关系拦截：非好友不能发消息
    int fromUid = m_userManager->getUidByUsername(from);
    int toUid   = m_userManager->getUidByUsername(to);
    if (fromUid <= 0 || toUid <= 0) {
        qWarning() << "[消息拦截] 无法获取 uid:" << from << "->" << to;
        QJsonObject err = Protocol::makeMsg(MsgType::ErrorStranger);
        err["text"] = "对方开启了好友验证，您还不是他(她)的好友";
        handler->sendMessage(err);
        return;
    }
    if (!m_serverDB->areFriends(fromUid, toUid)) {
        qDebug() << QString("[消息拦截] %1 -> %2 : 不是好友，拦截")
                        .arg(from, to);
        QJsonObject err = Protocol::makeMsg(MsgType::ErrorStranger);
        err["text"] = "对方开启了好友验证，您还不是他(她)的好友";
        handler->sendMessage(err);
        return;
    }

    QString text = msg["text"].toString();
    ClientHandler *target = findClient(to);
    if (!target) {
        // 目标不在线，保存离线消息
        qDebug() << QString("[消息离线] %1 -> %2 : 接收方不在线，保存离线消息")
                        .arg(from, to);
        
        // 将完整 JSON 帧存入数据库（加上 offline 标记供客户端去重）
        QJsonObject fwdMsg = msg;
        fwdMsg["from"] = from;
        fwdMsg["offline"] = true;
        QJsonDocument doc(fwdMsg);
        QString payload = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        
        if (m_serverDB->saveOfflineMessage(fromUid, toUid, payload, 0)) {
            // 离线消息保存成功，通知发送方
            QJsonObject ack = Protocol::makeMsg("message_ack");
            ack["to"] = to;
            ack["time"] = msg["time"];
            ack["offline"] = true;  // 标记这是离线消息
            handler->sendMessage(ack);
            
            qDebug() << QString("[消息离线] %1 -> %2 : 离线消息已保存")
                            .arg(from, to);
        } else {
            // 保存失败
            handler->sendMessage(Protocol::makeError("消息发送失败，请重试"));
        }
        return;
    }
    // 转发消息给接收方
    QJsonObject fwd = msg;
    fwd["from"] = from;
    target->sendMessage(fwd);

    // 日志：显示消息内容
    QString preview = text.length() > 80 ? text.left(80) + "..." : text;
    qDebug() << QString("[消息] %1 -> %2 : %3")
                    .arg(from, to, preview);

    // 回传确认给发送方
    QJsonObject ack = Protocol::makeMsg("message_ack");
    ack["to"] = to;
    ack["time"] = msg["time"];
    handler->sendMessage(ack);
}

void ChatServer::handleFileOffer(ClientHandler *handler, const QJsonObject &msg)
{
    QString from = handler->username();
    QString to = msg["to"].toString();
    QString fileName = msg["file_name"].toString();
    qint64 fileSize = static_cast<qint64>(msg["file_size"].toDouble());

    // 好友关系拦截：非好友不能发文件
    int fromUid = m_userManager->getUidByUsername(from);
    int toUid   = m_userManager->getUidByUsername(to);
    if (fromUid <= 0 || toUid <= 0) {
        qWarning() << "[文件拦截] 无法获取 uid:" << from << "->" << to;
        QJsonObject err = Protocol::makeMsg(MsgType::ErrorStranger);
        err["text"] = "对方开启了好友验证，您还不是他(她)的好友";
        handler->sendMessage(err);
        return;
    }
    if (!m_serverDB->areFriends(fromUid, toUid)) {
        qDebug() << QString("[文件拦截] %1 -> %2 : 不是好友，拦截 (文件: %3)")
                        .arg(from, to, fileName);
        QJsonObject err = Protocol::makeMsg(MsgType::ErrorStranger);
        err["text"] = "对方开启了好友验证，您还不是他(她)的好友";
        handler->sendMessage(err);
        return;
    }

    ClientHandler *target = findClient(to);
    if (!target) {
        // 目标不在线，不缓存文件，直接返回错误
        qDebug() << QString("[文件离线] %1 -> %2 : 接收方不在线，暂不支持发送离线文件 (文件: %3)")
                        .arg(from, to, fileName);
        handler->sendMessage(Protocol::makeError("对方不在线，暂不支持发送离线文件"));
        return;
    }
    QJsonObject fwd = msg;
    fwd["from"] = from;
    target->sendMessage(fwd);
    qDebug() << QString("[文件传输] %1 -> %2 : %3 (%4 bytes)")
                    .arg(from, to, fileName).arg(fileSize);
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
                UserInfo info = m_userManager->getUserInfoByName(other->username());
                QJsonObject u;
                u["username"] = other->username();
                u["nickname"] = info.nickname;
                u["role"] = other->role();
                u["online"] = true;
                if (!info.avatarBlob.isEmpty())
                    u["avatar_base64"] = QString::fromLatin1(info.avatarBlob.toBase64());
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
            UserInfo info = m_userManager->getUserInfoByName(h->username());
            QJsonObject u;
            u["username"] = h->username();
            u["nickname"] = info.nickname;
            u["role"] = h->role();
            u["online"] = true;
            if (!info.avatarBlob.isEmpty())
                u["avatar_base64"] = QString::fromLatin1(info.avatarBlob.toBase64());
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

void ChatServer::handleOfflineSyncAck(ClientHandler *handler, const QJsonObject &msg)
{
    QString username = handler->username();
    int uid = m_userManager->getUidByUsername(username);
    if (uid <= 0) {
        qWarning() << "[离线ACK] 无法获取 uid:" << username;
        return;
    }

    // 收到客户端 ACK，删除 status=1 的记录
    if (m_serverDB->deleteAckedOfflineMessages(uid)) {
        qDebug() << QString("[离线ACK] %1 离线消息已确认并清除").arg(username);
    } else {
        qWarning() << "[离线ACK] 清除离线消息失败: uid=" << uid;
    }
}

void ChatServer::handleUpdateProfile(ClientHandler *handler, const QJsonObject &msg)
{
    QString username = handler->username();
    if (username.isEmpty()) {
        handler->sendMessage(Protocol::makeError("未登录"));
        return;
    }

    QString nickname = msg["nickname"].toString();
    QString avatarB64 = msg["avatar_base64"].toString();

    // 更新昵称
    if (!nickname.isEmpty()) {
        if (!m_userManager->updateNickname(username, nickname)) {
            handler->sendMessage(Protocol::makeError("昵称更新失败"));
            return;
        }
        qDebug() << QString("[资料更新] %1 昵称更新为: %2").arg(username, nickname);
    }

    // 更新头像
    QByteArray avatarData;
    if (!avatarB64.isEmpty()) {
        QByteArray rawAvatar = QByteArray::fromBase64(avatarB64.toLatin1());
        if (!m_userManager->updateAvatar(username, rawAvatar)) {
            handler->sendMessage(Protocol::makeError("头像更新失败"));
            return;
        }
        // 获取处理后的头像
        int uid = m_userManager->getUidByUsername(username);
        if (uid > 0)
            avatarData = m_userManager->getAvatar(uid);
        qDebug() << QString("[资料更新] %1 头像已更新").arg(username);
    }

    // 构建广播消息，通知所有在线联系人
    UserInfo updatedInfo = m_userManager->getUserInfoByName(username);
    QJsonObject broadcast = Protocol::makeMsg(MsgType::ProfileUpdated);
    broadcast["username"] = username;
    broadcast["nickname"] = updatedInfo.nickname;
    if (!updatedInfo.avatarBlob.isEmpty())
        broadcast["avatar_base64"] = QString::fromLatin1(updatedInfo.avatarBlob.toBase64());
    else
        broadcast["avatar_base64"] = QString();

    // 广播给所有在线客户端（包括自己，以便自己的UI也更新）
    for (ClientHandler *h : m_clients) {
        if (!h->username().isEmpty())
            h->sendMessage(broadcast);
    }
}
