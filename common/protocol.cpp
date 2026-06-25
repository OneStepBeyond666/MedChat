#include "protocol.h"
#include <QDataStream>
#include <QDateTime>
#include <QtEndian>

QByteArray Protocol::encode(const QJsonObject &obj)
{
    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    quint32 len = static_cast<quint32>(body.size());
    QByteArray frame;
    frame.reserve(4 + body.size());
    quint32 netLen = qToBigEndian(len);
    frame.append(reinterpret_cast<const char*>(&netLen), 4);
    frame.append(body);
    return frame;
}

QJsonObject Protocol::decode(QByteArray &buffer, bool &ok)
{
    ok = false;
    if (buffer.size() < 4)
        return QJsonObject();

    quint32 netLen;
    memcpy(&netLen, buffer.constData(), 4);
    quint32 len = qFromBigEndian(netLen);

    if (len > 100 * 1024 * 1024) { // 超过 100MB 视为异常
        buffer.clear();
        return QJsonObject();
    }

    if (static_cast<quint32>(buffer.size() - 4) < len)
        return QJsonObject();

    QByteArray body = buffer.mid(4, len);
    buffer.remove(0, 4 + len);

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError)
        return QJsonObject();

    ok = true;
    return doc.object();
}

QJsonObject Protocol::makeMsg(const QString &type)
{
    QJsonObject obj;
    obj["type"] = type;
    return obj;
}

QJsonObject Protocol::makeError(const QString &text)
{
    QJsonObject obj = makeMsg(MsgType::Error);
    obj["text"] = text;
    return obj;
}

QJsonObject Protocol::makeAuthResult(bool success, const QString &msg, const QString &role)
{
    QJsonObject obj = makeMsg(MsgType::AuthResult);
    obj["success"] = success;
    obj["message"] = msg;
    if (!role.isEmpty())
        obj["role"] = role;
    return obj;
}

QJsonObject Protocol::makeMessage(const QString &from, const QString &to, const QString &text)
{
    QJsonObject obj = makeMsg(MsgType::Message);
    obj["from"] = from;
    obj["to"] = to;
    obj["text"] = text;
    obj["time"] = QDateTime::currentMSecsSinceEpoch();
    return obj;
}

QJsonObject Protocol::makeRecallMessage(const QString &from, const QString &to, qint64 originalTimestamp)
{
    QJsonObject obj = makeMsg(MsgType::RecallMessage);
    obj["from"] = from;
    obj["to"] = to;
    obj["original_timestamp"] = originalTimestamp;
    return obj;
}

QJsonObject Protocol::makeFileOffer(const QString &from, const QString &to,
                                    const QString &fileName, qint64 fileSize, const QString &fileId)
{
    QJsonObject obj = makeMsg(MsgType::FileOffer);
    obj["from"] = from;
    obj["to"] = to;
    obj["file_name"] = fileName;
    obj["file_size"] = fileSize;
    obj["file_id"] = fileId;
    return obj;
}

QJsonObject Protocol::makeFileAccept(const QString &from, const QString &to, const QString &fileId)
{
    QJsonObject obj = makeMsg(MsgType::FileAccept);
    obj["from"] = from;
    obj["to"] = to;
    obj["file_id"] = fileId;
    return obj;
}

QJsonObject Protocol::makeFileReject(const QString &from, const QString &to, const QString &fileId, const QString &reason)
{
    QJsonObject obj = makeMsg(MsgType::FileReject);
    obj["from"] = from;
    obj["to"] = to;
    obj["file_id"] = fileId;
    obj["reason"] = reason;
    return obj;
}

QJsonObject Protocol::makeFileData(const QString &fileId, const QByteArray &chunk, int seq)
{
    QJsonObject obj = makeMsg(MsgType::FileData);
    obj["file_id"] = fileId;
    obj["seq"] = seq;
    obj["data"] = QString::fromLatin1(chunk.toBase64());
    return obj;
}

QJsonObject Protocol::makeFileEnd(const QString &fileId, const QString &md5)
{
    QJsonObject obj = makeMsg(MsgType::FileEnd);
    obj["file_id"] = fileId;
    obj["md5"] = md5;
    return obj;
}

QJsonObject Protocol::makeSecQuestionRes(bool success, const QString &question, const QString &error)
{
    QJsonObject obj = makeMsg(MsgType::SecQuestionRes);
    obj["success"] = success;
    if (!question.isEmpty())
        obj["question"] = question;
    if (!error.isEmpty())
        obj["error"] = error;
    return obj;
}

QJsonObject Protocol::makeResetPasswordRes(bool success, const QString &message)
{
    QJsonObject obj = makeMsg(MsgType::ResetPasswordRes);
    obj["success"] = success;
    obj["message"] = message;
    return obj;
}

QJsonObject Protocol::makeChangePasswordRes(bool success, const QString &message)
{
    QJsonObject obj = makeMsg(MsgType::ChangePasswordRes);
    obj["success"] = success;
    obj["message"] = message;
    return obj;
}
