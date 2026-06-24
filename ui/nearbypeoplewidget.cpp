#include "nearbypeoplewidget.h"
#include "avatarcropper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>

NearbyPeopleWidget::NearbyPeopleWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    applyStyles();
}

void NearbyPeopleWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 标题栏
    QWidget *header = new QWidget;
    header->setObjectName("npHeader");
    header->setFixedHeight(44);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 16, 0);
    QLabel *titleLabel = new QLabel(QString::fromUtf8(
        "\xe9\x99\x84\xe8\xbf\x91\xe7\x9a\x84\xe4\xba\xba"));  // "附近的人"
    titleLabel->setObjectName("npTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    layout->addWidget(header);

    // 列表
    m_listWidget = new QListWidget;
    m_listWidget->setObjectName("npList");
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_listWidget);
}

void NearbyPeopleWidget::applyStyles()
{
    setStyleSheet(
        "#npHeader { background: #FAFAFA; border-bottom: 1px solid #E0E0E0; }"
        "#npTitle { font-size: 16px; font-weight: bold; color: #333; }"
        "#npList { border: none; background: #EBEBEB; }"
        "#npList::item { border-bottom: 1px solid #E0E0E0; }"
    );
}

void NearbyPeopleWidget::setStrangers(const QMap<QString, ContactInfo> &strangers)
{
    m_listWidget->clear();
    m_pendingRequests.clear();

    if (strangers.isEmpty()) {
        QListWidgetItem *emptyItem = new QListWidgetItem(
            QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe9\x99\x84\xe8\xbf\x91\xe7\x9a\x84\xe4\xba\xba"));  // "暂无附近的人"
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setTextAlignment(Qt::AlignCenter);
        emptyItem->setSizeHint(QSize(280, 60));
        emptyItem->setForeground(QColor("#999"));
        m_listWidget->addItem(emptyItem);
        return;
    }

    for (auto it = strangers.constBegin(); it != strangers.constEnd(); ++it) {
        const QString &username = it.key();
        const ContactInfo &ci = it.value();
        QString display = ci.nickname.isEmpty() ? ci.username : ci.nickname;

        QWidget *itemWidget = new QWidget;
        QHBoxLayout *hLayout = new QHBoxLayout(itemWidget);
        hLayout->setContentsMargins(12, 8, 12, 8);
        hLayout->setSpacing(10);

        // 头像 (40x40)
        QLabel *avatarLabel = new QLabel;
        avatarLabel->setFixedSize(40, 40);
        avatarLabel->setScaledContents(true);
        if (!ci.avatarData.isEmpty())
            avatarLabel->setPixmap(AvatarCropper::roundAvatar(ci.avatarData, 40));
        else
            avatarLabel->setPixmap(AvatarCropper::defaultAvatar(display, 40));
        hLayout->addWidget(avatarLabel);

        // 昵称 + 角色
        QVBoxLayout *infoLayout = new QVBoxLayout;
        infoLayout->setSpacing(2);
        QLabel *nameLabel = new QLabel(display);
        nameLabel->setStyleSheet("font-size: 14px; color: #333; font-weight: bold;");
        infoLayout->addWidget(nameLabel);

        QString roleText = (ci.role == "doctor")
            ? QString::fromUtf8("\xe5\x8c\xbb\xe7\x94\x9f")    // "医生"
            : QString::fromUtf8("\xe6\x82\xa3\xe8\x80\x85");   // "患者"
        QLabel *roleLabel = new QLabel(roleText);
        roleLabel->setStyleSheet("font-size: 12px; color: #999;");
        infoLayout->addWidget(roleLabel);
        hLayout->addLayout(infoLayout, 1);

        // "添加好友" 按钮
        QPushButton *addBtn = new QPushButton(QString::fromUtf8(
            "\xe6\xb7\xbb\xe5\x8a\xa0\xe5\xa5\xbd\xe5\x8f\x8b"));  // "添加好友"
        addBtn->setFixedSize(76, 28);
        addBtn->setCursor(Qt::PointingHandCursor);
        addBtn->setObjectName("addFriendBtn");
        addBtn->setStyleSheet(
            "QPushButton#addFriendBtn { background: #07C160; color: white; "
            "border: none; border-radius: 4px; font-size: 12px; }"
            "QPushButton#addFriendBtn:hover { opacity: 0.85; }"
            "QPushButton#addFriendBtn:disabled { background: #C0C0C0; }"
        );
        hLayout->addWidget(addBtn);

        QListWidgetItem *item = new QListWidgetItem;
        item->setSizeHint(QSize(280, 64));
        item->setData(Qt::UserRole, username);
        m_listWidget->addItem(item);
        m_listWidget->setItemWidget(item, itemWidget);

        // 连接按钮信号
        connect(addBtn, &QPushButton::clicked, this, [this, username, addBtn]() {
            addBtn->setText(QString::fromUtf8(
                "\xe5\xb7\xb2\xe5\x8f\x91\xe9\x80\x81"));  // "已发送"
            addBtn->setEnabled(false);
            m_pendingRequests.insert(username);
            emit addFriendRequested(username);
        });
    }
}

void NearbyPeopleWidget::markRequestSent(const QString &username)
{
    m_pendingRequests.insert(username);
    // 遍历列表找到对应项，更新按钮状态
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toString() == username) {
            QWidget *w = m_listWidget->itemWidget(item);
            if (w) {
                QPushButton *btn = w->findChild<QPushButton*>("addFriendBtn");
                if (btn) {
                    btn->setText(QString::fromUtf8(
                        "\xe5\xb7\xb2\xe5\x8f\x91\xe9\x80\x81"));  // "已发送"
                    btn->setEnabled(false);
                }
            }
            break;
        }
    }
}

void NearbyPeopleWidget::clear()
{
    m_listWidget->clear();
    m_pendingRequests.clear();
}
