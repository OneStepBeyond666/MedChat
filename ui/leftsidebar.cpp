#include "leftsidebar.h"
#include "sessionlistwidget.h"
#include "contactlistwidget.h"
#include "avatarcropper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QPixmap>
#include <QPainter>
#include <QAction>
#include <QMenu>

// ============================================================
// 构造辅助：绘制纯色图标 QPixmap（用于无 FontAwesome 场景）
// ============================================================
static QPixmap makeIconPixmap(int size, const QColor &color, const QString &shape)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);

    if (shape == "chat") {
        // 聊天气泡：圆角矩形 + 小三角
        QRectF bubble(3, 2, size - 6, size - 8);
        p.drawRoundedRect(bubble, 3, 3);
        QPolygonF tail;
        tail << QPointF(6, size - 6) << QPointF(10, size - 6) << QPointF(6, size - 2);
        p.drawPolygon(tail);
    } else if (shape == "contacts") {
        // 联系人：圆形头 + 半圆身体
        p.drawEllipse(QPointF(size / 2.0, size / 3.0), size / 4.5, size / 4.5);
        QPainterPath body;
        body.moveTo(2, size - 2);
        body.arcTo(QRectF(2, size / 2.0 - 2, size - 4, size / 1.5), 180, -180);
        p.drawPath(body);
    } else if (shape == "plus") {
        // 加号
        QRectF r(0, 0, size, size);
        p.setBrush(Qt::NoBrush);
        QPen pen(color, 2.5);
        p.setPen(pen);
        int mid = size / 2;
        p.drawLine(QPoint(mid, 4), QPoint(mid, size - 4));
        p.drawLine(QPoint(4, mid), QPoint(size - 4, mid));
    }
    p.end();
    return pm;
}

// ============================================================
LeftSidebar::LeftSidebar(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    applyStyles();
}

void LeftSidebar::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ==================== IconBar (55px 深色) ====================
    m_iconBar = new QWidget;
    m_iconBar->setObjectName("iconBar");
    m_iconBar->setFixedWidth(55);
    QVBoxLayout *iconLayout = new QVBoxLayout(m_iconBar);
    iconLayout->setContentsMargins(0, 10, 0, 10);
    iconLayout->setSpacing(6);
    iconLayout->setAlignment(Qt::AlignHCenter);

    // 顶部头像
    m_avatarBtn = new QPushButton;
    m_avatarBtn->setFixedSize(40, 40);
    m_avatarBtn->setObjectName("iconBarAvatar");
    m_avatarBtn->setCursor(Qt::PointingHandCursor);
    m_avatarBtn->setToolTip("个人资料");
    connect(m_avatarBtn, &QPushButton::clicked, this, &LeftSidebar::onAvatarBtnClicked);
    iconLayout->addWidget(m_avatarBtn);
    iconLayout->addSpacing(12);

    // 聊天图标按钮
    m_chatIconBtn = new QPushButton;
    m_chatIconBtn->setFixedSize(40, 40);
    m_chatIconBtn->setObjectName("iconBtn");
    m_chatIconBtn->setCheckable(true);
    m_chatIconBtn->setChecked(true);
    m_chatIconBtn->setCursor(Qt::PointingHandCursor);
    m_chatIconBtn->setToolTip("聊天");
    QPixmap chatIcon = makeIconPixmap(28, QColor("#CCCCCC"), "chat");
    m_chatIconBtn->setIcon(QIcon(chatIcon));
    m_chatIconBtn->setIconSize(QSize(28, 28));

    // 通讯录图标按钮
    m_contactIconBtn = new QPushButton;
    m_contactIconBtn->setFixedSize(40, 40);
    m_contactIconBtn->setObjectName("iconBtn");
    m_contactIconBtn->setCheckable(true);
    m_contactIconBtn->setCursor(Qt::PointingHandCursor);
    m_contactIconBtn->setToolTip("通讯录");
    QPixmap contactIcon = makeIconPixmap(28, QColor("#CCCCCC"), "contacts");
    m_contactIconBtn->setIcon(QIcon(contactIcon));
    m_contactIconBtn->setIconSize(QSize(28, 28));

    m_iconGroup = new QButtonGroup(this);
    m_iconGroup->setExclusive(true);
    m_iconGroup->addButton(m_chatIconBtn, 0);
    m_iconGroup->addButton(m_contactIconBtn, 1);

    iconLayout->addWidget(m_chatIconBtn);
    iconLayout->addWidget(m_contactIconBtn);
    iconLayout->addStretch();

    // 底部设置占位
    QWidget *settingsPlaceholder = new QWidget;
    settingsPlaceholder->setFixedSize(40, 40);
    iconLayout->addWidget(settingsPlaceholder);

    connect(m_iconGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &LeftSidebar::onIconClicked);

    mainLayout->addWidget(m_iconBar);

    // ==================== ContentPanel (浅色面板) ====================
    m_contentPanel = new QWidget;
    m_contentPanel->setObjectName("contentPanel");
    QVBoxLayout *contentLayout = new QVBoxLayout(m_contentPanel);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // 顶部栏：搜索框 + "+" 按钮
    QWidget *topBar = new QWidget;
    topBar->setObjectName("contentTopBar");
    topBar->setFixedHeight(44);
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(8, 6, 8, 6);
    topLayout->setSpacing(4);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("搜索");
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setFixedHeight(30);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &LeftSidebar::onSearchChanged);
    topLayout->addWidget(m_searchEdit);

    // "+" 号按钮
    m_plusBtn = new QToolButton;
    m_plusBtn->setObjectName("plusBtn");
    m_plusBtn->setFixedSize(30, 30);
    m_plusBtn->setCursor(Qt::PointingHandCursor);
    QPixmap plusIcon = makeIconPixmap(20, QColor("#666666"), "plus");
    m_plusBtn->setIcon(QIcon(plusIcon));
    m_plusBtn->setIconSize(QSize(20, 20));
    topLayout->addWidget(m_plusBtn);

    // "+" 号下拉菜单
    m_plusMenu = new QMenu(this);
    QAction *addFriendAction = m_plusMenu->addAction("添加朋友");
    connect(addFriendAction, &QAction::triggered, this, [this]() {
        emit addFriendRequested();
    });
    // "发起群聊" 暂灰显
    QAction *createGroupAction = m_plusMenu->addAction("发起群聊");
    createGroupAction->setEnabled(false);

    connect(m_plusBtn, &QToolButton::clicked, this, &LeftSidebar::onPlusMenuClicked);

    contentLayout->addWidget(topBar);

    // 内容区 QStackedWidget
    m_stack = new QStackedWidget;
    m_sessionList = new SessionListWidget;
    m_contactList = new ContactListWidget;

    m_stack->addWidget(m_sessionList);   // index 0
    m_stack->addWidget(m_contactList);   // index 1

    contentLayout->addWidget(m_stack, 1);

    mainLayout->addWidget(m_contentPanel, 1);

    // 信号转发
    connect(m_sessionList, &SessionListWidget::sessionClicked,
            this, &LeftSidebar::onSessionClicked);
    connect(m_contactList, &ContactListWidget::contactSelected,
            this, &LeftSidebar::onContactClicked);
    connect(m_contactList, &ContactListWidget::friendRequestEntryClicked,
            this, &LeftSidebar::friendRequestEntryClicked);
}

