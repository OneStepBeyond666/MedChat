#include "serverdb.h"
#include "common/constants.h"
#include <QDir>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <QUuid>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDateTime>

ServerDB::ServerDB(const QString &dbDir, QObject *parent)
    : QObject(parent)
{
    // 确保数据库目录存在
    QDir dir(dbDir);
    if (!dir.exists())
        dir.mkpath(".");

    m_dbPath = dir.filePath("server.db");
    m_connName = QStringLiteral("medchat_main_") + QUuid::createUuid().toString(QUuid::Id128);

    initDatabase();
}

ServerDB::~ServerDB()
{
    // 关闭主连接
    {
        QSqlDatabase db = QSqlDatabase::database(m_connName);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connName);
}

void ServerDB::initDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
    db.setDatabaseName(m_dbPath);

    if (!db.open()) {
        qCritical() << "[ServerDB] 无法打开数据库:" << m_dbPath
                    << db.lastError().text();
        return;
    }

    // 启用外键约束
    QSqlQuery pragma(db);
    pragma.exec("PRAGMA foreign_keys = ON");
    pragma.exec("PRAGMA journal_mode = WAL");      // WAL 模式提升并发性能
    pragma.exec("PRAGMA synchronous = NORMAL");     // 平衡性能与安全

    qDebug() << "[ServerDB] 数据库已打开:" << m_dbPath;

    createTables();
}

void ServerDB::createTables()
{
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);

    // =========================================================
    // users 表
    // =========================================================
    bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "  uid           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username      TEXT    UNIQUE NOT NULL,"
        "  password_hash TEXT    NOT NULL,"
        "  salt          TEXT    NOT NULL,"
        "  nickname      TEXT    DEFAULT '',"
        "  role          TEXT    NOT NULL CHECK(role IN ('doctor','patient')),"
        "  avatar_blob   BLOB"
        ")"
    );
    if (!ok) qCritical() << "[ServerDB] 创建 users 表失败:" << q.lastError().text();

    // =========================================================
    // friends 表
    // =========================================================
    ok = q.exec(
        "CREATE TABLE IF NOT EXISTS friends ("
        "  uid          INTEGER NOT NULL,"
        "  friend_uid   INTEGER NOT NULL,"
        "  status       INTEGER NOT NULL DEFAULT 0,"  // 0=申请, 1=好友
        "  created_at   INTEGER NOT NULL,"
        "  PRIMARY KEY (uid, friend_uid),"
        "  FOREIGN KEY (uid)        REFERENCES users(uid),"
        "  FOREIGN KEY (friend_uid) REFERENCES users(uid)"
        ")"
    );
    if (!ok) qCritical() << "[ServerDB] 创建 friends 表失败:" << q.lastError().text();

    // =========================================================
    // offline_messages 表
    // =========================================================
    ok = q.exec(
        "CREATE TABLE IF NOT EXISTS offline_messages ("
        "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  sender_uid    INTEGER NOT NULL,"
        "  receiver_uid  INTEGER NOT NULL,"
        "  payload       TEXT    NOT NULL,"
        "  type          INTEGER NOT NULL DEFAULT 0,"  // 0=文字, 1=文件, 2=好友请求
        "  timestamp     INTEGER NOT NULL,"
        "  status        INTEGER NOT NULL DEFAULT 0,"  // 0=pending, 1=sent(待ACK)
        "  FOREIGN KEY (sender_uid)   REFERENCES users(uid),"
        "  FOREIGN KEY (receiver_uid) REFERENCES users(uid)"
        ")"
    );
    if (!ok) qCritical() << "[ServerDB] 创建 offline_messages 表失败:" << q.lastError().text();

    // 兼容旧数据库：若表已存在但无 status 列，自动 ALTER ADD
    QSqlRecord rec = db.record("offline_messages");
    if (!rec.contains("status")) {
        q.exec("ALTER TABLE offline_messages ADD COLUMN status INTEGER NOT NULL DEFAULT 0");
        qDebug() << "[ServerDB] offline_messages 表已添加 status 列";
    }

    qDebug() << "[ServerDB] 数据表初始化完成 (users, friends, offline_messages)";
}

QSqlDatabase ServerDB::database() const
{
    return QSqlDatabase::database(m_connName);
}

bool ServerDB::areFriends(int uid1, int uid2)
{
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);

    // 查询双向记录：uid1→uid2 和 uid2→uid1，只要至少一方 status=1 即视为好友
    // 常规 IM 逻辑：A 加 B 为好友后，B 也有一条指向 A 的记录（status=1）
    // 但如果业务上只要单向同意就算好友，用 OR 即可
    q.prepare(
        "SELECT COUNT(*) FROM friends "
        "WHERE ((uid = :u1 AND friend_uid = :u2) OR (uid = :u2 AND friend_uid = :u1)) "
        "AND status = 1"
    );
    q.bindValue(":u1", uid1);
    q.bindValue(":u2", uid2);

    if (q.exec() && q.next()) {
        int count = q.value(0).toInt();
        // 双向好友：期望 2 条记录（A→B 和 B→A 都是 status=1）
        // 如果业务上只要求至少 1 方同意即可，改为 count >= 1
        return count >= 1;
    }

    qWarning() << "[ServerDB] areFriends 查询失败:" << q.lastError().text();
    return false;
}

