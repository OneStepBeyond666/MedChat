#include "chatclient.h"
#include "localdb.h"
#include "common/protocol.h"
#include "common/constants.h"
#include <QDebug>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QDate>
#include <QUuid>
#include <QNetworkProxy>

ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    // 强制不走系统代理，避免 Clash/VPN 等代理软件干扰局域网连接
    m_socket->setProxy(QNetworkProxy::NoProxy);
    connect(m_socket, &QTcpSocket::connected, this, &ChatClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &ChatClient::onError);
}

ChatClient::~ChatClient()
{
    disconnectFromServer();
    for (auto it = m_fileTransfers.begin(); it != m_fileTransfers.end(); ++it) {
        if (it->file) {
            it->file->close();
            delete it->file;
        }
        if (it->hash)
            delete it->hash;
    }
}

void ChatClient::connectToServer(const QString &host, quint16 port)
{
    m_socket->connectToHost(host, port);
}

void ChatClient::disconnectFromServer()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

bool ChatClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void ChatClient::sendRegister(const QString &username, const QString &password, const QString &role, const QString &nickname)
{
    QJsonObject obj = Protocol::makeMsg(MsgType::Register);
    obj["username"] = username;
    obj["password"] = password;
    obj["role"] = role;
    obj["nickname"] = nickname;
    sendJson(obj);
}

void ChatClient::sendLogin(const QString &username, const QString &password)
{
    QJsonObject obj = Protocol::makeMsg(MsgType::Login);
    obj["username"] = username;
    obj["password"] = password;
    sendJson(obj);
}

void ChatClient::sendTextMessage(const QString &to, const QString &text)
{
    QJsonObject obj = Protocol::makeMessage(m_username, to, text);
    sendJson(obj);
}

void ChatClient::sendFile(const QString &to, const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        emit fileError("", "文件不存在: " + filePath);
        return;
    }

    if (fi.size() > Constants::MAX_FILE_SIZE) {
        emit fileError("", QString("文件 \"%1\" 超过100MB传输限制").arg(fi.fileName()));
        return;
    }

    QString fileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject offer = Protocol::makeFileOffer(m_username, to, fi.fileName(), fi.size(), fileId);
    sendJson(offer);

    m_pendingSendFiles[fileId] = filePath;

    FileTransferInfo info;
    info.fileId = fileId;
    info.from = m_username;
    info.to = to;
    info.fileName = fi.fileName();
    info.fileSize = fi.size();
    info.isSending = true;
    m_fileTransfers[fileId] = info;

    emit fileSendInitiated(to, fi.fileName(), fi.size(), fileId);
}

bool ChatClient::isTransferActive(const QString &fileId) const
{
    auto it = m_fileTransfers.constFind(fileId);
    if (it == m_fileTransfers.constEnd())
        return false;
    return it->file != nullptr; // acceptFile() 后才非空
}

void ChatClient::acceptFile(const QString &fileId)
{
    auto it = m_fileTransfers.find(fileId);
    if (it == m_fileTransfers.end()) return;

    // 在临时目录下创建 {fileId}.tmp
    QString tmpPath = LocalDB::instance().tempDir() + "/" + fileId + ".tmp";
    it->file = new QFile(tmpPath);
    if (!it->file->open(QIODevice::WriteOnly)) {
        emit fileError(fileId, "无法创建临时文件: " + tmpPath);
        delete it->file;
        it->file = nullptr;
        return;
    }
    it->hash = new QCryptographicHash(QCryptographicHash::Md5);

    // 写入 file_index 记录（status=0 接收中）
    FileRecord rec;
    rec.fileId = fileId;
    rec.originalName = it->fileName;
    rec.savePath = tmpPath;
    rec.size = it->fileSize;
    rec.status = 0;
    LocalDB::instance().insertFileRecord(rec);

    QJsonObject accept = Protocol::makeFileAccept(m_username, it->from, fileId);
    sendJson(accept);
}

void ChatClient::rejectFile(const QString &fileId, const QString &reason)
{
    auto it = m_fileTransfers.find(fileId);
    if (it == m_fileTransfers.end()) return;

    QJsonObject reject = Protocol::makeFileReject(m_username, it->from, fileId, reason);
    sendJson(reject);
}

void ChatClient::onConnected()
{
    qDebug() << "[Client] Connected to server.";
    emit connected();
}

void ChatClient::onDisconnected()
{
    qDebug() << "[Client] Disconnected from server.";
    emit disconnected();
}

void ChatClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    bool ok = false;
    QJsonObject obj;
    while (!(obj = Protocol::decode(m_buffer, ok)).isEmpty() && ok) {
        processMessage(obj);
    }
}

