#include "chathistory.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QDebug>

ChatHistory::ChatHistory(const QString &baseDir, QObject *parent)
    : QObject(parent), m_baseDir(baseDir)
{
    QDir dir(m_baseDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

void ChatHistory::saveHistory(const QString &myUsername, const QString &partner,
                               const QList<HistoryMessage> &messages)
{
    if (messages.isEmpty()) return;

    QString filePath = historyFilePath(myUsername, partner);
    QJsonArray arr;
    for (const HistoryMessage &msg : messages) {
        QJsonObject obj;
        obj["type"] = msg.type;
        obj["sender"] = msg.sender;
        obj["text"] = msg.text;
        obj["timestamp"] = static_cast<double>(msg.timestamp);
        obj["is_mine"] = msg.isMine;
        if (msg.type == 1) {
            obj["file_name"] = msg.fileName;
            obj["file_size"] = static_cast<double>(msg.fileSize);
        }
        arr.append(obj);
    }

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(arr).toJson());
        file.close();
        qDebug() << "[ChatHistory] Saved" << messages.size() << "messages for" << partner;
    } else {
        qWarning() << "[ChatHistory] Failed to save history to" << filePath;
    }
}

QList<HistoryMessage> ChatHistory::loadHistory(const QString &myUsername, const QString &partner)
{
    QList<HistoryMessage> result;
    QString filePath = historyFilePath(myUsername, partner);

    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return result;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) return result;

    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        HistoryMessage msg;
        msg.type = obj["type"].toInt();
        msg.sender = obj["sender"].toString();
        msg.text = obj["text"].toString();
        msg.timestamp = static_cast<qint64>(obj["timestamp"].toDouble());
        msg.isMine = obj["is_mine"].toBool();
        msg.fileName = obj["file_name"].toString();
        msg.fileSize = static_cast<qint64>(obj["file_size"].toDouble());
        result.append(msg);
    }

    qDebug() << "[ChatHistory] Loaded" << result.size() << "messages for" << partner;
    return result;
}

QString ChatHistory::historyFilePath(const QString &myUsername, const QString &partner) const
{
    // 用 MD5 哈希避免文件名特殊字符问题
    QString raw = myUsername + "_" + partner;
    QString hash = QString::fromLatin1(
        QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Md5).toHex().left(12));
    QString safeName = partner;
    // 替换文件系统不允许的字符
    safeName.replace(QRegExp("[<>:\"/\\\\|?*]"), "_");
    return m_baseDir + "/" + safeName + "_" + hash + ".json";
}
