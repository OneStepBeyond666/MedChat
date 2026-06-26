#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>

// ============================================================
// MedChat 全局常量定义
// 所有模块共享的约束值，集中维护便于统一修改
// ============================================================

namespace Constants {

// --- 网络 ---
constexpr int DEFAULT_PORT          = 9527;      // 默认监听端口
constexpr int MAX_FRAME_SIZE        = 104857600; // 单帧上限 100 MB

// --- 文件传输 ---
constexpr qint64 MAX_FILE_SIZE         = 104857600; // 100 MB 文件传输上限
constexpr qint64 OFFLINE_FILE_MAX_SIZE = 10485760;  // 10 MB 离线文件上限
constexpr int FILE_CHUNK_SIZE          = 65536;     // 64 KB 分块大小
constexpr int OFFLINE_FILE_EXPIRE_DAYS = 7;        // 离线文件过期天数

// --- 头像 ---
constexpr qint64 AVATAR_MAX_SIZE    = 2097152;   // 头像上传限制 2 MB
constexpr int AVATAR_DISPLAY_SIZE   = 200;       // 头像显示标准像素

// --- 消息撤回 ---
constexpr qint64 RECALL_TIME_LIMIT    = 120000;    // 撤回时限 2 分钟（毫秒）

// --- 存储 ---
const QString DB_FOLDER_NAME        = QStringLiteral("medchat_data"); // 服务端数据库文件夹名

// --- 客户端本地存储 ---
const QString FILE_ROOT_NAME        = QStringLiteral("medchat_file"); // 客户端文件根目录
const QString MSG_SUBDIR            = QStringLiteral("msg");          // 消息数据库子目录
const QString FILE_SUBDIR           = QStringLiteral("file");         // 文件存储子目录
const QString TEMP_SUBDIR           = QStringLiteral("temp");         // 临时文件子目录
const QString META_DB_NAME          = QStringLiteral("meta.db");      // 索引库
const QString MSG_DB_NAME           = QStringLiteral("messages.db");  // 消息库

} // namespace Constants

#endif // CONSTANTS_H
