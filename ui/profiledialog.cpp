#include "profiledialog.h"
#include "avatarcropper.h"
#include "common/constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPixmap>

ProfileDialog::ProfileDialog(Mode mode,
                             const QString &username,
                             const QString &nickname,
                             const QString &role,
                             const QByteArray &avatarData,
                             QWidget *parent)
    : QDialog(parent), m_mode(mode), m_username(username),
      m_nickname(nickname), m_role(role), m_avatarData(avatarData)
{
    setWindowTitle(mode == SelfProfile ? "个人资料" : "用户资料");
    setFixedSize(360, mode == SelfProfile ? 420 : 380);
    setModal(true);

    if (mode == SelfProfile)
        setupSelfUI();
    else
        setupOtherUI();
}

void ProfileDialog::setupSelfUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignHCenter);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(30, 20, 30, 20);

    // 头像（可点击更换）
    m_avatarLabel = new QLabel;
    m_avatarLabel->setFixedSize(120, 120);
    m_avatarLabel->setScaledContents(true);
    m_avatarLabel->setCursor(Qt::PointingHandCursor);
    m_avatarLabel->setToolTip("点击更换头像");
    m_avatarLabel->setStyleSheet("QLabel { border: 2px solid #ddd; border-radius: 60px; }");
    m_avatarLabel->installEventFilter(this);
    mainLayout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    updateAvatarDisplay();

    // 点击头像更换（用 mouse press event filter 代替 connect）
    // 我们用一个透明按钮覆盖在头像上来实现点击
    QPushButton *avatarBtn = new QPushButton(this);
    avatarBtn->setFixedSize(120, 120);
    avatarBtn->setStyleSheet("QPushButton { background: transparent; border: none; }"
                             "QPushButton:hover { background: rgba(0,0,0,0.1); border-radius: 60px; }");
    avatarBtn->setCursor(Qt::PointingHandCursor);
    connect(avatarBtn, &QPushButton::clicked, this, &ProfileDialog::onAvatarClicked);
    // 把按钮放到头像上方
    QVBoxLayout *avatarLayout = new QVBoxLayout;
    avatarLayout->addWidget(avatarBtn, 0, Qt::AlignHCenter);
    // 需要把按钮放到头像Label上方 - 用布局实现
    // 重新调整布局：移除头像，用stacked方式
    mainLayout->removeWidget(m_avatarLabel);
    QWidget *avatarContainer = new QWidget;
    QVBoxLayout *acLayout = new QVBoxLayout(avatarContainer);
    acLayout->setContentsMargins(0, 0, 0, 0);
    acLayout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    avatarBtn->setParent(m_avatarLabel);
    avatarBtn->move(0, 0);
    mainLayout->insertWidget(0, avatarContainer, 0, Qt::AlignHCenter);

    // 昵称编辑
    QHBoxLayout *nickLayout = new QHBoxLayout;
    nickLayout->addWidget(new QLabel("昵称："));
    m_nickEdit = new QLineEdit(m_nickname);
    m_nickEdit->setMaxLength(20);
    nickLayout->addWidget(m_nickEdit);
    mainLayout->addLayout(nickLayout);

    // 角色（只读）
    QHBoxLayout *roleLayout = new QHBoxLayout;
    roleLayout->addWidget(new QLabel("角色："));
    m_roleLabel = new QLabel(m_role == "doctor" ? "医生" : "患者");
    m_roleLabel->setStyleSheet("color: #666;");
    roleLayout->addWidget(m_roleLabel);
    roleLayout->addStretch();
    mainLayout->addLayout(roleLayout);

    // 用户名（只读）
    QHBoxLayout *userLayout = new QHBoxLayout;
    userLayout->addWidget(new QLabel("用户名："));
    m_userLabel = new QLabel(m_username);
    m_userLabel->setStyleSheet("color: #666;");
    userLayout->addWidget(m_userLabel);
    userLayout->addStretch();
    mainLayout->addLayout(userLayout);

    mainLayout->addStretch();

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    m_cancelBtn = new QPushButton("取消");
    m_saveBtn = new QPushButton("保存");
    m_saveBtn->setStyleSheet(
        "QPushButton { background-color: #07c160; color: white; border: none; "
        "padding: 8px 24px; border-radius: 4px; font-size: 14px; }"
        "QPushButton:hover { background-color: #06ad56; }"
    );
    m_cancelBtn->setStyleSheet(
        "QPushButton { background-color: #f0f0f0; color: #333; border: 1px solid #ddd; "
        "padding: 8px 24px; border-radius: 4px; font-size: 14px; }"
        "QPushButton:hover { background-color: #e0e0e0; }"
    );
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_saveBtn, &QPushButton::clicked, this, &ProfileDialog::onSaveClicked);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);
}

