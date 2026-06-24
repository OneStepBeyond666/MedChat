#ifndef FRIENDREQUESTNOTIFICATION_H
#define FRIENDREQUESTNOTIFICATION_H

#include <QWidget>
#include <QTimer>

/// 好友请求通知弹窗：右上角弹出，10s 自动消失
class FriendRequestNotification : public QWidget
{
    Q_OBJECT
public:
    explicit FriendRequestNotification(const QString &nickname,
                                       const QByteArray &avatarData,
                                       QWidget *parent = nullptr);

signals:
    void viewRequested();   // 用户点击"查看"

private:
    void setupUI();

    QTimer *m_autoCloseTimer;
};

#endif // FRIENDREQUESTNOTIFICATION_H
