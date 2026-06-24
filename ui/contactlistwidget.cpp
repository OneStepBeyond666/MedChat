#include "contactlistwidget.h"
#include "avatarcropper.h"
#include "client/chatclient.h"
#include "common/protocol.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QTextCodec>
#include <algorithm>

// ============================================================
// 汉字拼音首字母映射表
// 基于 GBK 编码排序，每个条目 { gbk编码, 该处开始的声母 }
// 使用二分查找确定任意汉字的声母分组
// ============================================================
struct PinyinBound { unsigned short gbk; char initial; };

static const PinyinBound s_pinyinTable[] = {
    // A
    {0xA1A3,'A'},{0xA3E2,'A'},
    // B
    {0xB0A1,'B'},{0xB0C4,'B'},{0xB2B6,'B'},{0xB3BB,'B'},{0xB4E8,'B'},
    // C
    {0xB5A6,'C'},{0xB5E7,'C'},{0xB6A1,'C'},{0xB6C7,'C'},{0xB7A1,'C'},
    // D
    {0xB8A1,'D'},{0xB9A1,'D'},{0xBADE,'D'},{0xBBBF,'D'},
    // E
    {0xBDA1,'E'},
    // F
    {0xBFA1,'F'},
    // G
    {0xC0A1,'G'},{0xC1A1,'G'},{0xC3A1,'G'},
    // H
    {0xC5A1,'H'},{0xC6A1,'H'},{0xC7A1,'H'},{0xC8A1,'H'},
    // J
    {0xC9A1,'J'},{0xCAB6,'J'},{0xCCE2,'J'},
    // K
    {0xCDFA,'K'},
    // L
    {0xCEA1,'L'},{0xD0A1,'L'},
    // M
    {0xD1A1,'M'},{0xD3A1,'M'},
    // N
    {0xD4A1,'N'},
    // O (no common Chinese chars, skip)
    // P
    {0xD5A1,'P'},{0xD6A1,'P'},
    // Q
    {0xD7A1,'Q'},{0xD8A1,'Q'},
    // R
    {0xD9A1,'R'},
    // S
    {0xDAA1,'S'},{0xDBA1,'S'},{0xDCA1,'S'},{0xDDA1,'S'},{0xDEA1,'S'},
    // T
    {0xDFA1,'T'},
    // W
    {0xE0A1,'W'},{0xE1A1,'W'},
    // X
    {0xE2A1,'X'},{0xE3A1,'X'},{0xE4A1,'X'},
    // Y
    {0xE5A1,'Y'},{0xE6A1,'Y'},{0xE7A1,'Y'},{0xE8A1,'Y'},
    // Z
    {0xE9A1,'Z'},{0xEAA1,'Z'},{0xEBA1,'Z'},{0xECA1,'Z'},
    {0xEDA1,'Z'},{0xEEA1,'Z'},{0xEFA1,'Z'},{0xF0A1,'Z'},
    {0xF1A1,'Z'},{0xF2A1,'Z'},
    // sentinel
    {0xFFFF,'#'}
};

static char pinyinInitial(QChar ch)
{
    ushort u = ch.unicode();

    // ASCII 字母直接返回大写
    if (u < 128) {
        QChar upper = QChar(u).toUpper();
        if (upper.unicode() >= 'A' && upper.unicode() <= 'Z')
            return upper.toLatin1();
        return '#';
    }

    // 尝试 GBK 编码
    QTextCodec *gbk = QTextCodec::codecForName("GBK");
    if (!gbk) return '#';

    QByteArray encoded = gbk->fromUnicode(QString(ch));
    if (encoded.size() < 2) return '#';

    unsigned short code = ((unsigned char)encoded[0] << 8) | (unsigned char)encoded[1];

    // 非汉字范围
    if (code < 0xB0A1 || code > 0xF7FE) return '#';

    // 二分查找：找到 <= code 的最大条目
    char initial = '#';
    int lo = 0, hi = (int)(sizeof(s_pinyinTable)/sizeof(s_pinyinTable[0])) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (s_pinyinTable[mid].gbk <= code) {
            initial = s_pinyinTable[mid].initial;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return initial;
}

// ============================================================
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
        "#contactList { border: none; background: #EBEBEB; }"
        "#contactList::item { padding: 6px 12px; border-bottom: 1px solid #E0E0E0; }"
        "#contactList::item:selected { background-color: #C8E6C9; }"
        "#contactList::item:hover { background-color: #DCDCDC; }"
    );
}

QChar ContactListWidget::groupLetter(const QString &str)
{
    if (str.isEmpty()) return '#';
    QChar first = str.at(0);
    char initial = pinyinInitial(first);
    if (initial >= 'A' && initial <= 'Z')
        return QChar(initial);
    return '#';
}

void ContactListWidget::updateContacts(const QMap<QString, ContactInfo> &contacts)
{
    m_contacts = contacts;
    rebuildList();
}

