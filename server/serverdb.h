#ifndef SERVERDB_H
#define SERVERDB_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>

class ServerDB : public QObject
{
    Q_OBJECT
public:
    explicit ServerDB(const QString &dbDir, QObject *parent = nullptr);
    ~ServerDB();

    /// 获取主数据库连接（主线程使用）
    QSqlDatabase database() const;

    /// 为调用线程创建独立连接（线程安全）
    QSqlDatabase cloneConnection(const QString &connName);

    /// 数据库文件路径
    QString databasePath() const { return m_dbPath; }

    /// 好友关系查询：双方是否互为好友（双向 status=1）
    bool areFriends(int uid1, int uid2);

    /// 添加好友关系：插入双向 status=1 记录，返回是否成功
    bool addFriendship(int uid1, int uid2);

    /// 获取某用户所有已接受的好友 UID 列表（status=1）
    QList<int> getFriendUids(int uid);

    /// 离线消息：保存离线消息（timestamp 由调用方传入，毫秒级）
    bool saveOfflineMessage(int senderUid, int receiverUid, const QString &payload, int type, qint64 timestamp);

    /// 离线消息：获取待下发的消息（status <= 1，不删除）
    QJsonArray getPendingOfflineMessages(int receiverUid);

    /// 离线消息：将 pending (status=0) 标记为已发送 (status=1)
    bool markOfflineMessagesAsSent(int receiverUid);

    /// 离线消息：收到客户端 ACK 后删除已确认的消息（status=1，事务保证原子性）
    bool deleteAckedOfflineMessages(int receiverUid);

    /// 离线消息：撤回时删除指定消息（按发送者、接收者、时间戳匹配）
    /// 返回关联的 file_id（如果是文件消息），否则返回空字符串
    QString deleteOfflineMessageByTimestamp(int senderUid, int receiverUid, qint64 timestamp);

    // ============================================================
    // server_files 表（离线文件资源池索引）
    // ============================================================

    /// 文件记录结构体
    struct ServerFileRecord {
        QString fileId;
        QString fileName;
        qint64 fileSize;
        QString md5;
        QString storagePath;  // 相对路径，如 "2026/06/uuid.dat"
        int status;           // 0=接收中(.tmp), 1=已完成(.dat)
        qint64 createdAt;    // 秒级时间戳
    };

    /// 插入文件记录（status=0，接收中）
    bool insertServerFile(const QString &fileId, const QString &fileName,
                        qint64 fileSize, const QString &md5,
                        const QString &storagePath, qint64 createdAt);

    /// 更新文件状态（接收完成后调用，status=1，路径从 .tmp 改为 .dat）
    bool updateServerFileStatus(const QString &fileId, int status,
                               const QString &finalStoragePath);

    /// 查询文件记录，不存在返回空 QJsonObject
    QJsonObject getServerFile(const QString &fileId);

    /// 删除文件记录（同时由调用方删除物理文件）
    bool deleteServerFile(const QString &fileId);

    /// 查询超过 expireDays 天未下载的文件记录
    QJsonArray getExpiredFiles(int expireDays);

    // ---- friend_requests ----
    /// 添加好友请求，返回请求 ID（-1 表示已有待处理请求）
    int  addFriendRequest(int fromUid, int toUid, const QString &message);

    /// 获取某用户所有待处理的好友请求（to_uid=receiver, status=0）
    QJsonArray getPendingFriendRequests(int toUid);

    /// 获取单条好友请求 {from_uid, to_uid, status}，不存在返回空对象
    QJsonObject getFriendRequest(int requestId);

    /// 接受好友请求（status 0→1），成功返回 true
    bool acceptFriendRequest(int requestId);

    /// 拒绝好友请求（status 0→2），成功返回 true
    bool rejectFriendRequest(int requestId);

    // ---- 密码安全 ----
    /// 获取密保问题，返回 {success, question}，用户不存在返回 success=false
    QJsonObject getSecurityQuestion(const QString &username);

    /// 重置密码：校验答案哈希，成功则更新密码并返回 true
    bool resetPassword(const QString &username, const QString &answerHash, const QString &newPasswordHash);

    /// 修改密码：校验旧密码，成功则更新密码并返回 true
    bool changePassword(int uid, const QString &oldPasswordHash, const QString &newPasswordHash);

private:
    void initDatabase();
    void createTables();

    QString m_dbPath;
    QString m_connName;
};

#endif // SERVERDB_H
