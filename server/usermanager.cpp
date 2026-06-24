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

bool UserManager::registerUser(const QString &username, const QString &password, const QString &role, const QString &nickname,
                                const QString &secQuestion, const QString &secAnswerHash)
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
        "INSERT INTO users (username, password_hash, salt, nickname, role, sec_question, sec_answer_hash) "
        "VALUES (:username, :hash, :salt, :nickname, :role, :sec_q, :sec_a)"
    );
    q.bindValue(":username", username);
    q.bindValue(":hash", hash);
    q.bindValue(":salt", salt);
    q.bindValue(":nickname", actualNick);
    q.bindValue(":role", role);
    q.bindValue(":sec_q", secQuestion);
    q.bindValue(":sec_a", secAnswerHash);

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
    q.exec("SELECT uid, username, nickname, role, avatar_blob FROM users ORDER BY uid");
    while (q.next()) {
        UserInfo info;
        info.uid        = q.value(0).toInt();
        info.username   = q.value(1).toString();
        info.nickname   = q.value(2).toString();
        info.role       = q.value(3).toString();
        info.avatarBlob = q.value(4).toByteArray();
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
    q.prepare("SELECT uid, username, nickname, role, avatar_blob, signature, gender, birthday, region "
              "FROM users WHERE uid = :uid");
    q.bindValue(":uid", uid);
    if (q.exec() && q.next()) {
        info.uid        = q.value(0).toInt();
        info.username   = q.value(1).toString();
        info.nickname   = q.value(2).toString();
        info.role       = q.value(3).toString();
        info.avatarBlob = q.value(4).toByteArray();
        info.signature  = q.value(5).toString();
        info.gender     = q.value(6).toInt();
        info.birthday   = q.value(7).toString();
        info.region     = q.value(8).toString();
    }
    return info;
}

UserInfo UserManager::getUserInfoByName(const QString &username) const
{
    UserInfo info;
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("SELECT uid, username, nickname, role, avatar_blob, signature, gender, birthday, region "
              "FROM users WHERE username = :u");
    q.bindValue(":u", username);
    if (q.exec() && q.next()) {
        info.uid        = q.value(0).toInt();
        info.username   = q.value(1).toString();
        info.nickname   = q.value(2).toString();
        info.role       = q.value(3).toString();
        info.avatarBlob = q.value(4).toByteArray();
        info.signature  = q.value(5).toString();
        info.gender     = q.value(6).toInt();
        info.birthday   = q.value(7).toString();
        info.region     = q.value(8).toString();
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

// ============================================================
// 密码安全
// ============================================================

QJsonObject UserManager::getSecurityQuestion(const QString &username) const
{
    return m_db->getSecurityQuestion(username);
}

bool UserManager::resetPassword(const QString &username, const QString &answerHash, const QString &newPassword)
{
    QString newPassHash = hashPassword(newPassword, generateSalt());
    // 注意：这里传给 DB 的是纯新密码，DB 内部会重新生成盐并哈希
    // 实际上 DB 的 resetPassword 期望接收的是新密码的明文哈希（不对，应该是重新哈希）
    // 让我重新设计：resetPassword 的 newPasswordHash 应该传明文密码，由 DB 重新哈希
    // 但为了保持接口一致，我让 UserManager 直接调用 m_db->resetPassword，传入 answerHash 和 newPassword 的哈希

    // 先生成新盐和新哈希
    QString newSalt = generateSalt();
    QByteArray data = newSalt.toUtf8() + newPassword.toUtf8();
    QString newHash = QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()
    );

    // 不对，这样 DB 又要重新哈希一次。让我简化：
    // UserManager::resetPassword 生成新盐，哈希新密码，直接 UPDATE 数据库

    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);

    // 先校验答案
    q.prepare("SELECT sec_answer_hash FROM users WHERE username = :u");
    q.bindValue(":u", username);
    if (!q.exec() || !q.next())
        return false;

    QString storedAnswerHash = q.value(0).toString();
    if (storedAnswerHash.isEmpty() || storedAnswerHash != answerHash)
        return false;

    // 生成新密码哈希
    QString salt = generateSalt();
    QString hash = hashPassword(newPassword, salt);

    QSqlQuery up(db);
    up.prepare("UPDATE users SET password_hash = :hash, salt = :salt WHERE username = :u");
    up.bindValue(":hash", hash);
    up.bindValue(":salt", salt);
    up.bindValue(":u", username);

    if (!up.exec()) {
        qWarning() << "[UserManager] 重置密码 UPDATE 失败:" << up.lastError().text();
        return false;
    }
    return up.numRowsAffected() > 0;
}

bool UserManager::changePassword(int uid, const QString &oldPassword, const QString &newPassword)
{
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);

    // 校验旧密码
    q.prepare("SELECT password_hash, salt FROM users WHERE uid = :uid");
    q.bindValue(":uid", uid);
    if (!q.exec() || !q.next())
        return false;

    QString storedHash = q.value(0).toString();
    QString storedSalt = q.value(1).toString();

    if (hashPassword(oldPassword, storedSalt) != storedHash)
        return false;

    // 更新为新密码
    QString newSalt = generateSalt();
    QString newHash = hashPassword(newPassword, newSalt);

    QSqlQuery up(db);
    up.prepare("UPDATE users SET password_hash = :hash, salt = :salt WHERE uid = :uid");
    up.bindValue(":hash", newHash);
    up.bindValue(":salt", newSalt);
    up.bindValue(":uid", uid);

    if (!up.exec()) {
        qWarning() << "[UserManager] 修改密码 UPDATE 失败:" << up.lastError().text();
        return false;
    }
    return up.numRowsAffected() > 0;
}

// ============================================================
// 更新个性签名
// ============================================================

bool UserManager::updateSignature(const QString &username, const QString &signature)
{
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("UPDATE users SET signature = :sig WHERE username = :u");
    q.bindValue(":sig", signature);
    q.bindValue(":u", username);
    return q.exec() && q.numRowsAffected() > 0;
}

// ============================================================
// 更新性别
// ============================================================

bool UserManager::updateGender(const QString &username, int gender)
{
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("UPDATE users SET gender = :g WHERE username = :u");
    q.bindValue(":g", gender);
    q.bindValue(":u", username);
    return q.exec() && q.numRowsAffected() > 0;
}

// ============================================================
// 更新生日
// ============================================================

bool UserManager::updateBirthday(const QString &username, const QString &birthday)
{
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("UPDATE users SET birthday = :b WHERE username = :u");
    q.bindValue(":b", birthday);
    q.bindValue(":u", username);
    return q.exec() && q.numRowsAffected() > 0;
}

// ============================================================
// 更新地区
// ============================================================

bool UserManager::updateRegion(const QString &username, const QString &region)
{
    QSqlDatabase db = m_db->database();
    QSqlQuery q(db);
    q.prepare("UPDATE users SET region = :r WHERE username = :u");
    q.bindValue(":r", region);
    q.bindValue(":u", username);
    return q.exec() && q.numRowsAffected() > 0;
}
