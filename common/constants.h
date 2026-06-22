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
constexpr qint64 MAX_FILE_SIZE      = 104857600; // 100 MB 文件传输上限
constexpr int FILE_CHUNK_SIZE       = 65536;     // 64 KB 分块大小

// --- 头像 ---
constexpr qint64 AVATAR_MAX_SIZE    = 2097152;   // 头像上传限制 2 MB
constexpr int AVATAR_DISPLAY_SIZE   = 200;       // 头像显示标准像素

// --- 存储 ---
const QString DB_FOLDER_NAME        = QStringLiteral("medchat_data"); // 数据库文件夹名
const QString HISTORY_FOLDER_NAME   = QStringLiteral("history");      // 聊天历史文件夹名

} // namespace Constants

#endif // CONSTANTS_H
