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

    /// 离线消息：保存离线消息
    bool saveOfflineMessage(int senderUid, int receiverUid, const QString &payload, int type = 0);

    /// 离线消息：获取待下发的消息（status <= 1，不删除）
    QJsonArray getPendingOfflineMessages(int receiverUid);

    /// 离线消息：将 pending (status=0) 标记为已发送 (status=1)
    bool markOfflineMessagesAsSent(int receiverUid);

    /// 离线消息：收到客户端 ACK 后删除已确认的消息（status=1，事务保证原子性）
    bool deleteAckedOfflineMessages(int receiverUid);

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