bool ServerDB::addFriendship(int uid1, int uid2)
{
    if (uid1 <= 0 || uid2 <= 0 || uid1 == uid2) return false;

    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;

    // 插入 uid1 → uid2（若不存在）
    q.prepare(
        "INSERT OR IGNORE INTO friends (uid, friend_uid, status, created_at) "
        "VALUES (:u1, :u2, 1, :ts)"
    );
    q.bindValue(":u1", uid1);
    q.bindValue(":u2", uid2);
    q.bindValue(":ts", now);
    if (!q.exec()) {
        qWarning() << "[ServerDB] addFriendship 插入方向1失败:" << q.lastError().text();
        return false;
    }

    // 插入 uid2 → uid1（若不存在）
    q.prepare(
        "INSERT OR IGNORE INTO friends (uid, friend_uid, status, created_at) "
        "VALUES (:u2, :u1, 1, :ts)"
    );
    q.bindValue(":u1", uid1);
    q.bindValue(":u2", uid2);
    q.bindValue(":ts", now);
    if (!q.exec()) {
        qWarning() << "[ServerDB] addFriendship 插入方向2失败:" << q.lastError().text();
        return false;
    }

    qDebug() << QString("[ServerDB] 添加好友关系: uid=%1 <-> uid=%2").arg(uid1).arg(uid2);
    return true;
}

QSqlDatabase ServerDB::cloneConnection(const QString &connName)
{
    QSqlDatabase src = QSqlDatabase::database(m_connName);
    QSqlDatabase clone = QSqlDatabase::addDatabase("QSQLITE", connName);
    clone.setDatabaseName(src.databaseName());
    if (!clone.open()) {
        qWarning() << "[ServerDB] 克隆连接失败:" << connName
                   << clone.lastError().text();
    } else {
        QSqlQuery pragma(clone);
        pragma.exec("PRAGMA foreign_keys = ON");
    }
    return clone;
}

bool ServerDB::saveOfflineMessage(int senderUid, int receiverUid, const QString &payload, int type)
{
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);

    q.prepare(
        "INSERT INTO offline_messages (sender_uid, receiver_uid, payload, type, timestamp) "
        "VALUES (:sender, :receiver, :payload, :type, :timestamp)"
    );
    q.bindValue(":sender", senderUid);
    q.bindValue(":receiver", receiverUid);
    q.bindValue(":payload", payload);
    q.bindValue(":type", type);
    q.bindValue(":timestamp", QDateTime::currentDateTime().toSecsSinceEpoch());

    if (!q.exec()) {
        qWarning() << "[ServerDB] 保存离线消息失败:" << q.lastError().text();
        return false;
    }

    qDebug() << "[ServerDB] 离线消息已保存: sender=" << senderUid << "receiver=" << receiverUid;
    return true;
}

QJsonArray ServerDB::getPendingOfflineMessages(int receiverUid)
{
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QJsonArray messages;

    // 查询所有 status<=1 的消息（pending + 之前发了但没收到ACK的）
    QSqlQuery selectQuery(db);
    selectQuery.prepare(
        "SELECT id, sender_uid, payload, type, timestamp, status "
        "FROM offline_messages "
        "WHERE receiver_uid = :receiver AND status <= 1 "
        "ORDER BY timestamp ASC"
    );
    selectQuery.bindValue(":receiver", receiverUid);

    if (!selectQuery.exec()) {
        qWarning() << "[ServerDB] 查询离线消息失败:" << selectQuery.lastError().text();
        return messages;
    }

    while (selectQuery.next()) {
        int id = selectQuery.value("id").toInt();
        QString payload = selectQuery.value("payload").toString();

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            messages.append(doc.object());
        } else {
            qWarning() << "[ServerDB] 离线消息 JSON 解析失败: id=" << id;
        }
    }

    qDebug() << "[ServerDB] 离线消息查询: receiver=" << receiverUid
             << "count=" << messages.size();
    return messages;
}

bool ServerDB::markOfflineMessagesAsSent(int receiverUid)
{
    QSqlDatabase db = QSqlDatabase::database(m_connName);

    // 将该用户的 pending (status=0) 消息标记为已发送 (status=1)
    QSqlQuery q(db);
    q.prepare(
        "UPDATE offline_messages SET status = 1 "
        "WHERE receiver_uid = :receiver AND status = 0"
    );
    q.bindValue(":receiver", receiverUid);

    if (!q.exec()) {
        qWarning() << "[ServerDB] 标记离线消息已发送失败:" << q.lastError().text();
        return false;
    }

    qDebug() << "[ServerDB] 离线消息标记已发送: receiver=" << receiverUid
             << "rows=" << q.numRowsAffected();
    return true;
}

bool ServerDB::deleteAckedOfflineMessages(int receiverUid)
{
    QSqlDatabase db = QSqlDatabase::database(m_connName);

    if (!db.transaction()) {
        qWarning() << "[ServerDB] 删除ACK离线消息事务启动失败";
        return false;
    }

    QSqlQuery q(db);
    q.prepare(
        "DELETE FROM offline_messages "
        "WHERE receiver_uid = :receiver AND status = 1"
    );
    q.bindValue(":receiver", receiverUid);

    if (!q.exec()) {
        qWarning() << "[ServerDB] 删除ACK离线消息失败:" << q.lastError().text();
        db.rollback();
        return false;
    }

    int deleted = q.numRowsAffected();

    if (!db.commit()) {
        qWarning() << "[ServerDB] 删除ACK离线消息提交失败:" << db.lastError().text();
        db.rollback();
        return false;
    }

    qDebug() << "[ServerDB] 离线消息ACK删除: receiver=" << receiverUid
             << "deleted=" << deleted;
    return true;
}