void LeftSidebar::applyStyles()
{
    setStyleSheet(
        // IconBar 深色背景
        "#iconBar { background-color: #2E2E2E; }"
        "#iconBarAvatar { background: transparent; border: 2px solid #555; border-radius: 20px; }"
        "#iconBarAvatar:hover { border: 2px solid #07c160; }"
        // 图标按钮
        "#iconBtn { background: transparent; border: none; border-radius: 6px; }"
        "#iconBtn:hover { background-color: #3A3A3A; }"
        "#iconBtn:checked { background-color: #3A3A3A; }"
        // 右侧内容面板
        "#contentPanel { background-color: #EBEBEB; }"
        "#contentTopBar { background-color: #EBEBEB; border-bottom: 1px solid #DCDCDC; }"
        // 搜索框
        "#searchEdit { border: none; background: #DBDBDB; border-radius: 4px; "
        "  font-size: 13px; padding: 2px 10px; color: #333; }"
        "#searchEdit:focus { background: #D5D5D5; }"
        // "+" 按钮
        "#plusBtn { background: transparent; border: none; border-radius: 4px; }"
        "#plusBtn:hover { background: #DBDBDB; }"
    );
}

// ============================================================
// Public API
// ============================================================

void LeftSidebar::setSelfProfile(const QString &nickname, const QByteArray &avatarData)
{
    m_selfNickname = nickname;
    m_selfAvatarData = avatarData;

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
    // 外部 MainWindow 负责实际加载逻辑
}

QString LeftSidebar::selectedContact() const
{
    return m_contactList->selectedContact();
}

void LeftSidebar::clearSelection()
{
    m_contactList->clearSelection();
}

void LeftSidebar::setFriendRequestCount(int count)
{
    m_contactList->setFriendRequestCount(count);
}

// ============================================================
// Slots
// ============================================================

void LeftSidebar::onIconClicked(int id)
{
    m_currentTab = id;
    m_stack->setCurrentIndex(id);
    m_searchEdit->clear();

    // 更新搜索框 placeholder
    if (id == 0)
        m_searchEdit->setPlaceholderText("搜索");
    else
        m_searchEdit->setPlaceholderText("搜索联系人...");

    // 更新图标高亮色
    QColor activeColor("#07c160");
    QColor inactiveColor("#CCCCCC");
    m_chatIconBtn->setIcon(QIcon(makeIconPixmap(28,
        id == 0 ? activeColor : inactiveColor, "chat")));
    m_contactIconBtn->setIcon(QIcon(makeIconPixmap(28,
        id == 1 ? activeColor : inactiveColor, "contacts")));
}

void LeftSidebar::onSearchChanged(const QString &text)
{
    if (m_stack->currentIndex() == 0)
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

void LeftSidebar::onPlusMenuClicked()
{
    m_plusMenu->exec(m_plusBtn->mapToGlobal(QPoint(0, m_plusBtn->height())));
}
