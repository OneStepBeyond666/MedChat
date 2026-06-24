#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>
#include <QByteArray>
#include <QJsonObject>

class ServerDB;

/// 用户信息结构（供外部查询使用）
struct UserInfo {
    int uid = 0;
    QString username;
    QString nickname;
    QString role;           // "doctor" | "patient"
    QByteArray avatarBlob;  // 可能为空
    QString signature;      // 个性签名
    int gender = 0;        // 0=未知, 1=男, 2=女
    QString birthday;      // YYYY-MM-DD
    QString region;        // 地区
};

class UserManager : public QObject
{
    Q_OBJECT
public:
    explicit UserManager(ServerDB *db, QObject *parent = nullptr);

    // ---- 核心接口（与旧版兼容） ----
    bool registerUser(const QString &username, const QString &password, const QString &role, const QString &nickname,
                      const QString &secQuestion = QString(), const QString &secAnswerHash = QString());
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

    /// 更新个性签名
    bool updateSignature(const QString &username, const QString &signature);

    /// 更新性别
    bool updateGender(const QString &username, int gender);

    /// 更新生日
    bool updateBirthday(const QString &username, const QString &birthday);

    /// 更新地区
    bool updateRegion(const QString &username, const QString &region);

    /// 获取头像二进制数据
    QByteArray getAvatar(int uid) const;

    // ---- 密码安全 ----
    /// 获取密保问题
    QJsonObject getSecurityQuestion(const QString &username) const;

    /// 重置密码（校验密保答案）
    bool resetPassword(const QString &username, const QString &answerHash, const QString &newPassword);

    /// 修改密码（校验旧密码）
    bool changePassword(int uid, const QString &oldPassword, const QString &newPassword);

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
