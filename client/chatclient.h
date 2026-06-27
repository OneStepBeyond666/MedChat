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
    QByteArray avatarData;  // 头像原始字节（PNG/JPEG），从 base64 解码
    QString signature;      // 个性签名
    int gender = 0;         // 0=未知, 1=男, 2=女
    QString birthday;      // YYYY-MM-DD
    QString region;        // 地区
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
    // 离线文件相关
    bool isOffline = false;
    int expireDays = -1;  // -1 表示非离线文件
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

    void sendRegister(const QString &username, const QString &password, const QString &role, const QString &nickname,
                       const QString &secQuestion = QString(), const QString &secAnswerHash = QString());
    void sendLogin(const QString &username, const QString &password);

    void sendTextMessage(const QString &to, const QString &text);
    void sendRecallMessage(const QString &to, qint64 originalTimestamp, int msgType);
    void sendFile(const QString &to, const QString &filePath);
    void acceptFile(const QString &fileId);
    void rejectFile(const QString &fileId, const QString &reason = "用户拒绝");

    /// 发送资料更新请求（昵称和/或头像）
    void sendProfileUpdate(const QString &nickname, const QByteArray &avatarData,
                            const QString &signature = QString(),
                            int gender = -1,
                            const QString &birthday = QString(),
                            const QString &region = QString());

    /// 发送好友请求
    void sendFriendRequest(const QString &to);

    /// 发送好友请求响应（接受/拒绝）
    void sendFriendResponse(int requestId, const QString &to, bool accept);

    /// 发送密保问题查询请求
    void sendGetSecQuestion(const QString &username);

    /// 发送重置密码请求
    void sendResetPassword(const QString &username, const QString &answerHash, const QString &newPassword);

    /// 发送修改密码请求
    void sendChangePassword(const QString &oldPassword, const QString &newPassword);

    QString myUsername() const { return m_username; }
    QString myRole() const { return m_role; }
    QString myNickname() const { return m_myNickname; }
    QByteArray myAvatarData() const { return m_myAvatarData; }
    QString mySignature() const { return m_mySignature; }
    int myGender() const { return m_myGender; }
    QString myBirthday() const { return m_myBirthday; }
    QString myRegion() const { return m_myRegion; }
    QMap<QString, ContactInfo> contacts() const { return m_contacts; }
    QMap<QString, ContactInfo> onlineUsers() const { return m_onlineUsers; }
    bool isTransferActive(const QString &fileId) const;

signals:
    void connected();
    void connectionFailed(const QString &error);
    void disconnected();

    void authResult(bool success, const QString &message, const QString &role, const QByteArray &avatarData);
    void contactListUpdated(const QMap<QString, ContactInfo> &contacts);
    void contactProfileChanged(const QString &username, const QString &nickname, const QByteArray &avatarData,
                                const QString &signature = QString(), int gender = 0,
                                const QString &birthday = QString(), const QString &region = QString());
    void textMessageReceived(const QString &from, const QString &to, const QString &text, qint64 timestamp);
    void messageAck(const QString &to, qint64 timestamp);
    void messageRecalled(const QString &from, const QString &to, qint64 originalTimestamp, int msgType);

    void fileOfferReceived(const QString &from, const QString &fileName, qint64 fileSize,
                            const QString &fileId, bool isOffline = false, int expireDays = -1);
    void fileAccepted(const QString &fileId);
    void fileRejected(const QString &fileId, const QString &reason);
    void fileProgress(const QString &fileId, qint64 receivedBytes, qint64 totalBytes);
    void fileCompleted(const QString &fileId, const QString &savePath);
    void fileError(const QString &fileId, const QString &error);
    void fileSizeExceeded(const QString &fileId, const QString &fileName, qint64 fileSize);
    void fileSendInitiated(const QString &to, const QString &fileName, qint64 fileSize, const QString &fileId);
    void fileSendCompleted(const QString &fileId);

    void serverError(const QString &error);
    void offlineSyncDone();                       // 离线消息同步完成
    void strangerError(const QString &text);      // 陌生人拦截提示
    void friendRequestReceived(int requestId, const QString &from, const QString &nickname,
                               const QString &text, const QByteArray &avatarData, bool isSynced);
    void friendResponseReceived(bool success, const QString &username, const QString &message);
    void friendRequestCountChanged(int count);    // 待处理好友请求数量变化
    void friendRequestConflict(const QString &target, const QString &direction,
                               const QString &message, int requestId);
    void onlineUsersUpdated(const QMap<QString, ContactInfo> &onlineUsers);

    // 密码安全
    void secQuestionReceived(bool success, const QString &question, const QString &error);
    void resetPasswordResult(bool success, const QString &message);
    void changePasswordResult(bool success, const QString &message);
    void kicked(const QString &reason);  // 被服务端强制下线

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
    void handleRecallMessage(const QJsonObject &msg);
    void handleFileOffer(const QJsonObject &msg);
    void handleFileAccept(const QJsonObject &msg);
    void handleFileReject(const QJsonObject &msg);
    void handleFileData(const QJsonObject &msg);
    void handleFileEnd(const QJsonObject &msg);
    void handleFileOfflineCached(const QJsonObject &msg);
    void handleError(const QJsonObject &msg);
    void handleOfflineSync(const QJsonObject &msg);
    void handleStrangerError(const QJsonObject &msg);
    void handleFriendRequest(const QJsonObject &msg);
    void handleFriendResponse(const QJsonObject &msg);
    void handleFriendRequestConflict(const QJsonObject &msg);
    void handleProfileUpdated(const QJsonObject &msg);
    void handleSecQuestionRes(const QJsonObject &msg);
    void handleResetPasswordRes(const QJsonObject &msg);
    void handleChangePasswordRes(const QJsonObject &msg);
    void handleKicked(const QJsonObject &msg);

    /// 离线消息去重：检查本地 DB 是否已有同条消息
    bool isDuplicateOfflineMessage(const QString &from, const QString &content, qint64 timestamp);

    void sendJson(const QJsonObject &obj);
    void continueSendingFile(const QString &fileId);

    QTcpSocket *m_socket;
    QByteArray m_buffer;
    QString m_username;
    QString m_role;
    QString m_myNickname;
    QByteArray m_myAvatarData;
    QString m_mySignature;
    int m_myGender = 0;
    QString m_myBirthday;
    QString m_myRegion;
    QMap<QString, ContactInfo> m_contacts;      // 仅好友
    QMap<QString, ContactInfo> m_onlineUsers;   // 所有在线用户（含好友和陌生人）
    QMap<QString, FileTransferInfo> m_fileTransfers;

    // 文件发送相关
    QMap<QString, QString> m_pendingSendFiles; // fileId -> filePath
};

#endif // CHATCLIENT_H
