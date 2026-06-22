#include "chatclient.h"
#include "common/protocol.h"
#include <QDebug>
#include <QJsonArray>
#include <QFileInfo>
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

void ChatClient::sendRegister(const QString &username, const QString &password, const QString &role)
{
    QJsonObject obj = Protocol::makeMsg(MsgType::Register);
    obj["username"] = username;
    obj["password"] = password;
    obj["role"] = role;
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
}

void ChatClient::acceptFile(const QString &fileId, const QString &savePath)
{
    auto it = m_fileTransfers.find(fileId);
    if (it == m_fileTransfers.end()) return;

    it->file = new QFile(savePath);
    if (!it->file->open(QIODevice::WriteOnly)) {
        emit fileError(fileId, "无法创建文件: " + savePath);
        delete it->file;
        it->file = nullptr;
        return;
    }
    it->hash = new QCryptographicHash(QCryptographicHash::Md5);
    m_pendingReceiveFiles[fileId] = savePath;

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
    else
        qWarning() << "[Client] Unknown message type:" << type;
}

void ChatClient::handleAuthResult(const QJsonObject &msg)
{
    bool success = msg["success"].toBool();
    QString message = msg["message"].toString();
    QString role = msg["role"].toString();
    if (success) {
        m_role = role;
    }
    emit authResult(success, message, role);
}

void ChatClient::handleContactList(const QJsonObject &msg)
{
    m_contacts.clear();
    QJsonArray arr = msg["contacts"].toArray();
    for (const QJsonValue &val : arr) {
        QJsonObject u = val.toObject();
        ContactInfo ci;
        ci.username = u["username"].toString();
        ci.role = u["role"].toString();
        ci.online = u["online"].toBool();
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
        } else {
            ContactInfo ci;
            ci.username = username;
            ci.role = u["role"].toString();
            ci.online = true;
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
        emit fileProgress(fileId, it->receivedBytes, it->fileSize);
    }
}

void ChatClient::handleFileEnd(const QJsonObject &msg)
{
    QString fileId = msg["file_id"].toString();
    QString md5 = msg["md5"].toString();
    auto it = m_fileTransfers.find(fileId);
    if (it == m_fileTransfers.end()) return;

    QString savePath;
    if (m_pendingReceiveFiles.contains(fileId)) {
        savePath = m_pendingReceiveFiles[fileId];
    }

    if (it->file) {
        it->file->close();
        // 校验 MD5
        if (it->hash && !md5.isEmpty()) {
            QString localMd5 = QString::fromLatin1(it->hash->result().toHex());
            if (localMd5 != md5) {
                emit fileError(fileId, "文件校验失败: MD5 不匹配");
                delete it->file;
                delete it->hash;
                m_fileTransfers.erase(it);
                return;
            }
        }
        delete it->file;
        if (it->hash) delete it->hash;
    }
    m_fileTransfers.erase(it);
    emit fileCompleted(fileId, savePath);
}

void ChatClient::handleError(const QJsonObject &msg)
{
    QString text = msg["text"].toString();
    emit serverError(text);
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
    const int chunkSize = 65536; // 64KB

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
}
