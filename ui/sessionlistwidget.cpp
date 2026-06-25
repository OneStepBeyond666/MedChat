#include "sessionlistwidget.h"
#include "avatarcropper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>

SessionListWidget::SessionListWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    applyStyles();
}

void SessionListWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_listWidget = new QListWidget;
    m_listWidget->setObjectName("sessionList");
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setFocusPolicy(Qt::NoFocus);
    m_listWidget->setAttribute(Qt::WA_MacShowFocusRect, false);
    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::itemClicked, this, &SessionListWidget::onItemClicked);
}

void SessionListWidget::applyStyles()
{
    setStyleSheet(
        "#sessionList { border: none; background: #EBEBEB; outline: none; }"
        "#sessionList::item { padding: 8px 12px; border-bottom: 1px solid #E0E0E0; outline: none; }"
        "#sessionList::item:selected { background-color: #C8E6C9; outline: none; }"
        "#sessionList::item:hover { background-color: #DCDCDC; outline: none; }"
        "#sessionList::item:focus { outline: none; border: none; }"
    );
}

QListWidgetItem *SessionListWidget::createSessionItem(const SessionInfo &s)
{
    // 创建自定义 widget 作为列表项
    QWidget *itemWidget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(itemWidget);
    layout->setContentsMargins(4, 0, 8, 0);  // 上下边距为0，让布局自动居中
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignVCenter);  // 整体垂直居中

    // 头像 (40x40 圆形)
    QLabel *avatarLabel = new QLabel;
    avatarLabel->setFixedSize(40, 40);
    avatarLabel->setObjectName("avatar");
    avatarLabel->setScaledContents(false);  // 防止pixmap被拉伸
    avatarLabel->setAlignment(Qt::AlignCenter);
    if (!s.avatarData.isEmpty()) {
        avatarLabel->setPixmap(AvatarCropper::roundAvatar(s.avatarData, 40));
    } else {
        QString nameForAvatar = s.nickname.isEmpty() ? s.username : s.nickname;
        avatarLabel->setPixmap(AvatarCropper::defaultAvatar(nameForAvatar, 40));
    }
    layout->addWidget(avatarLabel, 0, Qt::AlignVCenter);  // 头像垂直居中

    // 中间区域：昵称 + 预览
    QWidget *midWidget = new QWidget;
    QVBoxLayout *midLayout = new QVBoxLayout(midWidget);
    midLayout->setContentsMargins(0, 0, 0, 0);
    midLayout->setSpacing(2);

    // 昵称行（昵称 + 时间）
    QWidget *topRow = new QWidget;
    QHBoxLayout *topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setAlignment(Qt::AlignVCenter);  // 确保垂直居中

    QLabel *nickLabel = new QLabel(s.nickname.isEmpty() ? s.username : s.nickname);
    nickLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #333;");
    nickLabel->setObjectName("nickLabel");
    nickLabel->setAlignment(Qt::AlignVCenter);  // 文字垂直居中
    topLayout->addWidget(nickLabel);

    topLayout->addStretch();

    // 时间
    QString timeStr;
    if (s.lastTime > 0) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(s.lastTime);
        QDateTime now = QDateTime::currentDateTime();
        QDate today = now.date();
        QDate msgDate = dt.date();
        if (msgDate == today)
            timeStr = dt.toString("HH:mm");
        else if (msgDate == today.addDays(-1))
            timeStr = QStringLiteral("昨天");
        else if (msgDate.year() == today.year())
            timeStr = dt.toString("MM/dd");
        else
            timeStr = dt.toString("yyyy/MM/dd");
    }
    QLabel *timeLabel = new QLabel(timeStr);
    timeLabel->setStyleSheet("font-size: 11px; color: #999;");
    timeLabel->setAlignment(Qt::AlignVCenter);  // 文字垂直居中
    topLayout->addWidget(timeLabel);

    midLayout->addWidget(topRow);

    // 预览行（最多20字，超出显示...）
    QString preview = s.lastMsgPreview;
    if (preview.length() > 20)
        preview = preview.left(20) + QStringLiteral("...");
    QLabel *previewLabel = new QLabel(preview);
    previewLabel->setStyleSheet("font-size: 12px; color: #999;");
    previewLabel->setMaximumWidth(180);
    previewLabel->setWordWrap(false);
    previewLabel->setAlignment(Qt::AlignVCenter);  // 文字垂直居中
    midLayout->addWidget(previewLabel);

    layout->addWidget(midWidget, 1);

    // 未读红点（超过99显示99+）
    QLabel *badgeLabel = new QLabel;
    badgeLabel->setFixedSize(20, 20);
    badgeLabel->setAlignment(Qt::AlignCenter);
    if (s.unreadCount > 0) {
        QString badgeText = (s.unreadCount > 99) ? QStringLiteral("99+") : QString::number(s.unreadCount);
        badgeLabel->setText(badgeText);
        badgeLabel->setStyleSheet(
            "background-color: #F55545; color: white; border-radius: 10px; "
            "font-size: 11px; font-weight: bold;"
        );
    } else {
        badgeLabel->hide();
    }
    layout->addWidget(badgeLabel);

    // 创建 QListWidgetItem
    QListWidgetItem *item = new QListWidgetItem;
    item->setSizeHint(QSize(220, 56));
    item->setData(Qt::UserRole, s.contactUid);

    // 存储昵称用于过滤
    item->setData(Qt::UserRole + 1, s.nickname.isEmpty() ? s.username : s.nickname);

    m_listWidget->addItem(item);
    m_listWidget->setItemWidget(item, itemWidget);

    return item;
}

void SessionListWidget::setSessions(const QVector<SessionInfo> &sessions)
{
    m_sessions = sessions;
    m_listWidget->clear();

    for (const SessionInfo &s : m_sessions) {
        // 过滤
        if (!m_filterText.isEmpty()) {
            QString nick = s.nickname.isEmpty() ? s.username : s.nickname;
            if (!nick.toLower().contains(m_filterText) &&
                !s.lastMsgPreview.toLower().contains(m_filterText))
                continue;
        }
        createSessionItem(s);
    }
}

void SessionListWidget::updateSession(const SessionInfo &session)
{
    // 查找已有 item 并更新
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].contactUid == session.contactUid) {
            m_sessions[i] = session;
            break;
        }
    }
    // 简单重建列表
    setSessions(m_sessions);
}

void SessionListWidget::filter(const QString &text)
{
    m_filterText = text.trimmed().toLower();
    setSessions(m_sessions);
}

void SessionListWidget::onItemClicked(QListWidgetItem *item)
{
    QString username = item->data(Qt::UserRole).toString();
    emit sessionClicked(username);
}