void ContactListWidget::rebuildList()
{
    m_listWidget->clear();

    // ========== "新的朋友" 入口（置顶） ==========
    if (m_filterText.isEmpty()) {
        QWidget *frWidget = new QWidget;
        QHBoxLayout *frLayout = new QHBoxLayout(frWidget);
        frLayout->setContentsMargins(4, 4, 8, 4);
        frLayout->setSpacing(8);

        // 图标
        QLabel *iconLabel = new QLabel;
        iconLabel->setFixedSize(40, 40);
        iconLabel->setScaledContents(true);
        iconLabel->setPixmap(AvatarCropper::defaultAvatar(
            QString::fromUtf8("\xe6\x96\xb0\xe7\x9a\x84\xe6\x9c\x8b\xe5\x8f\x8b"), 40));
        frLayout->addWidget(iconLabel);

        // 名称
        QLabel *frNameLabel = new QLabel(QString::fromUtf8(
            "\xe6\x96\xb0\xe7\x9a\x84\xe6\x9c\x8b\xe5\x8f\x8b"));  // "新的朋友"
        frNameLabel->setStyleSheet("font-size: 14px; color: #333;");
        frLayout->addWidget(frNameLabel, 1);

        // 红点徽标
        if (m_friendRequestCount > 0) {
            QLabel *badge = new QLabel(QString::number(m_friendRequestCount));
            badge->setFixedSize(20, 20);
            badge->setAlignment(Qt::AlignCenter);
            badge->setStyleSheet(
                "background: #FA5151; color: white; border-radius: 10px; "
                "font-size: 11px; font-weight: bold;");
            frLayout->addWidget(badge);
        }

        QListWidgetItem *frItem = new QListWidgetItem;
        frItem->setSizeHint(QSize(220, 48));
        frItem->setData(Qt::UserRole, "__friend_requests__");
        m_listWidget->addItem(frItem);
        m_listWidget->setItemWidget(frItem, frWidget);
    }

    // ========== 文件传输助手（始终置顶） ==========
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
        helperItem->setSizeHint(QSize(220, 48));
        helperItem->setData(Qt::UserRole, helperName);
        m_listWidget->addItem(helperItem);
        m_listWidget->setItemWidget(helperItem, helperWidget);
    }

    // ========== 按 ABC 分组 ==========
    // 收集所有联系人到分组 map
    QMap<QChar, QVector<QPair<QString, ContactInfo>>> groups;
    for (auto it = m_contacts.constBegin(); it != m_contacts.constEnd(); ++it) {
        const ContactInfo &ci = it.value();
        QString display = ci.nickname.isEmpty() ? ci.username : ci.nickname;

        // 搜索过滤
        if (!m_filterText.isEmpty()
            && !ci.username.toLower().contains(m_filterText)
            && !display.toLower().contains(m_filterText))
            continue;

        QChar gl = groupLetter(display);
        groups[gl].append(qMakePair(it.key(), ci));
    }

    // 按字母顺序 A-Z, # 遍历分组
    QList<QChar> sortedKeys = groups.keys();
    std::sort(sortedKeys.begin(), sortedKeys.end(), [](QChar a, QChar b) {
        if (a == '#') return false;
        if (b == '#') return true;
        return a < b;
    });

    for (QChar key : sortedKeys) {
        // 分组 Header
        QWidget *headerWidget = new QWidget;
        QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
        headerLayout->setContentsMargins(12, 2, 8, 2);
        headerLayout->setSpacing(0);
        QLabel *headerLabel = new QLabel(QString(key));
        headerLabel->setStyleSheet("font-size: 11px; color: #999; font-weight: bold;");
        headerLayout->addWidget(headerLabel);
        headerLayout->addStretch();

        QListWidgetItem *headerItem = new QListWidgetItem;
        headerItem->setSizeHint(QSize(220, 24));
        headerItem->setFlags(Qt::NoItemFlags);
        m_listWidget->addItem(headerItem);
        m_listWidget->setItemWidget(headerItem, headerWidget);

        // 组内联系人（按昵称排序）
        auto &members = groups[key];
        std::sort(members.begin(), members.end(),
            [](const QPair<QString, ContactInfo> &a, const QPair<QString, ContactInfo> &b) {
                QString na = a.second.nickname.isEmpty() ? a.first : a.second.nickname;
                QString nb = b.second.nickname.isEmpty() ? b.first : b.second.nickname;
                return na.toLower() < nb.toLower();
            });

        for (const auto &pair : members) {
            const QString &username = pair.first;
            const ContactInfo &ci = pair.second;
            QString display = ci.nickname.isEmpty() ? ci.username : ci.nickname;

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

            // 昵称
            QLabel *nameLabel = new QLabel(display);
            nameLabel->setStyleSheet("font-size: 14px; color: #333;");
            if (ci.online) {
                QFont font = nameLabel->font();
                font.setBold(true);
                nameLabel->setFont(font);
            }
            hLayout->addWidget(nameLabel, 1);

            // 在线状态指示点
            QLabel *dotLabel = new QLabel;
            dotLabel->setFixedSize(10, 10);
            if (ci.online) {
                dotLabel->setStyleSheet("background-color: #1AAD19; border-radius: 5px;");
            } else {
                dotLabel->setStyleSheet("background-color: #A0A0A0; border-radius: 5px;");
            }
            hLayout->addWidget(dotLabel);

            QListWidgetItem *item = new QListWidgetItem;
            item->setSizeHint(QSize(220, 52));
            item->setData(Qt::UserRole, username);
            m_listWidget->addItem(item);
            m_listWidget->setItemWidget(item, itemWidget);
        }
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
    rebuildList();
}

void ContactListWidget::setFriendRequestCount(int count)
{
    m_friendRequestCount = count;
    rebuildList();
}

void ContactListWidget::onItemClicked(QListWidgetItem *item)
{
    QString username = item->data(Qt::UserRole).toString();
    if (username == "__friend_requests__") {
        emit friendRequestEntryClicked();
        return;
    }
    emit contactSelected(username);
}
