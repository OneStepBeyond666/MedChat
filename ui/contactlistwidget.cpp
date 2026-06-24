#include "contactlistwidget.h"
#include "avatarcropper.h"
#include "client/chatclient.h"
#include "common/protocol.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>

ContactListWidget::ContactListWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    applyStyles();
}

void ContactListWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_listWidget = new QListWidget;
    m_listWidget->setObjectName("contactList");
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::itemClicked, this, &ContactListWidget::onItemClicked);
}

void ContactListWidget::applyStyles()
{
    setStyleSheet(
        "#contactList { border: none; background: #f0f0f0; }"
        "#contactList::item { padding: 8px 12px; border-bottom: 1px solid #e8e8e8; }"
        "#contactList::item:selected { background-color: #c8e6c9; }"
        "#contactList::item:hover { background-color: #e0e0e0; }"
    );
}

void ContactListWidget::updateContacts(const QMap<QString, ContactInfo> &contacts)
{
    m_contacts = contacts;
    m_listWidget->clear();

    // 文件传输助手（始终置顶）
    QString helperName = QString::fromUtf8(MsgType::FileHelper);
    if (m_filterText.isEmpty() || helperName.contains(m_filterText)) {
        QWidget *helperWidget = new QWidget;
        QHBoxLayout *hLayout = new QHBoxLayout(helperWidget);
        hLayout->setContentsMargins(4, 4, 8, 4);
        hLayout->setSpacing(8);

        QLabel *avatarLabel = new QLabel;
        avatarLabel->setFixedSize(40, 40);
        avatarLabel->setPixmap(AvatarCropper::defaultAvatar(helperName, 40));
        avatarLabel->setScaledContents(true);
        hLayout->addWidget(avatarLabel);

        QLabel *nameLabel = new QLabel(helperName);
        QFont nf = nameLabel->font();
        nf.setBold(true);
        nameLabel->setFont(nf);
        nameLabel->setStyleSheet("color: #07c160; font-size: 14px;");
        hLayout->addWidget(nameLabel);
        hLayout->addStretch();

        QListWidgetItem *helperItem = new QListWidgetItem;
        helperItem->setSizeHint(QSize(260, 48));
        helperItem->setData(Qt::UserRole, helperName);
        m_listWidget->addItem(helperItem);
        m_listWidget->setItemWidget(helperItem, helperWidget);
    }

    // 辅助函数：添加分组标题
    auto addGroupHeader = [this](const QString &title) {
        QWidget *headerWidget = new QWidget;
        QHBoxLayout *hLayout = new QHBoxLayout(headerWidget);
        hLayout->setContentsMargins(12, 4, 8, 4);
        hLayout->setSpacing(0);

        QLabel *titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-size: 11px; color: #999; font-weight: bold;");
        hLayout->addWidget(titleLabel);
        hLayout->addStretch();

        QListWidgetItem *headerItem = new QListWidgetItem;
        headerItem->setSizeHint(QSize(260, 28));
        headerItem->setFlags(Qt::NoItemFlags);  // 不可选中
        m_listWidget->addItem(headerItem);
        m_listWidget->setItemWidget(headerItem, headerWidget);
    };

    // 辅助函数：添加联系人项（带头像）
    auto addContact = [this](const QString &username, const ContactInfo &ci) {
        QString display = ci.nickname.isEmpty() ? username : ci.nickname;
        if (!m_filterText.isEmpty()
            && !username.toLower().contains(m_filterText)
            && !display.toLower().contains(m_filterText))
            return;

        QWidget *itemWidget = new QWidget;
        QHBoxLayout *hLayout = new QHBoxLayout(itemWidget);
        hLayout->setContentsMargins(4, 4, 8, 4);
        hLayout->setSpacing(8);

        // 头像 (40x40 圆形)
        QLabel *avatarLabel = new QLabel;
        avatarLabel->setFixedSize(40, 40);
        avatarLabel->setScaledContents(true);
        if (!ci.avatarData.isEmpty())
            avatarLabel->setPixmap(AvatarCropper::roundAvatar(ci.avatarData, 40));
        else
            avatarLabel->setPixmap(AvatarCropper::defaultAvatar(display, 40));
        hLayout->addWidget(avatarLabel);

        // 中间：昵称 + 角色
        QWidget *midWidget = new QWidget;
        QVBoxLayout *midLayout = new QVBoxLayout(midWidget);
        midLayout->setContentsMargins(0, 0, 0, 0);
        midLayout->setSpacing(0);

        QLabel *nameLabel = new QLabel(display);
        nameLabel->setStyleSheet("font-size: 14px; color: #333;");
        if (ci.online) {
            QFont font = nameLabel->font();
            font.setBold(true);
            nameLabel->setFont(font);
        }
        midLayout->addWidget(nameLabel);

        QString roleText = (ci.role == "doctor") ? "医生" : "患者";
        QLabel *roleLabel = new QLabel(roleText);
        roleLabel->setStyleSheet("font-size: 11px; color: #999;");
        midLayout->addWidget(roleLabel);

        hLayout->addWidget(midWidget, 1);

        // 在线状态点
        QLabel *dotLabel = new QLabel(ci.online ? "\u25CF" : "\u25CB");
        dotLabel->setStyleSheet(
            ci.online ? "color: #1AAD19; font-size: 14px;" : "color: #A0A0A0; font-size: 14px;"
        );
        hLayout->addWidget(dotLabel);

        QListWidgetItem *item = new QListWidgetItem;
        item->setSizeHint(QSize(260, 52));
        item->setData(Qt::UserRole, username);

        m_listWidget->addItem(item);
        m_listWidget->setItemWidget(item, itemWidget);
    };

    // 在线医生
    bool hasOnlineDoctors = false;
    for (auto it = m_contacts.constBegin(); it != m_contacts.constEnd(); ++it) {
        if (it->online && it->role == "doctor") { hasOnlineDoctors = true; break; }
    }
    if (hasOnlineDoctors) addGroupHeader(QStringLiteral("在线医生"));
    for (auto it = m_contacts.constBegin(); it != m_contacts.constEnd(); ++it) {
        if (it->online && it->role == "doctor")
            addContact(it.key(), *it);
    }
    // 在线患者
    bool hasOnlinePatients = false;
    for (auto it = m_contacts.constBegin(); it != m_contacts.constEnd(); ++it) {
        if (it->online && it->role == "patient") { hasOnlinePatients = true; break; }
    }
    if (hasOnlinePatients) addGroupHeader(QStringLiteral("在线患者"));
    for (auto it = m_contacts.constBegin(); it != m_contacts.constEnd(); ++it) {
        if (it->online && it->role == "patient")
            addContact(it.key(), *it);
    }
    // 离线联系人
    bool hasOffline = false;
    for (auto it = m_contacts.constBegin(); it != m_contacts.constEnd(); ++it) {
        if (!it->online) { hasOffline = true; break; }
    }
    if (hasOffline) addGroupHeader(QStringLiteral("离线联系人"));
    for (auto it = m_contacts.constBegin(); it != m_contacts.constEnd(); ++it) {
        if (!it->online)
            addContact(it.key(), *it);
    }
}

QString ContactListWidget::selectedContact() const
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (item)
        return item->data(Qt::UserRole).toString();
    return QString();
}

void ContactListWidget::clearSelection()
{
    m_listWidget->clearSelection();
    m_listWidget->setCurrentItem(nullptr);
}

void ContactListWidget::filter(const QString &text)
{
    m_filterText = text.trimmed().toLower();
    updateContacts(m_contacts);
}

void ContactListWidget::onItemClicked(QListWidgetItem *item)
{
    QString username = item->data(Qt::UserRole).toString();
    emit contactSelected(username);
}
