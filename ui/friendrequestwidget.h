#ifndef FRIENDREQUESTWIDGET_H
#define FRIENDREQUESTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVector>
#include <QByteArray>

struct FriendRequestInfo;

/// 右侧好友请求面板：显示待处理请求列表，每项含头像/昵称/留言/接受/拒绝
class FriendRequestWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FriendRequestWidget(QWidget *parent = nullptr);

    /// 设置请求列表（全量替换）
    void setRequests(const QVector<FriendRequestInfo> &requests);

    /// 移除指定请求项（接受/拒绝后调用）
    void removeRequest(int requestId);

    /// 清空列表
    void clear();

signals:
    void acceptRequested(int requestId, const QString &fromUsername);
    void rejectRequested(int requestId, const QString &fromUsername);

private:
    void setupUI();
    void applyStyles();
    static QPushButton *makeActionBtn(const QString &text, const QString &color);

    QListWidget *m_listWidget;
};

#endif // FRIENDREQUESTWIDGET_H
