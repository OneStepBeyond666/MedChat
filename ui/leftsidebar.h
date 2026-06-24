#ifndef LEFTSIDEBAR_H
#define LEFTSIDEBAR_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QByteArray>
#include <QMap>

class SessionListWidget;
class ContactListWidget;
class ContactInfo;

class LeftSidebar : public QWidget
{
    Q_OBJECT
public:
    explicit LeftSidebar(QWidget *parent = nullptr);

    /// 设置自身头像和昵称
    void setSelfProfile(const QString &nickname, const QByteArray &avatarData);

    /// 设置联系人列表（通讯录 Tab）
    void setContacts(const QMap<QString, ContactInfo> &contacts);

    /// 设置会话列表（聊天 Tab）
    void setSessions(const QVector<struct SessionInfo> &sessions);

    /// 更新单个会话
    void updateSession(const struct SessionInfo &session);

    /// 刷新会话列表（从外部触发重新加载）
    void refreshSessions();

    /// 获取当前选中的联系人（通讯录 Tab）
    QString selectedContact() const;

    /// 清除选择
    void clearSelection();

signals:
    void contactSelected(const QString &username);     // 从通讯录 Tab 点击
    void sessionSelected(const QString &username);     // 从聊天 Tab 点击
    void avatarClicked();                               // 点击头像打开资料窗口
    void profileUpdateRequested(const QString &nickname, const QByteArray &avatarData);

private slots:
    void onTabChanged(int id);
    void onSearchChanged(const QString &text);
    void onAvatarBtnClicked();
    void onSessionClicked(const QString &username);
    void onContactClicked(const QString &username);

private:
    void setupUI();
    void applyStyles();

    // 顶部个人信息区
    QPushButton *m_avatarBtn;
    QLabel *m_nicknameLabel;

    // Tab 切换
    QPushButton *m_chatTabBtn;
    QPushButton *m_contactTabBtn;
    QButtonGroup *m_tabGroup;

    // 搜索框
    QLineEdit *m_searchEdit;

    // 内容区
    QStackedWidget *m_stack;
    SessionListWidget *m_sessionList;
    ContactListWidget *m_contactList;

    // 状态
    QByteArray m_selfAvatarData;
    QString m_selfNickname;
};

#endif // LEFTSIDEBAR_H
