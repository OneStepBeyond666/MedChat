#include "localdb.h"
#include "common/constants.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QDesktopServices>
#include <QUrl>
#include <QUuid>
#include <QDateTime>
#include <QDebug>

// ============================================================
// 单例
// ============================================================

LocalDB &LocalDB::instance()
{
    static LocalDB inst;
    return inst;
}

LocalDB::LocalDB(QObject *parent)
    : QObject(parent)
{
}

LocalDB::~LocalDB()
{
    close();
}

// ============================================================
// 初始化 / 关闭
// ============================================================

bool LocalDB::init(const QString &uid)
{
    if (m_initialized)
        close();

    m_uid = uid;
    m_rootPath = QCoreApplication::applicationDirPath()
                 + "/" + Constants::FILE_ROOT_NAME
                 + "/" + uid;

    // 创建目录结构
    QDir root(m_rootPath);
    root.mkpath(Constants::MSG_SUBDIR);
    root.mkpath(Constants::FILE_SUBDIR + "/" + Constants::TEMP_SUBDIR);

    if (!initMetaDB())
        return false;
    if (!initMsgDB())
        return false;

    m_initialized = true;
    qDebug() << "[LocalDB] 初始化完成, 根目录:" << m_rootPath;
    return true;
}

void LocalDB::close()
{
    if (!m_initialized)
        return;

    auto closeConn = [](const QString &connName) {
        if (connName.isEmpty())
            return;
        QSqlDatabase db = QSqlDatabase::database(connName);
        if (db.isOpen())
            db.close();
        QSqlDatabase::removeDatabase(connName);
    };

    closeConn(m_metaConnName);
    closeConn(m_msgConnName);

    m_metaConnName.clear();
    m_msgConnName.clear();
    m_initialized = false;
}

QString LocalDB::tempDir() const
{
    return m_rootPath + "/" + Constants::FILE_SUBDIR
           + "/" + Constants::TEMP_SUBDIR;
}

// ============================================================
// meta.db 初始化
// ============================================================

bool LocalDB::initMetaDB()
{
    m_metaConnName = QStringLiteral("localdb_meta_")
                     + QUuid::createUuid().toString(QUuid::Id128);

    QString dbPath = m_rootPath + "/" + Constants::MSG_SUBDIR
                     + "/" + Constants::META_DB_NAME;

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_metaConnName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qCritical() << "[LocalDB] 无法打开 meta.db:" << dbPath
                    << db.lastError().text();
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec("PRAGMA journal_mode = WAL");
    pragma.exec("PRAGMA foreign_keys = ON");
    pragma.exec("PRAGMA synchronous = NORMAL");

    qDebug() << "[LocalDB] meta.db 已打开:" << dbPath;
    return createMetaTables();
}

bool LocalDB::createMetaTables()
{
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    QSqlQuery q(db);

    bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  contact_uid     TEXT PRIMARY KEY,"
        "  username        TEXT,"
        "  last_msg_preview TEXT,"
        "  last_time       INTEGER,"
        "  unread_count    INTEGER DEFAULT 0"
        ")"
    );
    if (!ok) {
        qCritical() << "[LocalDB] 创建 sessions 表失败:" << q.lastError().text();
        return false;
    }

    ok = q.exec(
        "CREATE TABLE IF NOT EXISTS file_index ("
        "  file_id        TEXT PRIMARY KEY,"
        "  original_name  TEXT,"
        "  save_name      TEXT,"
        "  save_path      TEXT,"
        "  size           INTEGER,"
        "  md5            TEXT,"
        "  year_month     TEXT,"
        "  status         INTEGER DEFAULT 0"
        ")"
    );
    if (!ok) {
        qCritical() << "[LocalDB] 创建 file_index 表失败:" << q.lastError().text();
        return false;
    }

    ok = q.exec(
        "CREATE TABLE IF NOT EXISTS friend_requests ("
        "  request_id    INTEGER PRIMARY KEY,"
        "  from_username TEXT,"
        "  nickname      TEXT,"
        "  message       TEXT,"
        "  timestamp     INTEGER,"
        "  avatar_data   BLOB,"
        "  status        INTEGER DEFAULT 0"
        ")"
    );
    if (!ok) {
        qCritical() << "[LocalDB] 创建 friend_requests 表失败:" << q.lastError().text();
        return false;
    }

    return true;
}

// ============================================================
// messages.db 初始化
// ============================================================

