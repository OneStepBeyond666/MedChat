#ifndef NEARBYPEOPLEWIDGET_H
#define NEARBYPEOPLEWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QMap>
#include <QSet>
#include "client/chatclient.h"

/// 右侧"附近的人"面板：显示在线非好友用户，含"添加好友"按钮
class NearbyPeopleWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NearbyPeopleWidget(QWidget *parent = nullptr);

    /// 设置在线非好友列表（全量替换）
    void setStrangers(const QMap<QString, ContactInfo> &strangers);

    /// 标记某用户已发送好友请求（按钮变灰）
    void markRequestSent(const QString &username);

    /// 清空列表
    void clear();

signals:
    void addFriendRequested(const QString &username);

private:
    void setupUI();
    void applyStyles();

    QListWidget *m_listWidget;
    QSet<QString> m_pendingRequests;  // 已发送请求的用户名
};

#endif // NEARBYPEOPLEWIDGET_H
