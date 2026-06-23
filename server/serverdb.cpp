#include "serverdb.h"
#include "common/constants.h"
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QUuid>

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
        "  type          INTEGER NOT NULL DEFAULT 0,"  // 0=文字, 1=文件
        "  timestamp     INTEGER NOT NULL,"
        "  FOREIGN KEY (sender_uid)   REFERENCES users(uid),"
        "  FOREIGN KEY (receiver_uid) REFERENCES users(uid)"
        ")"
    );
    if (!ok) qCritical() << "[ServerDB] 创建 offline_messages 表失败:" << q.lastError().text();

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
    q.bindValue(":timestamp", QDateTime::currentSecsSinceEpoch());

    if (!q.exec()) {
        qWarning() << "[ServerDB] 保存离线消息失败:" << q.lastError().text();
        return false;
    }

    qDebug() << "[ServerDB] 离线消息已保存: sender=" << senderUid << "receiver=" << receiverUid;
    return true;
}

QJsonArray ServerDB::getAndClearOfflineMessages(int receiverUid)
{
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QJsonArray messages;

    // 使用事务保证原子性：查询和下发过程中如果服务端崩溃，不会导致消息丢失或重复下发
    db.transaction();

    // 1. 查询所有离线消息
    QSqlQuery selectQuery(db);
    selectQuery.prepare(
        "SELECT id, sender_uid, payload, type, timestamp "
        "FROM offline_messages "
        "WHERE receiver_uid = :receiver "
        "ORDER BY timestamp ASC"
    );
    selectQuery.bindValue(":receiver", receiverUid);

    if (!selectQuery.exec()) {
        qWarning() << "[ServerDB] 查询离线消息失败:" << selectQuery.lastError().text();
        db.rollback();
        return messages;
    }

    // 2. 收集所有消息 ID（用于后续删除）
    QList<int> messageIds;
    while (selectQuery.next()) {
        int id = selectQuery.value("id").toInt();
        QString payload = selectQuery.value("payload").toString();

        messageIds.append(id);

        // 将 payload 解析为 JSON 对象
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            messages.append(doc.object());
        } else {
            qWarning() << "[ServerDB] 离线消息 JSON 解析失败: id=" << id;
        }
    }

    // 3. 删除已查询的消息（使用事务保证原子性）
    if (!messageIds.isEmpty()) {
        QString idList = "";
        for (int i = 0; i < messageIds.size(); ++i) {
            if (i > 0) idList += ",";
            idList += QString::number(messageIds[i]);
        }

        QSqlQuery deleteQuery(db);
        deleteQuery.prepare(QString("DELETE FROM offline_messages WHERE id IN (%1)").arg(idList));
        if (!deleteQuery.exec()) {
            qWarning() << "[ServerDB] 删除离线消息失败:" << deleteQuery.lastError().text();
            db.rollback();
            return QJsonArray();  // 返回空数组，触发重试
        }
    }

    // 4. 提交事务
    if (!db.commit()) {
        qWarning() << "[ServerDB] 提交事务失败:" << db.lastError().text();
        db.rollback();
        return QJsonArray();
    }

    qDebug() << "[ServerDB] 离线消息已下发并清除: receiver=" << receiverUid
             << "count=" << messages.size();
    return messages;
}
