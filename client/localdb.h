#ifndef LOCALDB_H
#define LOCALDB_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QByteArray>
#include <QVector>

// ============================================================
// 客户端本地存储数据结构
// ============================================================

struct SessionInfo {
    QString contactUid;      // 联系人 username
    QString username;        // 用户名
    QString lastMsgPreview;  // 最后一条消息预览（前50字）
    qint64  lastTime = 0;    // 最后消息时间戳 (ms)
    int     unreadCount = 0; // 未读消息数
    // 运行时合并字段（不存储在数据库中，由 MainWindow 从 contacts 合并）
    QString nickname;
    QByteArray avatarData;
};

struct FileRecord {
    QString fileId;          // 文件 ID（协议层 UUID）
    QString originalName;    // 原始文件名
    QString saveName;        // 实际保存的文件名（可能带 (1) 后缀）
    QString savePath;        // 绝对路径
    qint64  size = 0;
    QString md5;
    QString yearMonth;       // "2026/06"
    int     status = 0;      // 0=接收中, 1=完成, 2=失败
};

struct StoredMessage {
    qint64  msgId = 0;
    QString contactUid;      // 对话方 username
    int     type = 0;        // 0=文本, 1=文件
    QString content;         // 文本内容 或 文件名
    QString fileId;          // 文件消息关联 ID（文本消息为空）
    qint64  timestamp = 0;
    bool    isMine = false;
};

struct FriendRequestInfo {
    int      requestId = 0;
    QString  fromUsername;
    QString  nickname;
    QString  message;
    qint64   timestamp = 0;
    QByteArray avatarData;
    int      status = 0;     // 0=pending, 1=accepted, 2=rejected
};

// ============================================================
// LocalDB 单例 —— 客户端本地存储管理
// ============================================================

class LocalDB : public QObject
{
    Q_OBJECT
public:
    static LocalDB &instance();

    /// 初始化：创建目录结构 + 打开两个数据库 + 建表
    bool init(const QString &uid);

    /// 关闭数据库连接
    void close();

    /// 根目录路径：{appDir}/medchat_file/{uid}
    QString rootPath() const { return m_rootPath; }

    /// 临时文件目录：{root}/file/temp
    QString tempDir() const;

    // ---- meta.db: sessions ----
    void upsertSession(const SessionInfo &s);
    QVector<SessionInfo> loadSessions();
    void clearUnread(const QString &contactUid);
    void incrementUnread(const QString &contactUid);

    // ---- meta.db: file_index ----
    void insertFileRecord(const FileRecord &r);
    void updateFileRecord(const QString &fileId, int status,
                          const QString &savePath = QString(),
                          const QString &md5 = QString());
    FileRecord getFileRecord(const QString &fileId);

    // ---- messages.db ----
    qint64 insertMessage(const StoredMessage &m);
    QVector<StoredMessage> loadMessages(const QString &contactUid,
                                        int limit = 200);

    // ---- meta.db: friend_requests ----
    void insertFriendRequest(const FriendRequestInfo &r);
    QVector<FriendRequestInfo> loadPendingFriendRequests();
    void updateFriendRequestStatus(int requestId, int status);
    int  getPendingFriendRequestCount();

    // ---- 文件操作 ----
    QString generateUniqueFilePath(const QString &targetDir,
                                   const QString &originalName);
    bool openFile(const QString &fileId);

private:
    explicit LocalDB(QObject *parent = nullptr);
    ~LocalDB();
    LocalDB(const LocalDB &) = delete;
    LocalDB &operator=(const LocalDB &) = delete;

    bool initMetaDB();
    bool initMsgDB();
    bool createMetaTables();
    bool createMsgTables();

    QString m_uid;
    QString m_rootPath;         // {appDir}/medchat_file/{uid}
    QString m_metaConnName;     // meta.db 连接名
    QString m_msgConnName;      // messages.db 连接名
    bool    m_initialized = false;
};

#endif // LOCALDB_H
