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
    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::itemClicked, this, &SessionListWidget::onItemClicked);
}

void SessionListWidget::applyStyles()
{
    setStyleSheet(
        "#sessionList { border: none; background: #f0f0f0; }"
        "#sessionList::item { padding: 8px 12px; border-bottom: 1px solid #e8e8e8; }"
        "#sessionList::item:selected { background-color: #c8e6c9; }"
        "#sessionList::item:hover { background-color: #e0e0e0; }"
    );
}

QListWidgetItem *SessionListWidget::createSessionItem(const SessionInfo &s)
{
    // 创建自定义 widget 作为列表项
    QWidget *itemWidget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(itemWidget);
    layout->setContentsMargins(4, 4, 8, 4);
    layout->setSpacing(8);

    // 头像 (40x40 圆形)
    QLabel *avatarLabel = new QLabel;
    avatarLabel->setFixedSize(40, 40);
    avatarLabel->setObjectName("avatar");
    if (!s.avatarData.isEmpty()) {
        avatarLabel->setPixmap(AvatarCropper::roundAvatar(s.avatarData, 40));
    } else {
        QString nameForAvatar = s.nickname.isEmpty() ? s.username : s.nickname;
        avatarLabel->setPixmap(AvatarCropper::defaultAvatar(nameForAvatar, 40));
    }
    layout->addWidget(avatarLabel);

    // 中间区域：昵称 + 预览
    QWidget *midWidget = new QWidget;
    QVBoxLayout *midLayout = new QVBoxLayout(midWidget);
    midLayout->setContentsMargins(0, 0, 0, 0);
    midLayout->setSpacing(2);

    // 昵称行（昵称 + 时间）
    QWidget *topRow = new QWidget;
    QHBoxLayout *topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *nickLabel = new QLabel(s.nickname.isEmpty() ? s.username : s.nickname);
    nickLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #333;");
    nickLabel->setObjectName("nickLabel");
    topLayout->addWidget(nickLabel);

    topLayout->addStretch();

    // 时间
    QString timeStr;
    if (s.lastTime > 0) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(s.lastTime);
        QDateTime now = QDateTime::currentDateTime();
        if (dt.date() == now.date())
            timeStr = dt.toString("HH:mm");
        else if (dt.date().year() == now.date().year())
            timeStr = dt.toString("MM/dd");
        else
            timeStr = dt.toString("yyyy/MM/dd");
    }
    QLabel *timeLabel = new QLabel(timeStr);
    timeLabel->setStyleSheet("font-size: 11px; color: #999;");
    topLayout->addWidget(timeLabel);

    midLayout->addWidget(topRow);

    // 预览行
    QLabel *previewLabel = new QLabel(s.lastMsgPreview);
    previewLabel->setStyleSheet("font-size: 12px; color: #999;");
    previewLabel->setMaximumWidth(180);
    previewLabel->setWordWrap(false);
    // 截断长文本
    QFontMetrics fm(previewLabel->font());
    QString elided = fm.elidedText(s.lastMsgPreview, Qt::ElideRight, 180);
    previewLabel->setText(elided);
    midLayout->addWidget(previewLabel);

    layout->addWidget(midWidget, 1);

    // 未读红点
    QLabel *badgeLabel = new QLabel;
    badgeLabel->setFixedSize(20, 20);
    badgeLabel->setAlignment(Qt::AlignCenter);
    if (s.unreadCount > 0) {
        badgeLabel->setText(QString::number(s.unreadCount));
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
    item->setSizeHint(QSize(260, 56));
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
