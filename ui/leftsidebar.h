#ifndef LEFTSIDEBAR_H
#define LEFTSIDEBAR_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QByteArray>
#include <QMap>

class SessionListWidget;
class ContactListWidget;
class ContactInfo;
class QMenu;

// ============================================================
// 微信风格左侧栏：IconBar(55px深色) + ContentPanel(浅色)
// ============================================================
class LeftSidebar : public QWidget
{
    Q_OBJECT
public:
    explicit LeftSidebar(QWidget *parent = nullptr);

    void setSelfProfile(const QString &nickname, const QByteArray &avatarData);
    void setContacts(const QMap<QString, ContactInfo> &contacts);
    void setSessions(const QVector<struct SessionInfo> &sessions);
    void updateSession(const struct SessionInfo &session);
    void refreshSessions();
    QString selectedContact() const;
    void clearSelection();

signals:
    void contactSelected(const QString &username);
    void sessionSelected(const QString &username);
    void avatarClicked();
    void addFriendRequested();

private slots:
    void onIconClicked(int id);
    void onSearchChanged(const QString &text);
    void onAvatarBtnClicked();
    void onSessionClicked(const QString &username);
    void onContactClicked(const QString &username);
    void onPlusMenuClicked();

private:
    void setupUI();
    void applyStyles();

    // ---- IconBar (左侧深色导航栏) ----
    QWidget    *m_iconBar;
    QPushButton *m_avatarBtn;       // 顶部头像
    QPushButton *m_chatIconBtn;     // 聊天图标
    QPushButton *m_contactIconBtn;  // 通讯录图标
    QButtonGroup *m_iconGroup;

    // ---- ContentPanel (右侧内容面板) ----
    QWidget      *m_contentPanel;
    QLineEdit    *m_searchEdit;
    QToolButton  *m_plusBtn;
    QMenu        *m_plusMenu;
    QStackedWidget *m_stack;
    SessionListWidget  *m_sessionList;
    ContactListWidget  *m_contactList;

    // 状态
    QByteArray m_selfAvatarData;
    QString m_selfNickname;
    int m_currentTab = 0;  // 0=聊天, 1=通讯录
};

#endif // LEFTSIDEBAR_H
