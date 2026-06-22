#include "contactlistwidget.h"
#include "client/chatclient.h"
#include "common/protocol.h"
#include <QVBoxLayout>
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

    QLabel *header = new QLabel("  联系人");
    header->setFixedHeight(50);
    header->setObjectName("contactHeader");
    layout->addWidget(header);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("  搜索联系人...");
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setFixedHeight(36);
    m_searchEdit->setContentsMargins(8, 0, 8, 0);
    layout->addWidget(m_searchEdit);

    m_listWidget = new QListWidget;
    m_listWidget->setObjectName("contactList");
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_listWidget);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &ContactListWidget::onSearchTextChanged);
    connect(m_listWidget, &QListWidget::itemClicked, this, &ContactListWidget::onItemClicked);
}

void ContactListWidget::applyStyles()
{
    setStyleSheet(
        "#contactHeader { background-color: #f0f0f0; font-size: 16px; font-weight: bold; "
        "  color: #333; padding-left: 10px; border-bottom: 1px solid #ddd; }"
        "#searchEdit { border: none; border-bottom: 1px solid #e0e0e0; background: #f7f7f7; "
        "  border-radius: 4px; margin: 6px 8px; font-size: 13px; padding: 4px 8px; }"
        "#contactList { border: none; background: #f0f0f0; }"
        "#contactList::item { padding: 10px 12px; border-bottom: 1px solid #e8e8e8; }"
        "#contactList::item:selected { background-color: #c8e6c9; }"
        "#contactList::item:hover { background-color: #e0e0e0; }"
    );
}

void ContactListWidget::updateContacts(const QMap<QString, ContactInfo> &contacts)
{
    m_contacts = contacts;
    QString filter = m_searchEdit->text().trimmed().toLower();
    m_listWidget->clear();

    // 文件传输助手（始终置顶）
    QString helperName = QString::fromUtf8(MsgType::FileHelper);
    if (filter.isEmpty() || helperName.contains(filter)) {
        QListWidgetItem *helperItem = new QListWidgetItem(
            QStringLiteral("\u25B6") + " " + helperName);
        helperItem->setData(Qt::UserRole, helperName);
        QFont hf = helperItem->font();
        hf.setBold(true);
        helperItem->setFont(hf);
        helperItem->setForeground(QColor("#07c160"));
        m_listWidget->addItem(helperItem);
    }

    // 先显示在线的医生，再显示在线的病人，然后离线的
    auto addContact = [this, &filter](const QString &username, const ContactInfo &ci) {
        if (!filter.isEmpty() && !username.toLower().contains(filter))
            return;
        QString roleLabel = (ci.role == "doctor") ? "[医生]" : "[患者]";
        QString onlineDot = ci.online ? QStringLiteral(" \u25CF") : QStringLiteral(" \u25CB");
        QListWidgetItem *item = new QListWidgetItem(roleLabel + " " + username + onlineDot);
        item->setData(Qt::UserRole, username);

        if (ci.online) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        } else {
            item->setForeground(QColor("#999"));
        }
        m_listWidget->addItem(item);
    };

    // 在线医生
    for (auto it = m_contacts.constBegin(); it != m_contacts.constEnd(); ++it) {
        if (it->online && it->role == "doctor")
            addContact(it.key(), *it);
    }
    // 在线患者
    for (auto it = m_contacts.constBegin(); it != m_contacts.constEnd(); ++it) {
        if (it->online && it->role == "patient")
            addContact(it.key(), *it);
    }
    // 离线
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

void ContactListWidget::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text)
    updateContacts(m_contacts);
}

void ContactListWidget::onItemClicked(QListWidgetItem *item)
{
    QString username = item->data(Qt::UserRole).toString();
    emit contactSelected(username);
}