void ProfileDialog::setupOtherUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignHCenter);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(30, 20, 30, 20);

    // 头像（只读）
    m_avatarLabel = new QLabel;
    m_avatarLabel->setFixedSize(120, 120);
    m_avatarLabel->setScaledContents(true);
    m_avatarLabel->setStyleSheet("QLabel { border: 2px solid #ddd; border-radius: 60px; }");
    mainLayout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    updateAvatarDisplay();

    // 昵称
    QHBoxLayout *nickLayout = new QHBoxLayout;
    nickLayout->addWidget(new QLabel("昵称："));
    m_nickLabel = new QLabel(m_nickname);
    m_nickLabel->setStyleSheet("font-weight: bold; font-size: 16px;");
    nickLayout->addWidget(m_nickLabel);
    nickLayout->addStretch();
    mainLayout->addLayout(nickLayout);

    // 角色
    QHBoxLayout *roleLayout = new QHBoxLayout;
    roleLayout->addWidget(new QLabel("角色："));
    m_roleLabel = new QLabel(m_role == "doctor" ? "医生" : "患者");
    m_roleLabel->setStyleSheet("color: #666;");
    roleLayout->addWidget(m_roleLabel);
    roleLayout->addStretch();
    mainLayout->addLayout(roleLayout);

    // 用户名
    QHBoxLayout *userLayout = new QHBoxLayout;
    userLayout->addWidget(new QLabel("用户名："));
    m_userLabel = new QLabel(m_username);
    m_userLabel->setStyleSheet("color: #666;");
    userLayout->addWidget(m_userLabel);
    userLayout->addStretch();
    mainLayout->addLayout(userLayout);

    mainLayout->addStretch();

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    m_sendMsgBtn = new QPushButton("发送消息");
    m_sendMsgBtn->setStyleSheet(
        "QPushButton { background-color: #07c160; color: white; border: none; "
        "padding: 8px 20px; border-radius: 4px; font-size: 14px; }"
        "QPushButton:hover { background-color: #06ad56; }"
    );
    connect(m_sendMsgBtn, &QPushButton::clicked, this, &ProfileDialog::onSendMsgClicked);
    btnLayout->addWidget(m_sendMsgBtn);

    m_addFriendBtn = new QPushButton("加好友");
    m_addFriendBtn->setStyleSheet(
        "QPushButton { background-color: #f0f0f0; color: #333; border: 1px solid #ddd; "
        "padding: 8px 20px; border-radius: 4px; font-size: 14px; }"
        "QPushButton:hover { background-color: #e0e0e0; }"
    );
    connect(m_addFriendBtn, &QPushButton::clicked, this, &ProfileDialog::onAddFriendClicked);
    btnLayout->addWidget(m_addFriendBtn);

    mainLayout->addLayout(btnLayout);
}

void ProfileDialog::updateAvatarDisplay()
{
    QPixmap avatar;
    if (m_mode == SelfProfile && m_avatarChanged && !m_newAvatarData.isEmpty()) {
        avatar = AvatarCropper::roundAvatar(m_newAvatarData, 120);
    } else if (!m_avatarData.isEmpty()) {
        avatar = AvatarCropper::roundAvatar(m_avatarData, 120);
    } else {
        avatar = AvatarCropper::defaultAvatar(m_nickname, 120);
    }
    m_avatarLabel->setPixmap(avatar);
}

void ProfileDialog::onAvatarClicked()
{
    QPixmap cropped = AvatarCropper::selectAndCrop(this, Constants::AVATAR_DISPLAY_SIZE);
    if (cropped.isNull())
        return;

    // 检查大小
    QByteArray pngBytes = AvatarCropper::toPngBytes(cropped);
    if (pngBytes.size() > Constants::AVATAR_MAX_SIZE) {
        QMessageBox::warning(this, "头像过大",
            "头像文件不能超过 2MB，请选择更小的图片或裁剪后再试。");
        return;
    }

    m_newAvatarData = pngBytes;
    m_avatarChanged = true;
    updateAvatarDisplay();
}

void ProfileDialog::onSaveClicked()
{
    QString newNick = m_nickEdit->text().trimmed();
    if (newNick.isEmpty()) {
        QMessageBox::warning(this, "提示", "昵称不能为空");
        return;
    }

    QByteArray avatarToSave;
    if (m_avatarChanged)
        avatarToSave = m_newAvatarData;

    emit profileSaved(newNick, avatarToSave);
    accept();
}

void ProfileDialog::onSendMsgClicked()
{
    emit sendMessageRequested(m_username);
    accept();
}

void ProfileDialog::onAddFriendClicked()
{
    emit addFriendRequested(m_username);
}
