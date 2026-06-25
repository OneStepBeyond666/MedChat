#include "forwarddialog.h"
#include "avatarcropper.h"
#include "client/chatclient.h"   // ContactInfo
#include "common/protocol.h"     // MsgType::FileHelper

#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>

ForwardDialog::ForwardDialog(const QMap<QString, ContactInfo> &contacts,
                             const QString &preview,
                             QWidget *parent)
    : QDialog(parent)
{
    m_contacts = contacts;

    // 收集联系人（排除自己不需要，因为 MainWindow 传入的 contacts 不含自己）
    for (auto it = contacts.constBegin(); it != contacts.constEnd(); ++it) {
        m_allUsernames.append(it.key());
        m_selectionMap[it.key()] = false;
    }
    // 添加"文件传输助手"伪联系人
    QString fileHelper = QString::fromUtf8(MsgType::FileHelper);
    if (!m_selectionMap.contains(fileHelper)) {
        m_allUsernames.prepend(fileHelper);
        m_selectionMap[fileHelper] = false;
    }

    setupUI(preview);
    applyStyles();
}

QStringList ForwardDialog::selectedUsernames() const
{
    QStringList result;
    for (auto it = m_selectionMap.constBegin(); it != m_selectionMap.constEnd(); ++it) {
        if (it.value()) result.append(it.key());
    }
    return result;
}

void ForwardDialog::setupUI(const QString &preview)
{
    setWindowTitle(QStringLiteral("转发"));
    setFixedSize(400, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 顶部标题栏 ──
    QWidget *header = new QWidget;
    header->setObjectName("header");
    header->setFixedHeight(50);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 16, 0);
    QLabel *titleLabel = new QLabel(QStringLiteral("转发"));
    titleLabel->setObjectName("title");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    mainLayout->addWidget(header);

    // ── 分隔线 ──
    QWidget *sep1 = new QWidget;
    sep1->setFixedHeight(1);
    sep1->setObjectName("separator");
    mainLayout->addWidget(sep1);

    // ── 搜索框 ──
    QWidget *searchArea = new QWidget;
    searchArea->setObjectName("searchArea");
    QHBoxLayout *searchLayout = new QHBoxLayout(searchArea);
    searchLayout->setContentsMargins(12, 8, 12, 8);
    m_searchEdit = new QLineEdit;
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索联系人"));
    m_searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(m_searchEdit);
    mainLayout->addWidget(searchArea);

    // ── 联系人列表 ──
    m_listWidget = new QListWidget;
    m_listWidget->setObjectName("contactList");
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    for (const QString &uname : m_allUsernames) {
        // 行容器
        QWidget *row = new QWidget;
        row->setObjectName("contactRow");
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 6, 12, 6);
        rowLayout->setSpacing(10);

        QCheckBox *checkBox = new QCheckBox;
        checkBox->setObjectName("rowCheck");

        // 头像
        QLabel *avatarLabel = new QLabel;
        avatarLabel->setFixedSize(40, 40);
        avatarLabel->setObjectName("avatar");

        bool isFileHelper = (uname == QString::fromUtf8(MsgType::FileHelper));
        if (isFileHelper) {
            avatarLabel->setPixmap(AvatarCropper::defaultAvatar(QStringLiteral("文件"), 40));
        } else {
            const ContactInfo &ci = m_contacts.value(uname);
            if (!ci.avatarData.isEmpty()) {
                avatarLabel->setPixmap(AvatarCropper::roundAvatar(ci.avatarData, 40));
            } else {
                avatarLabel->setPixmap(AvatarCropper::defaultAvatar(
                    ci.nickname.isEmpty() ? uname : ci.nickname, 40));
            }
        }

        // 昵称
        QLabel *nameLabel = new QLabel;
        nameLabel->setObjectName("nameLabel");
        if (isFileHelper) {
            nameLabel->setText(QStringLiteral("文件传输助手"));
        } else {
            const ContactInfo &ci = m_contacts.value(uname);
            nameLabel->setText(ci.nickname.isEmpty() ? uname : ci.nickname);
        }

        rowLayout->addWidget(checkBox);
        rowLayout->addWidget(avatarLabel);
        rowLayout->addWidget(nameLabel, 1);

        QListWidgetItem *item = new QListWidgetItem;
        item->setSizeHint(row->sizeHint());
        item->setData(Qt::UserRole, uname);
        m_listWidget->addItem(item);
        m_listWidget->setItemWidget(item, row);

        // 预计算大小
        row->adjustSize();
        item->setSizeHint(QSize(row->width(), 52));
    }

    mainLayout->addWidget(m_listWidget, 1);

    // ── 分隔线 ──
    QWidget *sep2 = new QWidget;
    sep2->setFixedHeight(1);
    sep2->setObjectName("separator");
    mainLayout->addWidget(sep2);

    // ── 底部区域 ──
    QWidget *footer = new QWidget;
    footer->setObjectName("footer");
    footer->setFixedHeight(70);
    QVBoxLayout *footerLayout = new QVBoxLayout(footer);
    footerLayout->setContentsMargins(16, 8, 16, 8);
    footerLayout->setSpacing(4);

    // 消息预览
    m_previewLabel = new QLabel;
    m_previewLabel->setObjectName("previewLabel");
    m_previewLabel->setText(QStringLiteral("转发内容: %1").arg(preview));
    m_previewLabel->setWordWrap(false);
    m_previewLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    footerLayout->addWidget(m_previewLabel);

    // 底部按钮行
    QHBoxLayout *btnRow = new QHBoxLayout;
    m_countLabel = new QLabel(QStringLiteral("已选 0 人"));
    m_countLabel->setObjectName("countLabel");
    btnRow->addWidget(m_countLabel);
    btnRow->addStretch();

    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"));
    cancelBtn->setObjectName("cancelBtn");
    cancelBtn->setFixedSize(70, 32);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_sendBtn = new QPushButton(QStringLiteral("发送"));
    m_sendBtn->setObjectName("sendBtn");
    m_sendBtn->setFixedSize(70, 32);
    m_sendBtn->setEnabled(false);
    connect(m_sendBtn, &QPushButton::clicked, this, &ForwardDialog::onSendClicked);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(m_sendBtn);
    footerLayout->addLayout(btnRow);

    mainLayout->addWidget(footer);

    // ── 信号连接 ──
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ForwardDialog::onSearchTextChanged);
    connect(m_listWidget, &QListWidget::itemClicked, this, &ForwardDialog::onItemClicked);
}