bool LocalDB::initMsgDB()
{
    m_msgConnName = QStringLiteral("localdb_msg_")
                    + QUuid::createUuid().toString(QUuid::Id128);

    QString dbPath = m_rootPath + "/" + Constants::MSG_SUBDIR
                     + "/" + Constants::MSG_DB_NAME;

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_msgConnName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qCritical() << "[LocalDB] 无法打开 messages.db:" << dbPath
                    << db.lastError().text();
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec("PRAGMA journal_mode = WAL");
    pragma.exec("PRAGMA foreign_keys = ON");
    pragma.exec("PRAGMA synchronous = NORMAL");

    qDebug() << "[LocalDB] messages.db 已打开:" << dbPath;
    return createMsgTables();
}

bool LocalDB::createMsgTables()
{
    QSqlDatabase db = QSqlDatabase::database(m_msgConnName);
    QSqlQuery q(db);

    bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS messages ("
        "  msg_id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  contact_uid    TEXT NOT NULL,"
        "  type           INTEGER NOT NULL,"
        "  content        TEXT,"
        "  file_id        TEXT,"
        "  timestamp      INTEGER NOT NULL,"
        "  is_mine        INTEGER NOT NULL,"
        "  recalled       INTEGER DEFAULT 0"
        ")"
    );
    if (!ok) {
        qCritical() << "[LocalDB] 创建 messages 表失败:" << q.lastError().text();
        return false;
    }

    // 迁移：为旧版数据库添加 recalled 字段
    q.exec("ALTER TABLE messages ADD COLUMN recalled INTEGER DEFAULT 0");

    ok = q.exec(
        "CREATE INDEX IF NOT EXISTS idx_msg_contact_time "
        "ON messages(contact_uid, timestamp)"
    );
    if (!ok) {
        qCritical() << "[LocalDB] 创建索引失败:" << q.lastError().text();
        return false;
    }

    return true;
}

// ============================================================
// sessions 操作
// ============================================================

void LocalDB::upsertSession(const SessionInfo &s)
{
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    db.transaction();

    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO sessions (contact_uid, username, last_msg_preview, last_time, unread_count) "
        "VALUES (:cid, :uname, :preview, :ltime, :unread) "
        "ON CONFLICT(contact_uid) DO UPDATE SET "
        "  username = excluded.username,"
        "  last_msg_preview = excluded.last_msg_preview,"
        "  last_time = excluded.last_time,"
        "  unread_count = excluded.unread_count"
    );
    q.bindValue(":cid", s.contactUid);
    q.bindValue(":uname", s.username);
    q.bindValue(":preview", s.lastMsgPreview);
    q.bindValue(":ltime", s.lastTime);
    q.bindValue(":unread", s.unreadCount);

    if (!q.exec())
        qWarning() << "[LocalDB] upsertSession 失败:" << q.lastError().text();

    db.commit();
}

QVector<SessionInfo> LocalDB::loadSessions()
{
    QVector<SessionInfo> result;
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    QSqlQuery q(db);
    q.prepare("SELECT contact_uid, username, last_msg_preview, last_time, unread_count "
              "FROM sessions ORDER BY last_time DESC");

    if (q.exec()) {
        while (q.next()) {
            SessionInfo s;
            s.contactUid    = q.value(0).toString();
            s.username      = q.value(1).toString();
            s.lastMsgPreview = q.value(2).toString();
            s.lastTime      = q.value(3).toLongLong();
            s.unreadCount   = q.value(4).toInt();
            result.append(s);
        }
    }
    return result;
}

void LocalDB::clearUnread(const QString &contactUid)
{
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    QSqlQuery q(db);
    q.prepare("UPDATE sessions SET unread_count = 0 WHERE contact_uid = :cid");
    q.bindValue(":cid", contactUid);
    if (!q.exec())
        qWarning() << "[LocalDB] clearUnread 失败:" << q.lastError().text();
}

void LocalDB::incrementUnread(const QString &contactUid)
{
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    QSqlQuery q(db);
    q.prepare("UPDATE sessions SET unread_count = unread_count + 1 WHERE contact_uid = :cid");
    q.bindValue(":cid", contactUid);
    if (!q.exec())
        qWarning() << "[LocalDB] incrementUnread 失败:" << q.lastError().text();
}

// ============================================================
// file_index 操作
// ============================================================

