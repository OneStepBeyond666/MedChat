#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QList>
#include <QString>
#include <QCloseEvent>
#include <QStackedWidget>

class ChatClient;
class ContactListWidget;
class ChatWidget;
#include "chatclient.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(ChatClient *client, const QString &username, const QString &role,
                        const QString &nickname, QWidget *parent = nullptr);

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
    void onFileSizeExceeded(const QString &fileId, const QString &fileName, qint64 fileSize);
    void onFileSendInitiated(const QString &to, const QString &fileName,
                              qint64 fileSize, const QString &fileId);
    void onFileAcceptFromUI(const QString &fileId);
    void onFileRejectFromUI(const QString &fileId);
    void onServerError(const QString &error);
    void onDisconnected();

private:
    void setupUI();
    void applyStyles();
    void showMessage(const QString &title, const QString &text);
    void loadMessagesFromDB(const QString &contactUid);
    void persistTextMessage(const QString &contactUid, const QString &content,
                            qint64 timestamp, bool isMine);
    void persistFileMessage(const QString &contactUid, const QString &fileName,
                            const QString &fileId, qint64 timestamp, bool isMine);
    void updateSession(const QString &contactUid, const QString &preview, qint64 timestamp);
    QString displayName(const QString &username) const;

    ChatClient *m_client;
    QString m_username;
    QString m_role;
    QString m_nickname;
    ContactListWidget *m_contactList;
    QStackedWidget *m_chatStack;
    ChatWidget *m_chatWidget;

    // 文件接收映射: fileId -> from username
    QMap<QString, QString> m_pendingFileOffers;

    // 待显示的文件发送卡片: fileId -> to (发送文件后等待信号关联)
    QString m_pendingSendFileTo;
};

#endif // MAINWINDOW_H
