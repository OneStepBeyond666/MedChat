#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>
#include <QByteArray>

class ServerDB;

/// 用户信息结构（供外部查询使用）
struct UserInfo {
    int uid = 0;
    QString username;
    QString nickname;
    QString role;           // "doctor" | "patient"
    QByteArray avatarBlob;  // 可能为空
};

class UserManager : public QObject
{
    Q_OBJECT
public:
    explicit UserManager(ServerDB *db, QObject *parent = nullptr);

    // ---- 核心接口（与旧版兼容） ----
    bool registerUser(const QString &username, const QString &password, const QString &role);
    bool authenticate(const QString &username, const QString &password, QString &role) const;
    bool userExists(const QString &username) const;
    QList<UserInfo> allUsers() const;

    // ---- 新增接口 ----
    int  getUidByUsername(const QString &username) const;
    UserInfo getUserInfo(int uid) const;
    UserInfo getUserInfoByName(const QString &username) const;

    /// 更新昵称
    bool updateNickname(const QString &username, const QString &nickname);

    /// 更新头像（传入原始字节，内部做大小校验 + 缩放）
    bool updateAvatar(const QString &username, const QByteArray &rawAvatar);

    /// 获取头像二进制数据
    QByteArray getAvatar(int uid) const;

private:
    /// 生成随机盐（32 字节 hex = 64 字符）
    static QString generateSalt();

    /// 带盐哈希：SHA-256(salt + password) → hex
    static QString hashPassword(const QString &password, const QString &salt);

    /// 头像预处理：校验大小 + 缩放到标准尺寸，返回处理后的 PNG 字节
    static QByteArray processAvatar(const QByteArray &rawData, bool &ok);

    ServerDB *m_db;
};

#endif // USERMANAGER_H
