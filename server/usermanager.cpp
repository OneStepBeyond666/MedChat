#include "usermanager.h"
#include "serverdb.h"
#include "common/constants.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QImage>
#include <QBuffer>
#include <QDebug>
#include <QDateTime>

// ============================================================
// 构造
// ============================================================

UserManager::UserManager(ServerDB *db, QObject *parent)
    : QObject(parent), m_db(db)
{
}

// ============================================================
// 注册
// ============================================================

bool UserManager::registerUser(const QString &username, const QString &password, const QString &role, const QString &nickname)
{
    if (username.isEmpty() || password.isEmpty())
        return false;
    if (userExists(username))
        return false;

    QString salt = generateSalt();
    QString hash = hashPassword(password, salt);

    // 昵称为空时默认使用用户名
    QString actualNick = nickname.trimmed().isEmpty() ? username : nickname.trimmed();

    QSqlDatabase db = m_db->database();
    if (!db.transaction()) {
        qWarning() << "[UserManager] 事务开启失败:" << db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO users (username, password_hash, salt, nickname, role) "
        "VALUES (:username, :hash, :salt, :nickname, :role)"
    );
    q.bindValue(":username", username);
    q.bindValue(":hash", hash);
    q.bindValue(":salt", salt);
    q.bindValue(":nickname", actualNick);
    q.bindValue(":role", role);

    bool ok = q.exec();
    if (!ok) {
        qWarning() << "[UserManager] 注册 INSERT 失败:" << q.lastError().text();
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        qWarning() << "[UserManager] 注册 COMMIT 失败:" << db.lastError().text();
        return false;
    }

    qDebug() << "[UserManager] 注册用户:" << username << "昵称:" << actualNick << "role:" << role;
    return true;
}

// ============================================================
// 认证（带盐校验）
// ============================================================

bool UserManager::authenticate(const QString &username, const QString &password, QString &role) const
{
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("SELECT uid, password_hash, salt, role FROM users WHERE username = :u");
    q.bindValue(":u", username);

    if (!q.exec() || !q.next())
        return false;

    QString storedHash = q.value(1).toString();
    QString storedSalt = q.value(2).toString();
    role = q.value(3).toString();

    return hashPassword(password, storedSalt) == storedHash;
}

// ============================================================
// 查询
// ============================================================

bool UserManager::userExists(const QString &username) const
{
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM users WHERE username = :u LIMIT 1");
    q.bindValue(":u", username);
    return q.exec() && q.next();
}

QList<UserInfo> UserManager::allUsers() const
{
    QList<UserInfo> list;
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.exec("SELECT uid, username, nickname, role FROM users ORDER BY uid");
    while (q.next()) {
        UserInfo info;
        info.uid      = q.value(0).toInt();
        info.username = q.value(1).toString();
        info.nickname = q.value(2).toString();
        info.role     = q.value(3).toString();
        list.append(info);
    }
    return list;
}

int UserManager::getUidByUsername(const QString &username) const
{
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("SELECT uid FROM users WHERE username = :u LIMIT 1");
    q.bindValue(":u", username);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return -1;
}

UserInfo UserManager::getUserInfo(int uid) const
{
    UserInfo info;
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("SELECT uid, username, nickname, role FROM users WHERE uid = :uid");
    q.bindValue(":uid", uid);
    if (q.exec() && q.next()) {
        info.uid      = q.value(0).toInt();
        info.username = q.value(1).toString();
        info.nickname = q.value(2).toString();
        info.role     = q.value(3).toString();
    }
    return info;
}

UserInfo UserManager::getUserInfoByName(const QString &username) const
{
    UserInfo info;
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("SELECT uid, username, nickname, role FROM users WHERE username = :u");
    q.bindValue(":u", username);
    if (q.exec() && q.next()) {
        info.uid      = q.value(0).toInt();
        info.username = q.value(1).toString();
        info.nickname = q.value(2).toString();
        info.role     = q.value(3).toString();
    }
    return info;
}

// ============================================================
// 修改昵称
// ============================================================

bool UserManager::updateNickname(const QString &username, const QString &nickname)
{
    QSqlDatabase db = m_db->database();
    if (!db.transaction()) return false;

    QSqlQuery q(db);
    q.prepare("UPDATE users SET nickname = :nick WHERE username = :u");
    q.bindValue(":nick", nickname);
    q.bindValue(":u", username);

    bool ok = q.exec() && q.numRowsAffected() > 0;
    ok ? db.commit() : db.rollback();
    return ok;
}

// ============================================================
// 头像
// ============================================================

bool UserManager::updateAvatar(const QString &username, const QByteArray &rawAvatar)
{
    // 大小校验 + 缩放
    bool processOk = false;
    QByteArray processed = processAvatar(rawAvatar, processOk);
    if (!processOk) {
        qWarning() << "[UserManager] 头像处理失败：数据无效或超过大小限制";
        return false;
    }

    QSqlDatabase db = m_db->database();
    if (!db.transaction()) return false;

    QSqlQuery q(db);
    q.prepare("UPDATE users SET avatar_blob = :blob WHERE username = :u");
    q.bindValue(":blob", processed);
    q.bindValue(":u", username);

    bool ok = q.exec() && q.numRowsAffected() > 0;
    ok ? db.commit() : db.rollback();
    return ok;
}

QByteArray UserManager::getAvatar(int uid) const
{
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("SELECT avatar_blob FROM users WHERE uid = :uid");
    q.bindValue(":uid", uid);
    if (q.exec() && q.next())
        return q.value(0).toByteArray();
    return QByteArray();
}

// ============================================================
// 密码工具
// ============================================================

QString UserManager::generateSalt()
{
    QByteArray salt(32, 0);
    QRandomGenerator *rng = QRandomGenerator::system();
    for (int i = 0; i < salt.size(); ++i)
        salt[i] = static_cast<char>(rng->bounded(256));
    return QString::fromLatin1(salt.toHex());   // 64 hex 字符
}

QString UserManager::hashPassword(const QString &password, const QString &salt)
{
    QByteArray data = salt.toUtf8() + password.toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()
    );
}

// ============================================================
// 头像处理
// ============================================================

QByteArray UserManager::processAvatar(const QByteArray &rawData, bool &ok)
{
    ok = false;

    // 大小校验
    if (rawData.isEmpty() || rawData.size() > Constants::AVATAR_MAX_SIZE)
        return QByteArray();

    // 解码图片
    QImage img;
    if (!img.loadFromData(rawData))
        return QByteArray();

    // 缩放到标准尺寸（保持纵横比）
    QImage scaled = img.scaled(
        Constants::AVATAR_DISPLAY_SIZE,
        Constants::AVATAR_DISPLAY_SIZE,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    // 编码为 PNG
    QByteArray output;
    QBuffer buffer(&output);
    buffer.open(QIODevice::WriteOnly);
    if (!scaled.save(&buffer, "PNG"))
        return QByteArray();

    ok = true;
    return output;
}