void LocalDB::insertFileRecord(const FileRecord &r)
{
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    db.transaction();

    QSqlQuery q(db);
    q.prepare(
        "INSERT OR REPLACE INTO file_index "
        "(file_id, original_name, save_name, save_path, size, md5, year_month, status) "
        "VALUES (:fid, :oname, :sname, :spath, :sz, :md5, :ym, :st)"
    );
    q.bindValue(":fid", r.fileId);
    q.bindValue(":oname", r.originalName);
    q.bindValue(":sname", r.saveName);
    q.bindValue(":spath", r.savePath);
    q.bindValue(":sz", r.size);
    q.bindValue(":md5", r.md5);
    q.bindValue(":ym", r.yearMonth);
    q.bindValue(":st", r.status);

    if (!q.exec())
        qWarning() << "[LocalDB] insertFileRecord 失败:" << q.lastError().text();

    db.commit();
}

void LocalDB::updateFileRecord(const QString &fileId, int status,
                                const QString &savePath, const QString &md5)
{
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    QSqlQuery q(db);

    QStringList setClauses;
    setClauses << "status = :st";
    if (!savePath.isNull())
        setClauses << "save_path = :spath";
    if (!md5.isNull())
        setClauses << "md5 = :md5";

    q.prepare("UPDATE file_index SET " + setClauses.join(", ")
              + " WHERE file_id = :fid");
    q.bindValue(":fid", fileId);
    q.bindValue(":st", status);
    if (!savePath.isNull())
        q.bindValue(":spath", savePath);
    if (!md5.isNull())
        q.bindValue(":md5", md5);

    if (!q.exec())
        qWarning() << "[LocalDB] updateFileRecord 失败:" << q.lastError().text();
}

FileRecord LocalDB::getFileRecord(const QString &fileId)
{
    FileRecord r;
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    QSqlQuery q(db);
    q.prepare("SELECT file_id, original_name, save_name, save_path, size, md5, year_month, status "
              "FROM file_index WHERE file_id = :fid");
    q.bindValue(":fid", fileId);

    if (q.exec() && q.next()) {
        r.fileId       = q.value(0).toString();
        r.originalName = q.value(1).toString();
        r.saveName     = q.value(2).toString();
        r.savePath     = q.value(3).toString();
        r.size         = q.value(4).toLongLong();
        r.md5          = q.value(5).toString();
        r.yearMonth    = q.value(6).toString();
        r.status       = q.value(7).toInt();
    }
    return r;
}

// ============================================================
// messages 操作
// ============================================================

qint64 LocalDB::insertMessage(const StoredMessage &m)
{
    QSqlDatabase db = QSqlDatabase::database(m_msgConnName);
    db.transaction();

    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO messages (contact_uid, type, content, file_id, timestamp, is_mine) "
        "VALUES (:cid, :type, :content, :fid, :ts, :mine)"
    );
    q.bindValue(":cid", m.contactUid);
    q.bindValue(":type", m.type);
    q.bindValue(":content", m.content);
    q.bindValue(":fid", m.fileId);
    q.bindValue(":ts", m.timestamp);
    q.bindValue(":mine", m.isMine ? 1 : 0);

    qint64 rowId = -1;
    if (q.exec()) {
        rowId = q.lastInsertId().toLongLong();
    } else {
        qWarning() << "[LocalDB] insertMessage 失败:" << q.lastError().text();
    }

    db.commit();
    return rowId;
}

QVector<StoredMessage> LocalDB::loadMessages(const QString &contactUid, int limit)
{
    QVector<StoredMessage> result;
    QSqlDatabase db = QSqlDatabase::database(m_msgConnName);
    QSqlQuery q(db);
    q.prepare(
        "SELECT msg_id, contact_uid, type, content, file_id, timestamp, is_mine, recalled "
        "FROM messages WHERE contact_uid = :cid "
        "ORDER BY timestamp ASC LIMIT :lim"
    );
    q.bindValue(":cid", contactUid);
    q.bindValue(":lim", limit);

    if (q.exec()) {
        while (q.next()) {
            StoredMessage m;
            m.msgId      = q.value(0).toLongLong();
            m.contactUid = q.value(1).toString();
            m.type       = q.value(2).toInt();
            m.content    = q.value(3).toString();
            m.fileId     = q.value(4).toString();
            m.timestamp  = q.value(5).toLongLong();
            m.isMine     = q.value(6).toBool();
            m.isRecalled = q.value(7).toInt() != 0;
            result.append(m);
        }
    }
    return result;
}

