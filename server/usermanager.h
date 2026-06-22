#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QMap>

struct UserInfo {
    QString username;
    QString passwordHash;
    QString role; // "doctor" or "patient"
};

class UserManager : public QObject
{
    Q_OBJECT
public:
    explicit UserManager(const QString &filePath = "users.json", QObject *parent = nullptr);
    ~UserManager();

    bool registerUser(const QString &username, const QString &password, const QString &role);
    bool authenticate(const QString &username, const QString &password, QString &role) const;
    bool userExists(const QString &username) const;
    QList<UserInfo> allUsers() const;

private:
    void load();
    void save();
    static QString hashPassword(const QString &password);

    QString m_filePath;
    QMap<QString, UserInfo> m_users;
};

#endif // USERMANAGER_H
