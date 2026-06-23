#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QMap>
#include <QFile>
#include <QCryptographicHash>

struct ContactInfo {
    QString username;
    QString nickname;
    QString role;
    bool online = false;
};

struct FileTransferInfo {
    QString fileId;
    QString from;
    QString to;
    QString fileName;
    qint64 fileSize = 0;
    qint64 receivedBytes = 0;
    QFile *file = nullptr;
    QCryptographicHash *hash = nullptr;
    bool isSending = false;
    int nextSeq = 0;
};

class ChatClient : public QObject
{
    Q_OBJECT
public:
    explicit ChatClient(QObject *parent = nullptr);
    ~ChatClient();

    void connectToServer(const QString &host, quint16 port = 9527);
    void disconnectFromServer();
    bool isConnected() const;

    void sendRegister(const QString &username, const QString &password, const QString &role, const QString &nickname);
    void sendLogin(const QString &username, const QString &password);

    void sendTextMessage(const QString &to, const QString &text);
    void sendFile(const QString &to, const QString &filePath);
    void acceptFile(const QString &fileId);
    void rejectFile(const QString &fileId, const QString &reason = "用户拒绝");

    QString myUsername() const { return m_username; }
    QString myRole() const { return m_role; }
    QString myNickname() const { return m_myNickname; }
    QMap<QString, ContactInfo> contacts() const { return m_contacts; }
    bool isTransferActive(const QString &fileId) const;

signals:
    void connected();
    void connectionFailed(const QString &error);
    void disconnected();

    void authResult(bool success, const QString &message, const QString &role);
    void contactListUpdated(const QMap<QString, ContactInfo> &contacts);
    void textMessageReceived(const QString &from, const QString &to, const QString &text, qint64 timestamp);
    void messageAck(const QString &to, qint64 timestamp);

    void fileOfferReceived(const QString &from, const QString &fileName, qint64 fileSize, const QString &fileId);
    void fileAccepted(const QString &fileId);
    void fileRejected(const QString &fileId, const QString &reason);
    void fileProgress(const QString &fileId, qint64 receivedBytes, qint64 totalBytes);
    void fileCompleted(const QString &fileId, const QString &savePath);
    void fileError(const QString &fileId, const QString &error);
    void fileSizeExceeded(const QString &fileId, const QString &fileName, qint64 fileSize);
    void fileSendInitiated(const QString &to, const QString &fileName, qint64 fileSize, const QString &fileId);
    void fileSendCompleted(const QString &fileId);

    void serverError(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError err);

private:
    void processMessage(const QJsonObject &msg);
    void handleAuthResult(const QJsonObject &msg);
    void handleContactList(const QJsonObject &msg);
    void handleOnlineStatus(const QJsonObject &msg);
    void handleMessage(const QJsonObject &msg);
    void handleMessageAck(const QJsonObject &msg);
    void handleFileOffer(const QJsonObject &msg);
    void handleFileAccept(const QJsonObject &msg);
    void handleFileReject(const QJsonObject &msg);
    void handleFileData(const QJsonObject &msg);
    void handleFileEnd(const QJsonObject &msg);
    void handleError(const QJsonObject &msg);

    void sendJson(const QJsonObject &obj);
    void continueSendingFile(const QString &fileId);

    QTcpSocket *m_socket;
    QByteArray m_buffer;
    QString m_username;
    QString m_role;
    QString m_myNickname;
    QMap<QString, ContactInfo> m_contacts;
    QMap<QString, FileTransferInfo> m_fileTransfers;

    // 文件发送相关
    QMap<QString, QString> m_pendingSendFiles; // fileId -> filePath
};

#endif // CHATCLIENT_H