void ChatClient::onError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err)
    emit connectionFailed(m_socket->errorString());
}

void ChatClient::processMessage(const QJsonObject &msg)
{
    QString type = msg["type"].toString();

    if (type == MsgType::AuthResult)       handleAuthResult(msg);
    else if (type == MsgType::ContactList)  handleContactList(msg);
    else if (type == MsgType::OnlineStatus) handleOnlineStatus(msg);
    else if (type == MsgType::Message)      handleMessage(msg);
    else if (type == "message_ack")         handleMessageAck(msg);
    else if (type == MsgType::FileOffer)    handleFileOffer(msg);
    else if (type == MsgType::FileAccept)   handleFileAccept(msg);
    else if (type == MsgType::FileReject)   handleFileReject(msg);
    else if (type == MsgType::FileData)     handleFileData(msg);
    else if (type == MsgType::FileEnd)      handleFileEnd(msg);
    else if (type == MsgType::Error)        handleError(msg);
    else if (type == MsgType::OfflineSync)  handleOfflineSync(msg);
    else if (type == MsgType::ErrorStranger) handleStrangerError(msg);
    else if (type == MsgType::FriendRequest) handleFriendRequest(msg);
    else if (type == MsgType::FriendResponse) handleFriendResponse(msg);
    else if (type == MsgType::ProfileUpdated) handleProfileUpdated(msg);
    else
        qWarning() << "[Client] Unknown message type:" << type;
}

void ChatClient::handleAuthResult(const QJsonObject &msg)
{
    bool success = msg["success"].toBool();
    QString message = msg["message"].toString();
    QString role = msg["role"].toString();
    QByteArray avatarData;
    if (success) {
        m_role = role;
        m_myNickname = msg["nickname"].toString();
        if (m_myNickname.isEmpty())
            m_myNickname = m_username;
        // 解析头像
        QString avatarB64 = msg["avatar_base64"].toString();
        if (!avatarB64.isEmpty()) {
            avatarData = QByteArray::fromBase64(avatarB64.toLatin1());
            m_myAvatarData = avatarData;
        }
    }
    emit authResult(success, message, role, avatarData);
}

void ChatClient::handleContactList(const QJsonObject &msg)
{
    m_contacts.clear();
    QJsonArray arr = msg["contacts"].toArray();
    for (const QJsonValue &val : arr) {
        QJsonObject u = val.toObject();
        ContactInfo ci;
        ci.username = u["username"].toString();
        ci.nickname = u["nickname"].toString();
        if (ci.nickname.isEmpty()) ci.nickname = ci.username;
        ci.role = u["role"].toString();
        ci.online = u["online"].toBool();
        // 解析头像
        QString avatarB64 = u["avatar_base64"].toString();
        if (!avatarB64.isEmpty())
            ci.avatarData = QByteArray::fromBase64(avatarB64.toLatin1());
        m_contacts[ci.username] = ci;
    }
    emit contactListUpdated(m_contacts);
}

void ChatClient::handleOnlineStatus(const QJsonObject &msg)
{
    QJsonArray arr = msg["users"].toArray();
    // 更新现有联系人在线状态
    for (const QJsonValue &val : arr) {
        QJsonObject u = val.toObject();
        QString username = u["username"].toString();
        if (m_contacts.contains(username)) {
            m_contacts[username].online = true;
            QString nick = u["nickname"].toString();
            if (!nick.isEmpty())
                m_contacts[username].nickname = nick;
            // 更新头像
            QString avatarB64 = u["avatar_base64"].toString();
            if (!avatarB64.isEmpty())
                m_contacts[username].avatarData = QByteArray::fromBase64(avatarB64.toLatin1());
        } else {
            ContactInfo ci;
            ci.username = username;
            ci.nickname = u["nickname"].toString();
            if (ci.nickname.isEmpty()) ci.nickname = ci.username;
            ci.role = u["role"].toString();
            ci.online = true;
            QString avatarB64 = u["avatar_base64"].toString();
            if (!avatarB64.isEmpty())
                ci.avatarData = QByteArray::fromBase64(avatarB64.toLatin1());
            m_contacts[username] = ci;
        }
    }
    // 不在列表中的联系人设为离线
    QStringList onlineNames;
    for (const QJsonValue &val : arr) {
        onlineNames << val.toObject()["username"].toString();
    }
    for (auto it = m_contacts.begin(); it != m_contacts.end(); ++it) {
        if (!onlineNames.contains(it.key())) {
            it->online = false;
        }
    }
    emit contactListUpdated(m_contacts);
}

