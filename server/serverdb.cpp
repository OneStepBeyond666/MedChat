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
