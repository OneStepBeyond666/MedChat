#include "usermanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDebug>

UserManager::UserManager(const QString &filePath, QObject *parent)
    : QObject(parent), m_filePath(filePath)
{
    load();
}

UserManager::~UserManager()
{
    save();
}

bool UserManager::registerUser(const QString &username, const QString &password, const QString &role)
{
    if (username.isEmpty() || password.isEmpty())
        return false;
    if (m_users.contains(username))
        return false;

    UserInfo info;
    info.username = username;
    info.passwordHash = hashPassword(password);
    info.role = role;
    m_users[username] = info;
    save();
    qDebug() << "[UserManager] Registered user:" << username << "role:" << role;
    return true;
}

bool UserManager::authenticate(const QString &username, const QString &password, QString &role) const
{
    if (!m_users.contains(username))
        return false;
    const UserInfo &info = m_users[username];
    if (info.passwordHash != hashPassword(password))
        return false;
    role = info.role;
    return true;
}

bool UserManager::userExists(const QString &username) const
{
    return m_users.contains(username);
}

QList<UserInfo> UserManager::allUsers() const
{
    return m_users.values();
}

void UserManager::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[UserManager] No existing user database, starting fresh.";
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isArray())
        return;
    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        UserInfo info;
        info.username = obj["username"].toString();
        info.passwordHash = obj["password_hash"].toString();
        info.role = obj["role"].toString();
        if (!info.username.isEmpty())
            m_users[info.username] = info;
    }
    qDebug() << "[UserManager] Loaded" << m_users.size() << "users.";
}

void UserManager::save()
{
    QJsonArray arr;
    for (const UserInfo &info : m_users) {
        QJsonObject obj;
        obj["username"] = info.username;
        obj["password_hash"] = info.passwordHash;
        obj["role"] = info.role;
        arr.append(obj);
    }
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[UserManager] Failed to save user database to" << m_filePath;
        return;
    }
    file.write(QJsonDocument(arr).toJson());
    file.close();
}

QString UserManager::hashPassword(const QString &password)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex()
    );
}