void ChatClient::handleMessage(const QJsonObject &msg)
{
    QString from = msg["from"].toString();
    QString to = msg["to"].toString();
    QString text = msg["text"].toString();
    qint64 time = static_cast<qint64>(msg["time"].toDouble());

    // 离线消息去重：如果本地 DB 已有同条消息则跳过
    if (msg.contains("offline") && msg["offline"].toBool()) {
        if (isDuplicateOfflineMessage(from, text, time)) {
            qDebug() << "[Client] 离线消息去重跳过: from=" << from << "time=" << time;
            return;
        }
    }

    emit textMessageReceived(from, to, text, time);
}

void ChatClient::handleMessageAck(const QJsonObject &msg)
{
    QString to = msg["to"].toString();
    qint64 time = static_cast<qint64>(msg["time"].toDouble());
    emit messageAck(to, time);
}

void ChatClient::handleFileOffer(const QJsonObject &msg)
{
    QString from = msg["from"].toString();
    QString fileName = msg["file_name"].toString();
    qint64 fileSize = static_cast<qint64>(msg["file_size"].toDouble());
    QString fileId = msg["file_id"].toString();

    // 接收端 100MB 限制校验
    if (fileSize > Constants::MAX_FILE_SIZE) {
        emit fileSizeExceeded(fileId, fileName, fileSize);
        // 自动拒绝超限文件
        QJsonObject reject = Protocol::makeFileReject(m_username, from, fileId, "文件超过100MB传输限制");
        sendJson(reject);
        return;
    }

    FileTransferInfo info;
    info.fileId = fileId;
    info.from = from;
    info.to = m_username;
    info.fileName = fileName;
    info.fileSize = fileSize;
    info.isSending = false;
    m_fileTransfers[fileId] = info;

    emit fileOfferReceived(from, fileName, fileSize, fileId);
}

void ChatClient::handleFileAccept(const QJsonObject &msg)
{
    QString fileId = msg["file_id"].toString();
    emit fileAccepted(fileId);
    // 开始发送文件数据
    continueSendingFile(fileId);
}

void ChatClient::handleFileReject(const QJsonObject &msg)
{
    QString fileId = msg["file_id"].toString();
    QString reason = msg["reason"].toString();
    emit fileRejected(fileId, reason);
    // 清理
    if (m_pendingSendFiles.contains(fileId))
        m_pendingSendFiles.remove(fileId);
}

void ChatClient::handleFileData(const QJsonObject &msg)
{
    QString fileId = msg["file_id"].toString();
    auto it = m_fileTransfers.find(fileId);
    if (it == m_fileTransfers.end()) return;

    QString b64 = msg["data"].toString();
    QByteArray chunk = QByteArray::fromBase64(b64.toLatin1());

    if (it->file && it->file->isOpen()) {
        it->file->write(chunk);
        if (it->hash)
            it->hash->addData(chunk);
        it->receivedBytes += chunk.size();

        // 累计大小超限检查
        if (it->receivedBytes > Constants::MAX_FILE_SIZE) {
            it->file->close();
            QString tmpPath = it->file->fileName();
            delete it->file;
            if (it->hash) delete it->hash;
            QFile::remove(tmpPath);
            LocalDB::instance().updateFileRecord(fileId, 2); // 标记失败
            m_fileTransfers.erase(it);
            emit fileError(fileId, "文件接收超过100MB限制，已中止");
            return;
        }

        emit fileProgress(fileId, it->receivedBytes, it->fileSize);
    }
}

