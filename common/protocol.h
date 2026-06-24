#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace MsgType {
    // 用户管理
    constexpr const char* Register     = "register";
    constexpr const char* Login        = "login";
    constexpr const char* Logout       = "logout";
    constexpr const char* AuthResult   = "auth_result";

    // 联系人与状态
    constexpr const char* ContactList  = "contact_list";
    constexpr const char* OnlineStatus = "online_status";

    // 文本消息
    constexpr const char* Message      = "message";

    // 文件传输
    constexpr const char* FileOffer    = "file_offer";
    constexpr const char* FileAccept   = "file_accept";
    constexpr const char* FileData     = "file_data";
    constexpr const char* FileEnd      = "file_end";
    constexpr const char* FileReject   = "file_reject";

    // 系统和错误
    constexpr const char* Error        = "error";
    constexpr const char* ErrorStranger = "error_stranger";

    // 好友关系
    constexpr const char* FriendRequest  = "friend_request";
    constexpr const char* FriendResponse = "friend_response";
    constexpr const char* SyncFriends    = "sync_friends";

    // 离线同步
    constexpr const char* OfflineSync    = "offline_sync";
    constexpr const char* OfflineSyncAck = "offline_sync_ack";

    // 资料更新
    constexpr const char* UpdateProfile  = "update_profile";
    constexpr const char* ProfileUpdated = "profile_updated";

    // 特殊联系人
    constexpr const char* FileHelper   = "\xe6\x96\x87\xe4\xbb\xb6\xe4\xbc\xa0\xe8\xbe\x93\xe5\x8a\xa9\xe6\x89\x8b";
}

namespace Role {
    constexpr const char* Doctor  = "doctor";
    constexpr const char* Patient = "patient";
}

// 协议工具：包头4字节(网络字节序) + JSON body
class Protocol {
public:
    // 将 JSON 对象编码为协议帧
    static QByteArray encode(const QJsonObject &obj);

    // 尝试从缓冲区解码一帧，成功返回 JSON 对象并消费已用字节，失败返回空对象
    static QJsonObject decode(QByteArray &buffer, bool &ok);

    // 快捷构造
    static QJsonObject makeMsg(const QString &type);
    static QJsonObject makeError(const QString &text);
    static QJsonObject makeAuthResult(bool success, const QString &msg, const QString &role = QString());
    static QJsonObject makeMessage(const QString &from, const QString &to, const QString &text);
    static QJsonObject makeFileOffer(const QString &from, const QString &to,
                                     const QString &fileName, qint64 fileSize, const QString &fileId);
    static QJsonObject makeFileAccept(const QString &from, const QString &to, const QString &fileId);
    static QJsonObject makeFileReject(const QString &from, const QString &to, const QString &fileId, const QString &reason);
    static QJsonObject makeFileData(const QString &fileId, const QByteArray &chunk, int seq);
    static QJsonObject makeFileEnd(const QString &fileId, const QString &md5);
};

#endif // PROTOCOL_H
