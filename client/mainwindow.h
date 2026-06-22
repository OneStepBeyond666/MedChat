#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QPair>
#include <QList>
#include <QString>
#include <QCloseEvent>
#include <QStackedWidget>

class ChatClient;
class ContactListWidget;
class ChatWidget;
class ChatHistory;
#include "chatclient.h"

// 消息缓冲条目 —— 与 HistoryMessage 字段一致
struct MsgBufEntry {
    int type;        // 0 = text, 1 = file
    QString sender;
    QString text;
    qint64 timestamp;
    bool isMine;
    QString fileName;
    qint64 fileSize;
    QString fileId;  // 文件传输标识，文本消息为空
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(ChatClient *client, const QString &username, const QString &role,
                        QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onContactSelected(const QString &username);
    void onContactListUpdated(const QMap<QString, ContactInfo> &contacts);
    void onTextMessageReceived(const QString &from, const QString &to,
                               const QString &text, qint64 timestamp);
    void onMessageAck(const QString &to, qint64 timestamp);
    void onSendTextMessage(const QString &to, const QString &text);
    void onSendFileRequest(const QString &to);
    void onFileOfferReceived(const QString &from, const QString &fileName,
                              qint64 fileSize, const QString &fileId);
    void onFileAccepted(const QString &fileId);
    void onFileRejected(const QString &fileId, const QString &reason);
    void onFileProgress(const QString &fileId, qint64 received, qint64 total);
    void onFileCompleted(const QString &fileId, const QString &savePath);
    void onFileError(const QString &fileId, const QString &error);
    void onFileAcceptFromUI(const QString &fileId);
    void onFileRejectFromUI(const QString &fileId);
    void onServerError(const QString &error);
    void onDisconnected();

private:
    void setupUI();
    void applyStyles();
    void showMessage(const QString &title, const QString &text);
    void saveCurrentHistory();
    void flushBufferToUI(const QString &partner);
    void saveAllBuffers();

    ChatClient *m_client;
    QString m_username;
    QString m_role;
    ContactListWidget *m_contactList;
    QStackedWidget *m_chatStack;
    ChatWidget *m_chatWidget;
    ChatHistory *m_history;

    // 消息缓冲区：按联系人存储未合并到视图的消息
    QMap<QString, QList<MsgBufEntry>> m_msgBuffers;

    // 文件接收映射: fileId -> from username
    QMap<QString, QString> m_pendingFileOffers;
};

#endif // MAINWINDOW_H