void ChatClient::handleFileEnd(const QJsonObject &msg)
{
    QString fileId = msg["file_id"].toString();
    QString md5 = msg["md5"].toString();
    auto it = m_fileTransfers.find(fileId);
    if (it == m_fileTransfers.end()) return;

    QString tmpPath;
    if (it->file) {
        tmpPath = it->file->fileName();
        it->file->close();

        // 校验 MD5
        if (it->hash && !md5.isEmpty()) {
            QString localMd5 = QString::fromLatin1(it->hash->result().toHex());
            if (localMd5 != md5) {
                // 校验失败：删除临时文件，标记失败
                delete it->file;
                if (it->hash) delete it->hash;
                QFile::remove(tmpPath);
                LocalDB::instance().updateFileRecord(fileId, 2); // status=失败
                m_fileTransfers.erase(it);
                emit fileError(fileId, "文件校验失败: MD5 不匹配");
                return;
            }
        }
        delete it->file;
        if (it->hash) delete it->hash;
    }

    // MD5 校验通过：从 temp 移动到正式目录
    QString rootPath = LocalDB::instance().rootPath();
    QDate today = QDate::currentDate();
    QString yearMonth = QString("%1/%2")
                        .arg(today.year())
                        .arg(today.month(), 2, 10, QChar('0'));
    QString targetDir = rootPath + "/" + Constants::FILE_SUBDIR + "/" + yearMonth;

    // 确保目标目录存在
    QDir().mkpath(targetDir);

    // 生成不冲突的文件名
    QString finalPath = LocalDB::instance().generateUniqueFilePath(targetDir, it->fileName);
    QFileInfo finalFi(finalPath);
    QString saveName = finalFi.fileName();

    // 移动文件
    if (!QFile::rename(tmpPath, finalPath)) {
        qWarning() << "[ChatClient] 文件移动失败:" << tmpPath << "->" << finalPath;
        // 回退：尝试复制+删除
        QFile::copy(tmpPath, finalPath);
        QFile::remove(tmpPath);
    }

    // 计算实际 MD5（如果发送方未提供）
    QString actualMd5 = md5;

    // 更新 file_index：status=1(完成), 最终路径, md5
    LocalDB::instance().updateFileRecord(fileId, 1, finalPath, actualMd5);

    m_fileTransfers.erase(it);
    emit fileCompleted(fileId, finalPath);
}

void ChatClient::handleError(const QJsonObject &msg)
{
    QString text = msg["text"].toString();
    emit serverError(text);
}

void ChatClient::handleOfflineSync(const QJsonObject &msg)
{
    bool done = msg["done"].toBool();
    if (done) {
        qDebug() << "[Client] 离线消息同步完成，发送ACK";
        // 回传 ACK 给服务端，触发删除 status=1 的离线消息
        QJsonObject ackMsg = Protocol::makeMsg(MsgType::OfflineSyncAck);
        ackMsg["count"] = msg["count"].toInt();  // 可选：携带消息数量供服务端校验
        sendJson(ackMsg);
        emit offlineSyncDone();
    }
}

void ChatClient::handleStrangerError(const QJsonObject &msg)
{
    QString text = msg["text"].toString();
    qDebug() << "[Client] 陌生人拦截:" << text;
    emit strangerError(text);
}

void ChatClient::handleFriendRequest(const QJsonObject &msg)
{
    QString from = msg["from"].toString();
    QString text = msg["text"].toString();
    qint64 time = static_cast<qint64>(msg["time"].toDouble());
    int requestId = msg["request_id"].toInt();
    QString nickname = msg["nickname"].toString();
    bool isSynced = msg.contains("synced") && msg["synced"].toBool();

    // 离线好友请求去重
    if (msg.contains("offline") && msg["offline"].toBool()) {
        // 检查本地是否已存储该请求
        QVector<FriendRequestInfo> existing = LocalDB::instance().loadPendingFriendRequests();
        for (const FriendRequestInfo &r : existing) {
            if (r.requestId == requestId) {
                qDebug() << "[Client] 离线好友请求去重跳过: from=" << from << "id=" << requestId;
                return;
            }
        }
    }

    // 解析头像
    QByteArray avatarData;
    QString avatarB64 = msg["avatar_base64"].toString();
    if (!avatarB64.isEmpty())
        avatarData = QByteArray::fromBase64(avatarB64.toLatin1());

    // 持久化到本地 friend_requests 表
    FriendRequestInfo info;
    info.requestId    = requestId;
    info.fromUsername = from;
    info.nickname     = nickname.isEmpty() ? from : nickname;
    info.message      = text;
    info.timestamp    = time;
    info.avatarData   = avatarData;
    info.status       = 0;
    LocalDB::instance().insertFriendRequest(info);

    qDebug() << "[Client] 收到好友请求:" << from << "id=" << requestId;
    emit friendRequestReceived(requestId, from, nickname, text, avatarData, isSynced);
    emit friendRequestCountChanged(LocalDB::instance().getPendingFriendRequestCount());
}

void ChatClient::handleFriendResponse(const QJsonObject &msg)
{
    bool success = msg["success"].toBool();
    QString username = msg["username"].toString();
    QString message = msg["message"].toString();
    bool accepted = msg.contains("accepted") && msg["accepted"].toBool();
    bool rejected = msg.contains("rejected") && msg["rejected"].toBool();
    bool sent     = msg.contains("sent") && msg["sent"].toBool();

    qDebug() << "[Client] 好友请求响应:" << success << username << message
             << "accepted=" << accepted << "rejected=" << rejected << "sent=" << sent;

    // 如果对方接受了请求，更新本地好友请求状态
    if (accepted) {
        // 查找本地待处理请求中来自该用户的请求，标记为已接受
        QVector<FriendRequestInfo> pending = LocalDB::instance().loadPendingFriendRequests();
        for (const FriendRequestInfo &r : pending) {
            if (r.fromUsername == username) {
                LocalDB::instance().updateFriendRequestStatus(r.requestId, 1);
                break;
            }
        }
        emit friendRequestCountChanged(LocalDB::instance().getPendingFriendRequestCount());
    }

    emit friendResponseReceived(success, username, message);
}

