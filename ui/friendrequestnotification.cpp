#include "friendrequestnotification.h"
#include "avatarcropper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QScreen>
#include <QPropertyAnimation>

FriendRequestNotification::FriendRequestNotification(const QString &nickname,
                                                     const QByteArray &avatarData,
                                                     QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(320);

    setupUI();

    // 设置头像和昵称（在 setupUI 之后，因为需要引用控件）
    QLabel *avatarLabel = findChild<QLabel*>("notifAvatar");
    QLabel *nameLabel   = findChild<QLabel*>("notifName");
    if (avatarLabel) {
        if (!avatarData.isEmpty())
            avatarLabel->setPixmap(AvatarCropper::roundAvatar(avatarData, 36));
        else
            avatarLabel->setPixmap(AvatarCropper::defaultAvatar(nickname, 36));
    }
    if (nameLabel)
        nameLabel->setText(nickname + " 请求添加你为好友");

    // 10 秒自动关闭
    m_autoCloseTimer = new QTimer(this);
    m_autoCloseTimer->setSingleShot(true);
    connect(m_autoCloseTimer, &QTimer::timeout, this, &QWidget::close);
    m_autoCloseTimer->start(10000);

    // 定位到屏幕右上角
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeo = screen->availableGeometry();
        move(screenGeo.right() - 340, screenGeo.top() + 60);
    }
}

void FriendRequestNotification::setupUI()
{
    // 外层容器（带圆角背景）
    QWidget *container = new QWidget(this);
    container->setObjectName("notifContainer");
    container->setStyleSheet(
        "#notifContainer { background: #FFFFFF; border-radius: 8px; "
        "  border: 1px solid #E0E0E0; }"
    );

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(container);

    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);

    // 标题行
    QHBoxLayout *titleRow = new QHBoxLayout;
    QLabel *iconLabel = new QLabel;
    iconLabel->setFixedSize(18, 18);
    iconLabel->setStyleSheet("background: #07C160; border-radius: 9px;");
    titleRow->addWidget(iconLabel);
    QLabel *titleLabel = new QLabel("好友请求");
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #333;");
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    mainLayout->addLayout(titleRow);

    // 内容行：头像 + 昵称
    QHBoxLayout *contentRow = new QHBoxLayout;
    contentRow->setSpacing(10);

    QLabel *avatarLabel = new QLabel;
    avatarLabel->setObjectName("notifAvatar");
    avatarLabel->setFixedSize(36, 36);
    avatarLabel->setScaledContents(true);
    contentRow->addWidget(avatarLabel);

    QLabel *nameLabel = new QLabel;
    nameLabel->setObjectName("notifName");
    nameLabel->setStyleSheet("font-size: 13px; color: #333;");
    nameLabel->setWordWrap(true);
    contentRow->addWidget(nameLabel, 1);
    mainLayout->addLayout(contentRow);

    // 按钮行
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    btnRow->addStretch();

    QPushButton *viewBtn = new QPushButton("查看");
    viewBtn->setFixedSize(60, 28);
    viewBtn->setCursor(Qt::PointingHandCursor);
    viewBtn->setStyleSheet(
        "QPushButton { background: #07C160; color: white; border: none; "
        "  border-radius: 4px; font-size: 12px; }"
        "QPushButton:hover { background: #06AD56; }"
    );
    connect(viewBtn, &QPushButton::clicked, this, [this]() {
        emit viewRequested();
        m_autoCloseTimer->stop();
        close();
    });
    btnRow->addWidget(viewBtn);

    QPushButton *closeBtn = new QPushButton("关闭");
    closeBtn->setFixedSize(60, 28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { background: #E0E0E0; color: #666; border: none; "
        "  border-radius: 4px; font-size: 12px; }"
        "QPushButton:hover { background: #D0D0D0; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        m_autoCloseTimer->stop();
        close();
    });
    btnRow->addWidget(closeBtn);

    mainLayout->addLayout(btnRow);

    // 设置固定高度
    setFixedHeight(mainLayout->sizeHint().height() + 4);
}