void ForwardDialog::applyStyles()
{
    setStyleSheet(
        "ForwardDialog { background: #F5F5F5; }"
        "#header { background: #FAFAFA; }"
        "#title { font-size: 16px; font-weight: bold; color: #333; }"
        "#separator { background: #E0E0E0; }"
        "#searchArea { background: #F5F5F5; }"
        "#searchEdit {"
        "  border: 1px solid #E0E0E0; border-radius: 4px; padding: 6px 10px;"
        "  font-size: 14px; background: white;"
        "}"
        "#searchEdit:focus { border-color: #07C160; }"
        "#contactList {"
        "  border: none; background: white;"
        "  outline: none;"
        "}"
        "#contactList::item { border-bottom: 1px solid #F0F0F0; }"
        "#contactList::item:hover { background: #F5F5F5; }"
        "#contactRow { background: transparent; }"
        "#avatar { border-radius: 4px; }"
        "#nameLabel { font-size: 14px; color: #333; }"
        "#footer { background: #FAFAFA; }"
        "#previewLabel {"
        "  font-size: 12px; color: #999;"
        "  padding: 2px 0;"
        "}"
        "#countLabel { font-size: 12px; color: #999; }"
        "#cancelBtn {"
        "  background: white; border: 1px solid #E0E0E0; border-radius: 4px;"
        "  font-size: 13px; color: #666;"
        "}"
        "#cancelBtn:hover { background: #F5F5F5; }"
        "#sendBtn {"
        "  background: #07C160; border: none; border-radius: 4px;"
        "  font-size: 13px; color: white;"
        "}"
        "#sendBtn:hover { background: #06AD56; }"
        "#sendBtn:pressed { background: #059A4C; }"
        "#sendBtn:disabled { background: #C0C0C0; }"
    );
}

void ForwardDialog::onSearchTextChanged(const QString &text)
{
    QString keyword = text.trimmed().toLower();
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString uname = item->data(Qt::UserRole).toString();
        bool isFileHelper = (uname == QString::fromUtf8(MsgType::FileHelper));

        bool match = false;
        if (keyword.isEmpty()) {
            match = true;
        } else if (isFileHelper) {
            match = QStringLiteral("文件传输助手").toLower().contains(keyword)
                    || QStringLiteral("filehelper").contains(keyword);
        } else {
            // 匹配用户名或昵称
            match = uname.toLower().contains(keyword);
            if (!match) {
                const ContactInfo &ci = m_contacts.value(uname);
                match = ci.nickname.toLower().contains(keyword);
            }
        }
        item->setHidden(!match);
    }
}

void ForwardDialog::onItemClicked(QListWidgetItem *item)
{
    QString uname = item->data(Qt::UserRole).toString();
    QWidget *row = m_listWidget->itemWidget(item);
    QCheckBox *cb = row->findChild<QCheckBox*>();
    if (!cb) return;

    bool newState = !m_selectionMap.value(uname, false);
    m_selectionMap[uname] = newState;
    cb->setChecked(newState);

    updateSelectionCount();
}

void ForwardDialog::onSendClicked()
{
    if (selectedUsernames().isEmpty()) return;
    accept();
}

void ForwardDialog::updateSelectionCount()
{
    int count = 0;
    for (auto it = m_selectionMap.constBegin(); it != m_selectionMap.constEnd(); ++it) {
        if (it.value()) count++;
    }
    m_countLabel->setText(QStringLiteral("已选 %1 人").arg(count));
    m_sendBtn->setEnabled(count > 0);
}
