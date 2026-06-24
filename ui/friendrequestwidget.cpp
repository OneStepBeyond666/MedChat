#include "friendrequestwidget.h"
#include "avatarcropper.h"
#include "client/localdb.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>

FriendRequestWidget::FriendRequestWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    applyStyles();
}

void FriendRequestWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 标题栏
    QWidget *header = new QWidget;
    header->setObjectName("frHeader");
    header->setFixedHeight(44);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 16, 0);
    QLabel *titleLabel = new QLabel("新的朋友");
    titleLabel->setObjectName("frTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    layout->addWidget(header);

    // 列表
    m_listWidget = new QListWidget;
    m_listWidget->setObjectName("frList");
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_listWidget);
}

void FriendRequestWidget::applyStyles()
{
    setStyleSheet(
        "#frHeader { background: #FAFAFA; border-bottom: 1px solid #E0E0E0; }"
        "#frTitle { font-size: 16px; font-weight: bold; color: #333; }"
        "#frList { border: none; background: #EBEBEB; }"
        "#frList::item { border-bottom: 1px solid #E0E0E0; }"
    );
}

QPushButton *FriendRequestWidget::makeActionBtn(const QString &text, const QString &color)
{
    QPushButton *btn = new QPushButton(text);
    btn->setFixedSize(64, 28);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        QString("QPushButton { background: %1; color: white; border: none; "
                "border-radius: 4px; font-size: 12px; }"
                "QPushButton:hover { opacity: 0.85; }").arg(color)
    );
    return btn;
}

void FriendRequestWidget::setRequests(const QVector<FriendRequestInfo> &requests)
{
    m_listWidget->clear();

    for (const FriendRequestInfo &req : requests) {
        QString display = req.nickname.isEmpty() ? req.fromUsername : req.nickname;

        QWidget *itemWidget = new QWidget;
        QHBoxLayout *hLayout = new QHBoxLayout(itemWidget);
        hLayout->setContentsMargins(12, 8, 12, 8);
        hLayout->setSpacing(10);

        // 头像 (40x40)
        QLabel *avatarLabel = new QLabel;
        avatarLabel->setFixedSize(40, 40);
        avatarLabel->setScaledContents(true);
        if (!req.avatarData.isEmpty())
            avatarLabel->setPixmap(AvatarCropper::roundAvatar(req.avatarData, 40));
        else
            avatarLabel->setPixmap(AvatarCropper::defaultAvatar(display, 40));
        hLayout->addWidget(avatarLabel);

        // 昵称 + 留言
        QVBoxLayout *infoLayout = new QVBoxLayout;
        infoLayout->setSpacing(2);
        QLabel *nameLabel = new QLabel(display);
        nameLabel->setStyleSheet("font-size: 14px; color: #333; font-weight: bold;");
        infoLayout->addWidget(nameLabel);

        QLabel *msgLabel = new QLabel(req.message);
        msgLabel->setStyleSheet("font-size: 12px; color: #999;");
        msgLabel->setMaximumWidth(120);
        msgLabel->setWordWrap(false);
        infoLayout->addWidget(msgLabel);
        hLayout->addLayout(infoLayout, 1);

        // 接受 / 拒绝按钮
        QPushButton *acceptBtn = makeActionBtn("接受", "#07C160");
        QPushButton *rejectBtn = makeActionBtn("拒绝", "#FA5151");
        hLayout->addWidget(acceptBtn);
        hLayout->addWidget(rejectBtn);

        QListWidgetItem *item = new QListWidgetItem;
        item->setSizeHint(QSize(280, 64));
        item->setData(Qt::UserRole, req.requestId);
        m_listWidget->addItem(item);
        m_listWidget->setItemWidget(item, itemWidget);

        int rid = req.requestId;
        QString from = req.fromUsername;
        connect(acceptBtn, &QPushButton::clicked, this, [this, rid, from]() {
            emit acceptRequested(rid, from);
        });
        connect(rejectBtn, &QPushButton::clicked, this, [this, rid, from]() {
            emit rejectRequested(rid, from);
        });
    }
}

void FriendRequestWidget::removeRequest(int requestId)
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == requestId) {
            delete m_listWidget->takeItem(i);
            break;
        }
    }
}

void FriendRequestWidget::clear()
{
    m_listWidget->clear();
}
