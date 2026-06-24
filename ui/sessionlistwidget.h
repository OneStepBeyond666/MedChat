#ifndef SESSIONLISTWIDGET_H
#define SESSIONLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QMap>
#include "client/localdb.h"

class SessionListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SessionListWidget(QWidget *parent = nullptr);

    /// 设置会话列表（含昵称和头像信息）
    void setSessions(const QVector<SessionInfo> &sessions);

    /// 更新单个会话
    void updateSession(const SessionInfo &session);

    /// 过滤搜索
    void filter(const QString &text);

signals:
    void sessionClicked(const QString &username);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    void setupUI();
    void applyStyles();
    QListWidgetItem *createSessionItem(const SessionInfo &s);

    QListWidget *m_listWidget;
    QVector<SessionInfo> m_sessions;
    QString m_filterText;
};

#endif // SESSIONLISTWIDGET_H
