#include "leftsidebar.h"
#include "sessionlistwidget.h"
#include "contactlistwidget.h"
#include "avatarcropper.h"
#include "client/chatclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

LeftSidebar::LeftSidebar(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    applyStyles();
}

void LeftSidebar::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ========== 顶部个人信息区 ==========
    QWidget *profileBar = new QWidget;
    profileBar->setObjectName("profileBar");
    profileBar->setFixedHeight(56);
    QHBoxLayout *profileLayout = new QHBoxLayout(profileBar);
    profileLayout->setContentsMargins(12, 8, 12, 8);
    profileLayout->setSpacing(8);

    m_avatarBtn = new QPushButton;
    m_avatarBtn->setFixedSize(40, 40);
    m_avatarBtn->setObjectName("avatarBtn");
    m_avatarBtn->setCursor(Qt::PointingHandCursor);
    m_avatarBtn->setToolTip("查看个人资料");
    connect(m_avatarBtn, &QPushButton::clicked, this, &LeftSidebar::onAvatarBtnClicked);
    profileLayout->addWidget(m_avatarBtn);

    m_nicknameLabel = new QLabel("未登录");
    m_nicknameLabel->setObjectName("nicknameLabel");
    m_nicknameLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #333;");
    profileLayout->addWidget(m_nicknameLabel);
    profileLayout->addStretch();

    mainLayout->addWidget(profileBar);

    // ========== Tab 按钮区 ==========
    QWidget *tabBar = new QWidget;
    tabBar->setObjectName("tabBar");
    tabBar->setFixedHeight(40);
    QHBoxLayout *tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);

    m_chatTabBtn = new QPushButton("聊天");
    m_chatTabBtn->setObjectName("tabBtn");
    m_chatTabBtn->setCheckable(true);
    m_chatTabBtn->setChecked(true);

    m_contactTabBtn = new QPushButton("通讯录");
    m_contactTabBtn->setObjectName("tabBtn");
    m_contactTabBtn->setCheckable(true);

    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);
    m_tabGroup->addButton(m_chatTabBtn, 0);
    m_tabGroup->addButton(m_contactTabBtn, 1);

    tabLayout->addWidget(m_chatTabBtn);
    tabLayout->addWidget(m_contactTabBtn);
    mainLayout->addWidget(tabBar);

    connect(m_tabGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &LeftSidebar::onTabChanged);

    // ========== 搜索框 ==========
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("  搜索...");
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setFixedHeight(36);
    m_searchEdit->setContentsMargins(8, 0, 8, 0);
    mainLayout->addWidget(m_searchEdit);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &LeftSidebar::onSearchChanged);

    // ========== 内容区（QStackedWidget） ==========
    m_stack = new QStackedWidget;
    m_sessionList = new SessionListWidget;
    m_contactList = new ContactListWidget;

    m_stack->addWidget(m_sessionList);   // index 0
    m_stack->addWidget(m_contactList);   // index 1

    mainLayout->addWidget(m_stack, 1);

    // 信号转发
    connect(m_sessionList, &SessionListWidget::sessionClicked,
            this, &LeftSidebar::onSessionClicked);
    connect(m_contactList, &ContactListWidget::contactSelected,
            this, &LeftSidebar::onContactClicked);
}

void LeftSidebar::applyStyles()
{
    setStyleSheet(
        "#profileBar { background-color: #e8e8e8; border-bottom: 1px solid #ddd; }"
        "#avatarBtn { background: transparent; border: 2px solid #ccc; border-radius: 20px; }"
        "#avatarBtn:hover { border: 2px solid #07c160; }"
        "#tabBar { background-color: #f0f0f0; border-bottom: 1px solid #ddd; }"
        "#tabBtn { background: transparent; border: none; font-size: 14px; color: #666; "
        "  padding: 8px 0; }"
        "#tabBtn:checked { color: #07c160; font-weight: bold; "
        "  border-bottom: 2px solid #07c160; }"
        "#searchEdit { border: none; border-bottom: 1px solid #e0e0e0; background: #f7f7f7; "
        "  border-radius: 4px; margin: 6px 8px; font-size: 13px; padding: 4px 8px; }"
    );
}

void LeftSidebar::setSelfProfile(const QString &nickname, const QByteArray &avatarData)
{
    m_selfNickname = nickname;
    m_selfAvatarData = avatarData;

    m_nicknameLabel->setText(nickname.isEmpty() ? "未设置昵称" : nickname);

    QPixmap avatar;
    if (!avatarData.isEmpty())
        avatar = AvatarCropper::roundAvatar(avatarData, 40);
    else
        avatar = AvatarCropper::defaultAvatar(nickname, 40);
    m_avatarBtn->setIcon(QIcon(avatar));
    m_avatarBtn->setIconSize(QSize(36, 36));
}

void LeftSidebar::setContacts(const QMap<QString, ContactInfo> &contacts)
{
    m_contactList->updateContacts(contacts);
}

void LeftSidebar::setSessions(const QVector<SessionInfo> &sessions)
{
    m_sessionList->setSessions(sessions);
}

void LeftSidebar::updateSession(const SessionInfo &session)
{
    m_sessionList->updateSession(session);
}

void LeftSidebar::refreshSessions()
{
    // 触发从 DB 重新加载（由外部 MainWindow 负责实际加载逻辑）
}

QString LeftSidebar::selectedContact() const
{
    return m_contactList->selectedContact();
}

void LeftSidebar::clearSelection()
{
    m_contactList->clearSelection();
}

void LeftSidebar::onTabChanged(int id)
{
    m_stack->setCurrentIndex(id);
    m_searchEdit->clear();
    if (id == 0)
        m_searchEdit->setPlaceholderText("  搜索聊天...");
    else
        m_searchEdit->setPlaceholderText("  搜索联系人...");
}

void LeftSidebar::onSearchChanged(const QString &text)
{
    int currentTab = m_stack->currentIndex();
    if (currentTab == 0)
        m_sessionList->filter(text);
    else
        m_contactList->filter(text);
}

void LeftSidebar::onAvatarBtnClicked()
{
    emit avatarClicked();
}

void LeftSidebar::onSessionClicked(const QString &username)
{
    emit sessionSelected(username);
}

void LeftSidebar::onContactClicked(const QString &username)
{
    emit contactSelected(username);
}
