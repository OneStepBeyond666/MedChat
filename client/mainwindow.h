#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QList>
#include <QString>
#include <QCloseEvent>
#include <QStackedWidget>
#include <QSet>

class ChatClient;
class LeftSidebar;
class ChatWidget;
class ProfileDialog;
class FriendRequestWidget;
class FriendRequestNotification;
class NearbyPeopleWidget;
#include "chatclient.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(ChatClient *client, const QString &username, const QString &role,
                        const QString &nickname, const QByteArray &avatarData,
                        QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onContactSelected(const QString &username);
    void onSessionSelected(const QString &username);
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
    void onFileSendCompleted(const QString &fileId);
    void onFileAcceptFromUI(const QString &fileId);
    void onFileRejectFromUI(const QString &fileId);
    void onServerError(const QString &error);
    void onStrangerError(const QString &text);
    void onOfflineSyncDone();
    void onFriendRequestReceived(int requestId, const QString &from, const QString &nickname,
                                 const QString &text, const QByteArray &avatarData, bool isSynced);
    void onFriendRequestEntryClicked();
    void onAcceptFriendRequest(int requestId, const QString &fromUsername);
    void onRejectFriendRequest(int requestId, const QString &fromUsername);
    void onFriendRequestCountChanged(int count);
    void onFriendRequestConflict(const QString &target, const QString &direction,
                                 const QString &message, int requestId);
    void onOnlineUsersUpdated(const QMap<QString, ContactInfo> &onlineUsers);
    void onNearbyPeopleEntryClicked();
    void onNearbyAddFriendRequested(const QString &username);
    void onDisconnected();
    void onAvatarClicked();
    void onContactProfileChanged(const QString &username, const QString &nickname,
                                  const QByteArray &avatarData);
    void onAddFriendRequested();

private:
    void setupUI();
    void applyStyles();
    void showMessage(const QString &title, const QString &text);
    void loadMessagesFromDB(const QString &contactUid, const QString &partnerNick);
    void loadSessionsList();
    void persistTextMessage(const QString &contactUid, const QString &content,
                            qint64 timestamp, bool isMine);
    void persistFileMessage(const QString &contactUid, const QString &fileName,
                            const QString &fileId, qint64 timestamp, bool isMine);
    void updateSession(const QString &contactUid, const QString &preview, qint64 timestamp);
    QString displayName(const QString &username) const;
    void loadFriendRequests();

    ChatClient *m_client;
    QString m_username;
    QString m_role;
    QString m_nickname;
    QByteArray m_avatarData;
    LeftSidebar *m_sidebar;
    QStackedWidget *m_chatStack;
    ChatWidget *m_chatWidget;
    FriendRequestWidget *m_friendRequestWidget;
    NearbyPeopleWidget *m_nearbyPeopleWidget;

    // 好友请求: requestId -> fromUsername
    QMap<int, QString> m_pendingFriendRequests;

    // 文件接收映射: fileId -> from username
    QMap<QString, QString> m_pendingFileOffers;

    // 待显示的文件发送卡片: fileId -> to (发送文件后等待信号关联)
    QString m_pendingSendFileTo;
    QString m_pendingSendFilePath;  // 发送方原始文件路径，用于复制到本地存档

    // 本次会话中用户已点击接收的文件 ID（用于 stale 检测）
    QSet<QString> m_userAcceptedFiles;
};

#endif // MAINWINDOW_H
