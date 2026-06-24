#include "chatserver.h"
#include "clienthandler.h"
#include "usermanager.h"
#include "serverdb.h"
#include "common/protocol.h"
#include "common/constants.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QDateTime>
#include <QSet>
#include <QSqlQuery>

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
    } else if (type == MsgType::FriendRequest) {
        handleFriendRequest(handler, msg);
    } else if (type == MsgType::FriendResponse) {
        handleFriendResponse(handler, msg);
    } else if (type == MsgType::GetSecQuestion) {
        handleGetSecQuestion(handler, msg);
    } else if (type == MsgType::ResetPassword) {
        handleResetPassword(handler, msg);
    } else if (type == MsgType::ChangePassword) {
        handleChangePassword(handler, msg);
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
    QString secQuestion = msg["sec_question"].toString();
    QString secAnswerHash = msg["sec_answer_hash"].toString();

    if (username.isEmpty() || password.isEmpty() || role.isEmpty()) {
        qDebug() << "[注册失败] 参数不完整";
        handler->sendMessage(Protocol::makeAuthResult(false, "用户名、密码和角色不能为空"));
        return;
    }
    if (secQuestion.isEmpty() || secAnswerHash.isEmpty()) {
        qDebug() << "[注册失败] 密保问题或答案不能为空";
        handler->sendMessage(Protocol::makeAuthResult(false, "请设置密保问题和答案"));
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
    if (m_userManager->registerUser(username, password, role, nickname, secQuestion, secAnswerHash)) {
        handler->setUsername(username);
        handler->setRole(role);
        QString actualNick = nickname.trimmed().isEmpty() ? username : nickname.trimmed();
        UserInfo info = m_userManager->getUserInfoByName(username);
        QJsonObject authResp = Protocol::makeAuthResult(true, "注册成功", role);
        authResp["nickname"] = info.nickname;
        authResp["signature"] = info.signature;
        authResp["gender"] = info.gender;
        authResp["birthday"] = info.birthday;
        authResp["region"] = info.region;
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
        authResp["signature"] = info.signature;
        authResp["gender"] = info.gender;
        authResp["birthday"] = info.birthday;
        authResp["region"] = info.region;
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

            // 下发待处理的好友请求
            deliverPendingFriendRequests(handler);
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
                u["signature"] = info.signature;
                u["gender"] = info.gender;
                u["birthday"] = info.birthday;
                u["region"] = info.region;
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
    int myUid = m_userManager->getUidByUsername(handler->username());

    // 从数据库获取所有好友 UID（含在线和离线）
    QList<int> friendUids = m_serverDB->getFriendUids(myUid);

    // 构建在线用户快速查找表
    QSet<QString> onlineNames;
    for (ClientHandler *h : m_clients) {
        if (!h->username().isEmpty())
            onlineNames.insert(h->username());
    }

    for (int friendUid : friendUids) {
        UserInfo info = m_userManager->getUserInfo(friendUid);
        if (info.uid <= 0 || info.username.isEmpty())
            continue;

        QJsonObject u;
        u["username"] = info.username;
        u["nickname"] = info.nickname;
        u["role"] = info.role;
        u["online"] = onlineNames.contains(info.username);
        u["signature"] = info.signature;
        u["gender"] = info.gender;
        u["birthday"] = info.birthday;
        u["region"] = info.region;
        if (!info.avatarBlob.isEmpty())
            u["avatar_base64"] = QString::fromLatin1(info.avatarBlob.toBase64());
        users.append(u);
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
    QString signature = msg["signature"].toString();
    int gender = msg["gender"].toInt(-1);
    QString birthday = msg["birthday"].toString();
    QString region = msg["region"].toString();

    qDebug() << QString("[资料更新] 收到 %1 的资料更新请求: nick=%2 sig=%3 gender=%4 birthday=%5 region=%6")
                .arg(username, nickname, signature).arg(gender).arg(birthday, region);

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

    // 更新个性签名
    if (msg.contains("signature")) {
        m_userManager->updateSignature(username, signature);
    }

    // 更新性别
    if (gender >= 0) {
        m_userManager->updateGender(username, gender);
    }

    // 更新生日
    if (msg.contains("birthday")) {
        m_userManager->updateBirthday(username, birthday);
    }

    // 更新地区
    if (msg.contains("region")) {
        m_userManager->updateRegion(username, region);
    }

    // 构建广播消息，通知所有在线联系人
    UserInfo updatedInfo = m_userManager->getUserInfoByName(username);
    qDebug() << QString("[资料更新] 数据库读回 %1: nick=%2 sig=%3 gender=%4 birthday=%5 region=%6")
                .arg(username, updatedInfo.nickname, updatedInfo.signature)
                .arg(updatedInfo.gender).arg(updatedInfo.birthday, updatedInfo.region);
    QJsonObject broadcast = Protocol::makeMsg(MsgType::ProfileUpdated);
    broadcast["username"] = username;
    broadcast["nickname"] = updatedInfo.nickname;
    broadcast["signature"] = updatedInfo.signature;
    broadcast["gender"] = updatedInfo.gender;
    broadcast["birthday"] = updatedInfo.birthday;
    broadcast["region"] = updatedInfo.region;
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

void ChatServer::handleFriendRequest(ClientHandler *handler, const QJsonObject &msg)
{
    QString fromUsername = handler->username();
    QString toUsername = msg["to"].toString();
    QString text = msg["text"].toString();

    if (fromUsername.isEmpty() || toUsername.isEmpty()) {
        handler->sendMessage(Protocol::makeError("用户名不能为空"));
        return;
    }
    if (fromUsername == toUsername) {
        handler->sendMessage(Protocol::makeError("不能添加自己为好友"));
        return;
    }

    int fromUid = m_userManager->getUidByUsername(fromUsername);
    int toUid = m_userManager->getUidByUsername(toUsername);

    if (fromUid <= 0) {
        handler->sendMessage(Protocol::makeError("发送方用户不存在"));
        return;
    }
    if (toUid <= 0) {
        handler->sendMessage(Protocol::makeError("对方用户不存在"));
        return;
    }
    if (m_serverDB->areFriends(fromUid, toUid)) {
        handler->sendMessage(Protocol::makeError("你们已经是好友了"));
        return;
    }

    // 检查是否已有待处理的请求（双向），返回结构化的冲突消息
    {
        QSqlQuery checkQ(m_serverDB->database());
        checkQ.prepare(
            "SELECT id, from_uid, to_uid FROM friend_requests "
            "WHERE ((from_uid = :f AND to_uid = :t) OR (from_uid = :t AND to_uid = :f)) "
            "AND status = 0"
        );
        checkQ.bindValue(":f", fromUid);
        checkQ.bindValue(":t", toUid);
        if (checkQ.exec() && checkQ.next()) {
            int existingId = checkQ.value(0).toInt();
            int existingFrom = checkQ.value(1).toInt();

            // 构建冲突消息：告知客户端哪个方向存在待处理请求
            QJsonObject conflict = Protocol::makeMsg(MsgType::FriendRequestConflict);
            conflict["target"] = toUsername;
            if (existingFrom == fromUid) {
                // 自己已发过请求给对方
                conflict["direction"] = QString("outgoing");
                conflict["message"] = QString("你已向 %1 发送过好友请求，请等待对方处理").arg(toUsername);
            } else {
                // 对方已发过请求给自己
                conflict["direction"] = QString("incoming");
                conflict["request_id"] = existingId;
                conflict["message"] = QString("%1 已向你发送好友请求，请在好友请求中处理").arg(toUsername);
            }
            handler->sendMessage(conflict);
            qDebug() << QString("[好友请求] 冲突检测: %1(UID:%2) -> %3(UID:%4) 已有待处理请求 id=%5")
                            .arg(fromUsername).arg(fromUid).arg(toUsername).arg(toUid).arg(existingId);
            return;
        }
    }

    // 保存到 friend_requests 表（status=0 待处理），返回请求 ID
    int requestId = m_serverDB->addFriendRequest(fromUid, toUid, text);
    if (requestId < 0) {
        handler->sendMessage(Protocol::makeError("好友请求发送失败，请重试"));
        return;
    }

    qDebug() << QString("[好友请求] %1(UID:%2) -> %3(UID:%4) 请求ID:%5")
                    .arg(fromUsername).arg(fromUid).arg(toUsername).arg(toUid).arg(requestId);

    // 通知发送方：请求已发送
    QJsonObject ackMsg = Protocol::makeMsg(MsgType::FriendResponse);
    ackMsg["success"]  = true;
    ackMsg["username"] = toUsername;
    ackMsg["message"]  = "好友请求已发送，等待对方确认";
    ackMsg["sent"]     = true;   // 标记为"已发送"（非最终结果）
    handler->sendMessage(ackMsg);

    // 构建好友请求通知消息
    UserInfo fromInfo = m_userManager->getUserInfo(fromUid);
    QJsonObject requestMsg = Protocol::makeMsg(MsgType::FriendRequest);
    requestMsg["from"]       = fromUsername;
    requestMsg["text"]       = text;
    requestMsg["time"]       = QDateTime::currentMSecsSinceEpoch();
    requestMsg["request_id"] = requestId;
    requestMsg["nickname"]   = fromInfo.nickname;
    if (!fromInfo.avatarBlob.isEmpty())
        requestMsg["avatar_base64"] = QString::fromLatin1(fromInfo.avatarBlob.toBase64());

    // 目标在线 → 实时推送
    ClientHandler *target = findClient(toUsername);
    if (target) {
        target->sendMessage(requestMsg);
        qDebug() << QString("[好友请求] 已实时推送给 %1").arg(toUsername);
    } else {
        // 目标离线 → 保存为离线消息，登录时下发
        QJsonDocument doc(requestMsg);
        QString payload = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        m_serverDB->saveOfflineMessage(fromUid, toUid, payload, 2);
        qDebug() << QString("[好友请求] %1 不在线，已保存离线消息").arg(toUsername);
    }
}

void ChatServer::handleFriendResponse(ClientHandler *handler, const QJsonObject &msg)
{
    QString myUsername = handler->username();
    QString action     = msg["action"].toString();       // "accept" | "reject"
    int requestId      = msg["request_id"].toInt();
    QString toUsername = msg["to"].toString();           // 请求发起方

    if (myUsername.isEmpty() || toUsername.isEmpty()) {
        handler->sendMessage(Protocol::makeError("参数不完整"));
        return;
    }

    int myUid = m_userManager->getUidByUsername(myUsername);
    int toUid = m_userManager->getUidByUsername(toUsername);
    if (myUid <= 0 || toUid <= 0) {
        handler->sendMessage(Protocol::makeError("用户不存在"));
        return;
    }

    if (action == "accept") {
        // 验证请求存在且我是目标
        QJsonObject req = m_serverDB->getFriendRequest(requestId);
        if (req.isEmpty()) {
            handler->sendMessage(Protocol::makeError("好友请求不存在"));
            return;
        }
        if (req["to_uid"].toInt() != myUid) {
            handler->sendMessage(Protocol::makeError("无权操作此好友请求"));
            return;
        }

        // 更新请求状态 → accepted
        m_serverDB->acceptFriendRequest(requestId);

        // 建立双向好友关系
        m_serverDB->addFriendship(toUid, myUid);
        qDebug() << QString("[好友请求] %1 接受了 %2 的好友请求").arg(myUsername, toUsername);

        // 通知自己
        QJsonObject resp = Protocol::makeMsg(MsgType::FriendResponse);
        resp["success"]  = true;
        resp["username"] = toUsername;
        resp["message"]  = QString("已成功添加 %1 为好友").arg(toUsername);
        resp["accepted"] = true;
        handler->sendMessage(resp);

        // 通知对方（如果在线）
        ClientHandler *target = findClient(toUsername);
        if (target) {
            QJsonObject notify = Protocol::makeMsg(MsgType::FriendResponse);
            notify["success"]  = true;
            notify["username"] = myUsername;
            notify["message"]  = QString("%1 已接受你的好友请求").arg(myUsername);
            notify["accepted"] = true;
            target->sendMessage(notify);

            // 双方刷新联系人列表
            sendContactList(handler);
            sendContactList(target);
        } else {
            sendContactList(handler);
        }

    } else if (action == "reject") {
        // 验证请求存在且我是目标
        QJsonObject req = m_serverDB->getFriendRequest(requestId);
        if (req.isEmpty()) {
            handler->sendMessage(Protocol::makeError("好友请求不存在"));
            return;
        }
        if (req["to_uid"].toInt() != myUid) {
            handler->sendMessage(Protocol::makeError("无权操作此好友请求"));
            return;
        }

        m_serverDB->rejectFriendRequest(requestId);
        qDebug() << QString("[好友请求] %1 拒绝了 %2 的好友请求").arg(myUsername, toUsername);

        // 通知自己
        QJsonObject resp = Protocol::makeMsg(MsgType::FriendResponse);
        resp["success"]  = true;
        resp["username"] = toUsername;
        resp["message"]  = "已拒绝好友请求";
        resp["rejected"] = true;
        handler->sendMessage(resp);

        // 通知对方（如果在线）
        ClientHandler *target = findClient(toUsername);
        if (target) {
            QJsonObject notify = Protocol::makeMsg(MsgType::FriendResponse);
            notify["success"]  = false;
            notify["username"] = myUsername;
            notify["message"]  = QString("%1 拒绝了你的好友请求").arg(myUsername);
            notify["rejected"] = true;
            target->sendMessage(notify);
        }

    } else {
        handler->sendMessage(Protocol::makeError("未知操作"));
    }
}

void ChatServer::deliverPendingFriendRequests(ClientHandler *handler)
{
    QString username = handler->username();
    int uid = m_userManager->getUidByUsername(username);
    if (uid <= 0) return;

    QJsonArray pending = m_serverDB->getPendingFriendRequests(uid);
    if (pending.isEmpty()) return;

    qDebug() << QString("[好友请求] %1 有 %2 条待处理好友请求，开始下发")
                    .arg(username).arg(pending.size());

    for (const QJsonValue &val : pending) {
        QJsonObject req = val.toObject();
        int fromUid = req["from_uid"].toInt();
        UserInfo fromInfo = m_userManager->getUserInfo(fromUid);

        QJsonObject requestMsg = Protocol::makeMsg(MsgType::FriendRequest);
        requestMsg["from"]       = fromInfo.username;
        requestMsg["text"]       = req["message"].toString();
        requestMsg["time"]       = req["time"].toDouble();
        requestMsg["request_id"] = req["request_id"].toInt();
        requestMsg["nickname"]   = fromInfo.nickname;
        requestMsg["synced"]     = true;   // 标记为登录同步（非实时通知）
        if (!fromInfo.avatarBlob.isEmpty())
            requestMsg["avatar_base64"] = QString::fromLatin1(fromInfo.avatarBlob.toBase64());

        handler->sendMessage(requestMsg);
    }

    qDebug() << QString("[好友请求] %1 好友请求下发完成").arg(username);
}

// =========================================================
// 密码安全
// =========================================================

void ChatServer::handleGetSecQuestion(ClientHandler *handler, const QJsonObject &msg)
{
    QString username = msg["username"].toString();
    if (username.isEmpty()) {
        handler->sendMessage(Protocol::makeSecQuestionRes(false, QString(), "用户名不能为空"));
        return;
    }

    QJsonObject result = m_userManager->getSecurityQuestion(username);
    handler->sendMessage(Protocol::makeSecQuestionRes(
        result["success"].toBool(),
        result["question"].toString(),
        result["error"].toString()
    ));
}

void ChatServer::handleResetPassword(ClientHandler *handler, const QJsonObject &msg)
{
    QString username = msg["username"].toString();
    QString answerHash = msg["answer_hash"].toString();
    QString newPassword = msg["new_password"].toString();

    if (username.isEmpty() || answerHash.isEmpty() || newPassword.isEmpty()) {
        handler->sendMessage(Protocol::makeResetPasswordRes(false, "参数不完整"));
        return;
    }

    if (m_userManager->resetPassword(username, answerHash, newPassword)) {
        // 强制该用户下线（踢出当前 ClientHandler）
        ClientHandler *target = findClient(username);
        if (target) {
            qDebug() << QString("[密码重置] %1 密码已重置，强制下线").arg(username);
            QJsonObject kick = Protocol::makeMsg("kicked");
            kick["reason"] = "密码已重置，请使用新密码重新登录";
            target->sendMessage(kick);
            target->socket()->disconnectFromHost();
        }
        handler->sendMessage(Protocol::makeResetPasswordRes(true, "密码重置成功，请使用新密码登录"));
    } else {
        handler->sendMessage(Protocol::makeResetPasswordRes(false, "密保答案错误或重置失败"));
    }
}

void ChatServer::handleChangePassword(ClientHandler *handler, const QJsonObject &msg)
{
    QString username = handler->username();
    if (username.isEmpty()) {
        handler->sendMessage(Protocol::makeChangePasswordRes(false, "未登录"));
        return;
    }

    QString oldPassword = msg["old_password"].toString();
    QString newPassword = msg["new_password"].toString();

    if (oldPassword.isEmpty() || newPassword.isEmpty()) {
        handler->sendMessage(Protocol::makeChangePasswordRes(false, "旧密码和新密码不能为空"));
        return;
    }

    int uid = m_userManager->getUidByUsername(username);
    if (uid <= 0) {
        handler->sendMessage(Protocol::makeChangePasswordRes(false, "用户不存在"));
        return;
    }

    if (m_userManager->changePassword(uid, oldPassword, newPassword)) {
        // 强制当前客户端下线
        qDebug() << QString("[修改密码] %1 密码已修改，强制下线").arg(username);
        QJsonObject kick = Protocol::makeMsg("kicked");
        kick["reason"] = "密码已修改，请使用新密码重新登录";
        handler->sendMessage(kick);
        handler->socket()->disconnectFromHost();
    } else {
        handler->sendMessage(Protocol::makeChangePasswordRes(false, "旧密码错误或修改失败"));
    }
}
