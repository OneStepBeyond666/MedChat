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
    {0xB0A1,'A'},  // 啊
    {0xB0C5,'B'},  // 芭
    {0xB2C1,'C'},  // 擦
    {0xB4EE,'D'},  // 搭
    {0xB6EA,'E'},  // 蛾
    {0xB7A2,'F'},  // 发
    {0xB8C1,'G'},  // 噶
    {0xB9FE,'H'},  // 哈
    {0xBBF7,'J'},  // 击
    {0xBFA6,'K'},  // 喀
    {0xC0AC,'L'},  // 垃
    {0xC2E8,'M'},  // 妈
    {0xC4C3,'N'},  // 拿
    {0xC5B6,'O'},  // 哦
    {0xC5BE,'P'},  // 啪
    {0xC6DA,'Q'},  // 期
    {0xC8BB,'R'},  // 然
    {0xC8F6,'S'},  // 撒
    {0xCBFA,'T'},  // 塌
    {0xCDDA,'W'},  // 挖
    {0xCEF4,'X'},  // 昔
    {0xD1B9,'Y'},  // 压
    {0xD4D1,'Z'},  // 匝
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
    m_listWidget->setFocusPolicy(Qt::NoFocus);
    m_listWidget->setAttribute(Qt::WA_MacShowFocusRect, false);
    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::itemClicked, this, &ContactListWidget::onItemClicked);
}

void ContactListWidget::applyStyles()
{
    setStyleSheet(
        "#contactList { border: none; background: #EBEBEB; outline: none; }"
        "#contactList::item { padding: 6px 12px; border-bottom: 1px solid #E0E0E0; outline: none; }"
        "#contactList::item:selected { background-color: #C8E6C9; outline: none; }"
        "#contactList::item:hover { background-color: #DCDCDC; outline: none; }"
        "#contactList::item:focus { outline: none; border: none; }"
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

        // 图标（缩小为32x32，适应较小的item高度）
        QLabel *iconLabel = new QLabel;
        iconLabel->setFixedSize(32, 32);
        iconLabel->setScaledContents(false);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setPixmap(AvatarCropper::defaultAvatar(
            QString::fromUtf8("\xe6\x96\xb0\xe7\x9a\x84\xe6\x9c\x8b\xe5\x8f\x8b"), 32));
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

    // ========== "附近的人" 入口 ==========
    if (m_filterText.isEmpty()) {
        QWidget *npWidget = new QWidget;
        QHBoxLayout *npLayout = new QHBoxLayout(npWidget);
        npLayout->setContentsMargins(4, 4, 8, 4);
        npLayout->setSpacing(8);

        // 图标（缩小为32x32，适应较小的item高度）
        QLabel *npIcon = new QLabel;
        npIcon->setFixedSize(32, 32);
        npIcon->setScaledContents(false);
        npIcon->setAlignment(Qt::AlignCenter);
        npIcon->setPixmap(AvatarCropper::defaultAvatar(
            QString::fromUtf8("\xe9\x99\x84\xe8\xbf\x91"), 32));
        npLayout->addWidget(npIcon);

        // 名称
        QLabel *npNameLabel = new QLabel(QString::fromUtf8(
            "\xe9\x99\x84\xe8\xbf\x91\xe7\x9a\x84\xe4\xba\xba"));  // "附近的人"
        npNameLabel->setStyleSheet("font-size: 14px; color: #333;");
        npLayout->addWidget(npNameLabel, 1);

        // 人数徽标
        if (m_nearbyPeopleCount > 0) {
            QLabel *npBadge = new QLabel(QString::number(m_nearbyPeopleCount));
            npBadge->setFixedSize(20, 20);
            npBadge->setAlignment(Qt::AlignCenter);
            npBadge->setStyleSheet(
                "background: #576B95; color: white; border-radius: 10px; "
                "font-size: 11px; font-weight: bold;");
            npLayout->addWidget(npBadge);
        }

        QListWidgetItem *npItem = new QListWidgetItem;
        npItem->setSizeHint(QSize(220, 48));
        npItem->setData(Qt::UserRole, "__nearby_people__");
        m_listWidget->addItem(npItem);
        m_listWidget->setItemWidget(npItem, npWidget);
    }

    // ========== 文件传输助手（始终置顶） ==========
    QString helperName = QString::fromUtf8(MsgType::FileHelper);
    if (m_filterText.isEmpty() || helperName.contains(m_filterText)) {
        QWidget *helperWidget = new QWidget;
        QHBoxLayout *hLayout = new QHBoxLayout(helperWidget);
        hLayout->setContentsMargins(4, 4, 8, 4);
        hLayout->setSpacing(8);

        QLabel *avatarLabel = new QLabel;
        avatarLabel->setFixedSize(32, 32);
        avatarLabel->setScaledContents(false);
        avatarLabel->setAlignment(Qt::AlignCenter);
        avatarLabel->setPixmap(AvatarCropper::defaultAvatar(helperName, 32));
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
            avatarLabel->setScaledContents(false);
            avatarLabel->setAlignment(Qt::AlignCenter);
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

void ContactListWidget::setNearbyPeopleCount(int count)
{
    m_nearbyPeopleCount = count;
    rebuildList();
}

void ContactListWidget::onItemClicked(QListWidgetItem *item)
{
    QString username = item->data(Qt::UserRole).toString();
    if (username == "__friend_requests__") {
        emit friendRequestEntryClicked();
        return;
    }
    if (username == "__nearby_people__") {
        emit nearbyPeopleEntryClicked();
        return;
    }
    emit contactSelected(username);
}