void ChatClient::sendProfileUpdate(const QString &nickname, const QByteArray &avatarData)
{
    QJsonObject obj = Protocol::makeMsg(MsgType::UpdateProfile);
    obj["nickname"] = nickname;
    if (!avatarData.isEmpty())
        obj["avatar_base64"] = QString::fromLatin1(avatarData.toBase64());
    else
        obj["avatar_base64"] = QString();
    sendJson(obj);
}

void ChatClient::sendFriendRequest(const QString &to)
{
    QJsonObject obj = Protocol::makeMsg(MsgType::FriendRequest);
    obj["to"] = to;
    obj["text"] = "请求添加你为好友";
    sendJson(obj);
}

void ChatClient::sendFriendResponse(int requestId, const QString &to, bool accept)
{
    QJsonObject obj = Protocol::makeMsg(MsgType::FriendResponse);
    obj["action"]    = accept ? "accept" : "reject";
    obj["request_id"] = requestId;
    obj["to"]        = to;
    sendJson(obj);

    // 立即更新本地状态
    LocalDB::instance().updateFriendRequestStatus(requestId, accept ? 1 : 2);
    emit friendRequestCountChanged(LocalDB::instance().getPendingFriendRequestCount());
}

void ChatClient::handleProfileUpdated(const QJsonObject &msg)
{
    QString username = msg["username"].toString();
    QString nickname = msg["nickname"].toString();
    QString avatarB64 = msg["avatar_base64"].toString();
    QByteArray avatarData;
    if (!avatarB64.isEmpty())
        avatarData = QByteArray::fromBase64(avatarB64.toLatin1());

    // 更新本地联系人缓存
    if (m_contacts.contains(username)) {
        if (!nickname.isEmpty())
            m_contacts[username].nickname = nickname;
        if (!avatarData.isEmpty())
            m_contacts[username].avatarData = avatarData;
    }

    // 如果是自己的资料变更，更新自身信息
    if (username == m_username) {
        if (!nickname.isEmpty())
            m_myNickname = nickname;
        if (!avatarData.isEmpty())
            m_myAvatarData = avatarData;
    }

    emit contactProfileChanged(username, nickname, avatarData);
}

bool ChatClient::isDuplicateOfflineMessage(const QString &from, const QString &content, qint64 timestamp)
{
    // 查本地 DB：同联系人 + 同 timestamp + 同 content → 重复
    QVector<StoredMessage> msgs = LocalDB::instance().loadMessages(from, 500);
    for (const StoredMessage &m : msgs) {
        if (m.timestamp == timestamp && m.content == content && !m.isMine) {
            return true;
        }
    }
    return false;
}

void ChatClient::sendJson(const QJsonObject &obj)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(Protocol::encode(obj));
        m_socket->flush();
    }
}

void ChatClient::continueSendingFile(const QString &fileId)
{
    if (!m_pendingSendFiles.contains(fileId)) return;
    QString filePath = m_pendingSendFiles[fileId];

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit fileError(fileId, "无法打开文件: " + filePath);
        return;
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    int seq = 0;
    const int chunkSize = Constants::FILE_CHUNK_SIZE; // 64KB

    auto it = m_fileTransfers.find(fileId);
    QString to = (it != m_fileTransfers.end()) ? it->to : QString();

    while (!file.atEnd()) {
        QByteArray chunk = file.read(chunkSize);
        hash.addData(chunk);

        QJsonObject dataMsg = Protocol::makeFileData(fileId, chunk, seq++);
        dataMsg["to"] = to;
        sendJson(dataMsg);

        qint64 pos = file.pos();
        qint64 total = file.size();
        emit fileProgress(fileId, pos, total);
    }

    // 发送结束
    QString md5 = QString::fromLatin1(hash.result().toHex());
    QJsonObject endMsg = Protocol::makeFileEnd(fileId, md5);
    endMsg["to"] = to;
    sendJson(endMsg);

    m_pendingSendFiles.remove(fileId);
    file.close();

    // 清理发送方的 transfer 条目并通知 UI 发送完成
    m_fileTransfers.remove(fileId);
    emit fileSendCompleted(fileId);
}
