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

    /// 离线消息：保存离线消息
    bool saveOfflineMessage(int senderUid, int receiverUid, const QString &payload, int type = 0);

    /// 离线消息：获取并清除用户的所有离线消息（事务保证原子性）
    QJsonArray getAndClearOfflineMessages(int receiverUid);

private:
    void initDatabase();
    void createTables();

    QString m_dbPath;
    QString m_connName;
};

#endif // SERVERDB_H