void LocalDB::deleteMessage(qint64 msgId)
{
    QSqlDatabase db = QSqlDatabase::database(m_msgConnName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM messages WHERE msg_id = ?");
    q.addBindValue(msgId);
    q.exec();
}

void LocalDB::markMessageRecalled(qint64 msgId, bool isMine)
{
    QSqlDatabase db = QSqlDatabase::database(m_msgConnName);
    QSqlQuery q(db);
    q.prepare("UPDATE messages SET recalled = 1, content = ? WHERE msg_id = ?");
    q.addBindValue(isMine
        ? QStringLiteral("[你撤回了一条消息]")
        : QStringLiteral("[对方撤回了一条消息]"));
    q.addBindValue(msgId);
    q.exec();
}

// ============================================================
// friend_requests 操作
// ============================================================

void LocalDB::insertFriendRequest(const FriendRequestInfo &r)
{
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    QSqlQuery q(db);
    q.prepare(
        "INSERT OR REPLACE INTO friend_requests "
        "(request_id, from_username, nickname, message, timestamp, avatar_data, status) "
        "VALUES (:rid, :from, :nick, :msg, :ts, :avatar, :st)"
    );
    q.bindValue(":rid", r.requestId);
    q.bindValue(":from", r.fromUsername);
    q.bindValue(":nick", r.nickname);
    q.bindValue(":msg", r.message);
    q.bindValue(":ts", r.timestamp);
    q.bindValue(":avatar", r.avatarData);
    q.bindValue(":st", r.status);

    if (!q.exec())
        qWarning() << "[LocalDB] insertFriendRequest 失败:" << q.lastError().text();
}

QVector<FriendRequestInfo> LocalDB::loadPendingFriendRequests()
{
    QVector<FriendRequestInfo> result;
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    QSqlQuery q(db);
    q.prepare(
        "SELECT request_id, from_username, nickname, message, timestamp, avatar_data "
        "FROM friend_requests WHERE status = 0 "
        "ORDER BY timestamp DESC"
    );

    if (q.exec()) {
        while (q.next()) {
            FriendRequestInfo r;
            r.requestId    = q.value(0).toInt();
            r.fromUsername = q.value(1).toString();
            r.nickname     = q.value(2).toString();
            r.message      = q.value(3).toString();
            r.timestamp    = q.value(4).toLongLong();
            r.avatarData   = q.value(5).toByteArray();
            r.status       = 0;
            result.append(r);
        }
    }
    return result;
}

void LocalDB::updateFriendRequestStatus(int requestId, int status)
{
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    QSqlQuery q(db);
    q.prepare("UPDATE friend_requests SET status = :st WHERE request_id = :rid");
    q.bindValue(":st", status);
    q.bindValue(":rid", requestId);

    if (!q.exec())
        qWarning() << "[LocalDB] updateFriendRequestStatus 失败:" << q.lastError().text();
}

int LocalDB::getPendingFriendRequestCount()
{
    QSqlDatabase db = QSqlDatabase::database(m_metaConnName);
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM friend_requests WHERE status = 0");
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

// ============================================================
// 文件操作
// ============================================================

QString LocalDB::generateUniqueFilePath(const QString &targetDir,
                                         const QString &originalName)
{
    QFileInfo fi(originalName);
    QString base = fi.completeBaseName();
    QString ext  = fi.suffix().isEmpty() ? QString() : ("." + fi.suffix());

    QString candidate = targetDir + "/" + base + ext;
    if (!QFile::exists(candidate))
        return candidate;

    for (int i = 1; i < 10000; ++i) {
        candidate = targetDir + "/" + base
                    + "(" + QString::number(i) + ")" + ext;
        if (!QFile::exists(candidate))
            return candidate;
    }

    // 极端情况：返回带时间戳的文件名
    candidate = targetDir + "/" + base + "_"
                + QString::number(QDateTime::currentMSecsSinceEpoch()) + ext;
    return candidate;
}

bool LocalDB::openFile(const QString &fileId)
{
    FileRecord rec = getFileRecord(fileId);
    if (rec.fileId.isEmpty()) {
        qWarning() << "[LocalDB] openFile: 未找到文件记录" << fileId;
        return false;
    }

    if (rec.status != 1) {
        qWarning() << "[LocalDB] openFile: 文件未完成接收, status =" << rec.status;
        return false;
    }

    if (!QFile::exists(rec.savePath)) {
        qWarning() << "[LocalDB] openFile: 文件不存在" << rec.savePath;
        return false;
    }

    return QDesktopServices::openUrl(QUrl::fromLocalFile(rec.savePath));
}
