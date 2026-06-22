#ifndef CHATHISTORY_H
#define CHATHISTORY_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

struct HistoryMessage {
    int type;        // 0 = text, 1 = file
    QString sender;
    QString text;
    qint64 timestamp;
    bool isMine;
    QString fileName;
    qint64 fileSize;
};

class ChatHistory : public QObject
{
    Q_OBJECT
public:
    explicit ChatHistory(const QString &baseDir, QObject *parent = nullptr);

    void saveHistory(const QString &myUsername, const QString &partner,
                     const QList<HistoryMessage> &messages);
    QList<HistoryMessage> loadHistory(const QString &myUsername, const QString &partner);

private:
    QString historyFilePath(const QString &myUsername, const QString &partner) const;

    QString m_baseDir;
};

#endif // CHATHISTORY_H
