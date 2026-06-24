#include "profiledialog.h"
#include "avatarcropper.h"
#include "common/constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPixmap>
#include <QScrollArea>
#include <QFrame>
#include <QComboBox>
#include <QCalendarWidget>
#include <QMouseEvent>
#include <QScrollBar>

ProfileDialog::ProfileDialog(Mode mode,
                             const QString &username,
                             const QString &nickname,
                             const QString &role,
                             const QByteArray &avatarData,
                             const QString &signature,
                             int gender,
                             const QString &birthday,
                             const QString &region,
                             QWidget *parent)
    : QDialog(parent), m_mode(mode), m_username(username),
      m_nickname(nickname), m_role(role), m_avatarData(avatarData),
      m_signature(signature), m_gender(gender), m_birthday(birthday), m_region(region)
{
    setWindowTitle(mode == SelfProfile ? "个人资料" : "用户资料");
    setFixedSize(420, 520);
    setModal(true);

    // 主布局：浅灰背景
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 顶部保存按钮（仅自己资料页）
    if (mode == SelfProfile) {
        QHBoxLayout *topBar = new QHBoxLayout;
        topBar->setContentsMargins(16, 12, 16, 12);
        topBar->addStretch();
        m_saveBtn = new QPushButton("保存");
        m_saveBtn->setFixedSize(60, 32);
        m_saveBtn->setStyleSheet(
            "QPushButton { background-color: #07c160; color: white; border: none; "
            "border-radius: 4px; font-size: 13px; }"
            "QPushButton:hover { background-color: #06ad56; }"
        );
        connect(m_saveBtn, &QPushButton::clicked, this, &ProfileDialog::onSaveClicked);
        topBar->addWidget(m_saveBtn);
        mainLayout->addLayout(topBar);
    }

    // 滚动区域
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background-color: #f5f5f5; border: none; }");

    QWidget *contentWidget = new QWidget;
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 20);
    contentLayout->setSpacing(0);
    contentLayout->setAlignment(Qt::AlignTop);

    if (mode == SelfProfile)
        setupSelfUI(contentLayout);
    else
        setupOtherUI(contentLayout);

    contentLayout->addStretch();
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    applyStyles();
}

QWidget* ProfileDialog::createGroupTitle(const QString &title)
{
    QWidget *widget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(16, 12, 16, 8);
    QLabel *label = new QLabel(title);
    label->setStyleSheet("font-size: 12px; color: #999; font-weight: bold;");
    layout->addWidget(label);
    layout->addStretch();
    return widget;
}

QWidget* ProfileDialog::createSettingItem(const QString &labelText, QWidget *widget, QWidget *extra)
{
    QWidget *item = new QWidget;
    item->setStyleSheet("background-color: white;");
    QHBoxLayout *layout = new QHBoxLayout(item);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(12);

    QLabel *label = new QLabel(labelText);
    label->setFixedWidth(60);
    label->setStyleSheet("font-size: 14px; color: #333;");
    layout->addWidget(label);

    layout->addWidget(widget, 1);
    if (extra)
        layout->addWidget(extra);

    return item;
}

QWidget* ProfileDialog::createClickableItem(const QString &labelText, const QString &valueText)
{
    QWidget *item = new QWidget;
    item->setCursor(Qt::PointingHandCursor);
    item->setStyleSheet("background-color: white;");
    QHBoxLayout *layout = new QHBoxLayout(item);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(12);

    QLabel *label = new QLabel(labelText);
    label->setFixedWidth(60);
    label->setStyleSheet("font-size: 14px; color: #333;");
    layout->addWidget(label);

    layout->addStretch();

    QLabel *value = new QLabel(valueText);
    value->setStyleSheet("font-size: 14px; color: #666;");
    layout->addWidget(value);

    QLabel *arrow = new QLabel(">");
    arrow->setStyleSheet("font-size: 14px; color: #ccc;");
    layout->addWidget(arrow);

    return item;
}

void ProfileDialog::setupSelfUI(QVBoxLayout *mainLayout)
{
    // 头像区
    QWidget *avatarArea = new QWidget;
    avatarArea->setStyleSheet("background-color: white;");
    QHBoxLayout *avatarLayout = new QHBoxLayout(avatarArea);
    avatarLayout->setContentsMargins(16, 16, 16, 16);

    m_avatarLabel = new QLabel;
    m_avatarLabel->setFixedSize(100, 100);
    m_avatarLabel->setScaledContents(true);
    m_avatarLabel->setCursor(Qt::PointingHandCursor);
    m_avatarLabel->setToolTip("点击更换头像");
    m_avatarLabel->setStyleSheet(
        "QLabel { border-radius: 50px; border: 3px solid #ffffff; }"
    );
    updateAvatarDisplay();

    // 透明按钮覆盖头像
    QPushButton *avatarBtn = new QPushButton(avatarArea);
    avatarBtn->setFixedSize(100, 100);
    avatarBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 50px; }"
        "QPushButton:hover { background: rgba(0,0,0,0.1); }"
    );
    avatarBtn->setCursor(Qt::PointingHandCursor);
    connect(avatarBtn, &QPushButton::clicked, this, &ProfileDialog::onAvatarClicked);
    avatarBtn->move(16, 16);

    avatarLayout->addWidget(m_avatarLabel);
    avatarLayout->addStretch();
    mainLayout->addWidget(avatarArea);

    // 分隔线
    QFrame *line1 = new QFrame;
    line1->setFrameShape(QFrame::HLine);
    line1->setStyleSheet("color: #e0e0e0;");
    mainLayout->addWidget(line1);

    // 基本信息分组
    mainLayout->addWidget(createGroupTitle("基本信息"));

    // 昵称
    m_nickEdit = new QLineEdit(m_nickname);
    m_nickEdit->setMaxLength(36);
    m_nickEdit->setStyleSheet("border: none; font-size: 14px;");
    m_nickCounter = new QLabel(QString("%1/36").arg(m_nickname.length()));
    m_nickCounter->setStyleSheet("color: #bbb; font-size: 12px;");
    connect(m_nickEdit, &QLineEdit::textChanged, this, &ProfileDialog::onNickTextChanged);
    mainLayout->addWidget(createSettingItem("昵称", m_nickEdit, m_nickCounter));

    addSeparator(mainLayout);

    // 个签
    m_sigEdit = new QLineEdit(m_signature.isEmpty() ? "这个人很懒，什么都没写" : m_signature);
    m_sigEdit->setMaxLength(80);
    m_sigEdit->setStyleSheet("border: none; font-size: 14px;");
    m_sigCounter = new QLabel(QString("%1/80").arg(m_signature.length()));
    m_sigCounter->setStyleSheet("color: #bbb; font-size: 12px;");
    connect(m_sigEdit, &QLineEdit::textChanged, this, &ProfileDialog::onSigTextChanged);
    mainLayout->addWidget(createSettingItem("个签", m_sigEdit, m_sigCounter));

    addSeparator(mainLayout);

    // 性别
    m_genderCombo = new QComboBox;
    m_genderCombo->addItem("未知", 0);
    m_genderCombo->addItem("男", 1);
    m_genderCombo->addItem("女", 2);
    m_genderCombo->setCurrentIndex(m_gender);
    m_genderCombo->setStyleSheet("border: none; font-size: 14px; background: transparent;");
    mainLayout->addWidget(createSettingItem("性别", m_genderCombo));

    addSeparator(mainLayout);

    // 生日
    m_birthdayEdit = new QDateEdit;
    m_birthdayEdit->setCalendarPopup(true);
    m_birthdayEdit->setDisplayFormat("yyyy-MM-dd");
    if (!m_birthday.isEmpty())
        m_birthdayEdit->setDate(QDate::fromString(m_birthday, "yyyy-MM-dd"));
    else
        m_birthdayEdit->setDate(QDate::currentDate());
    m_birthdayEdit->setStyleSheet("border: none; font-size: 14px; background: transparent;");
    mainLayout->addWidget(createSettingItem("生日", m_birthdayEdit));

    addSeparator(mainLayout);

    // 地区
    m_regionEdit = new QLineEdit(m_region);
    m_regionEdit->setPlaceholderText("填写所在地区");
    m_regionEdit->setStyleSheet("border: none; font-size: 14px;");
    mainLayout->addWidget(createSettingItem("地区", m_regionEdit));

    addSeparator(mainLayout);

    // 用户名（只读）
    m_userLabel = new QLabel(m_username);
    m_userLabel->setStyleSheet("color: #999; font-size: 14px;");
    m_userLabel->setEnabled(false);
    QWidget *userItem = createSettingItem("用户名", m_userLabel);
    userItem->setStyleSheet("background-color: #f0f0f0;");
    mainLayout->addWidget(userItem);

    addSeparator(mainLayout);

    // 角色（只读）
    m_roleLabel = new QLabel(m_role == "doctor" ? "医生" : "患者");
    m_roleLabel->setStyleSheet("color: #999; font-size: 14px;");
    m_roleLabel->setEnabled(false);
    QWidget *roleItem = createSettingItem("角色", m_roleLabel);
    roleItem->setStyleSheet("background-color: #f0f0f0;");
    mainLayout->addWidget(roleItem);

    // 账号安全分组
    addSeparator(mainLayout);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(createGroupTitle("账号安全"));

    // 修改密码入口 - 白色卡片风格
    QWidget *passItem = new QWidget;
    passItem->setStyleSheet("background-color: white;");
    passItem->setCursor(Qt::PointingHandCursor);
    QHBoxLayout *passLayout = new QHBoxLayout(passItem);
    passLayout->setContentsMargins(16, 12, 16, 12);
    QLabel *passLabel = new QLabel("修改密码");
    passLabel->setStyleSheet("font-size: 14px; color: #333;");
    passLayout->addWidget(passLabel);
    passLayout->addStretch();
    QLabel *passArrow = new QLabel(">");
    passArrow->setStyleSheet("font-size: 14px; color: #ccc;");
    passLayout->addWidget(passArrow);

    QPushButton *passBtn = new QPushButton(passItem);
    passBtn->setFixedSize(passItem->sizeHint().width(), 44);
    passBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; }"
        "QPushButton:hover { background: rgba(0,0,0,0.02); }"
    );
    passBtn->setCursor(Qt::PointingHandCursor);
    connect(passBtn, &QPushButton::clicked, this, &ProfileDialog::onChangePasswordClicked);
    mainLayout->addWidget(passItem);
}

void ProfileDialog::addSeparator(QVBoxLayout *layout)
{
    QFrame *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #e8e8e8; max-height: 1px;");
    line->setFixedHeight(1);
    layout->addWidget(line);
}

void ProfileDialog::setupOtherUI(QVBoxLayout *mainLayout)
{
    // 头像（只读）
    QWidget *avatarArea = new QWidget;
    avatarArea->setStyleSheet("background-color: white;");
    QHBoxLayout *avatarLayout = new QHBoxLayout(avatarArea);
    avatarLayout->setContentsMargins(16, 16, 16, 16);

    m_avatarLabel = new QLabel;
    m_avatarLabel->setFixedSize(100, 100);
    m_avatarLabel->setScaledContents(true);
    m_avatarLabel->setStyleSheet("QLabel { border-radius: 50px; border: 3px solid #ffffff; }");
    updateAvatarDisplay();
    avatarLayout->addWidget(m_avatarLabel);
    avatarLayout->addStretch();
    mainLayout->addWidget(avatarArea);

    addSeparator(mainLayout);

    // 基本信息
    mainLayout->addWidget(createGroupTitle("基本信息"));

    // 昵称
    QLabel *nickLabel = new QLabel(m_nickname);
    nickLabel->setStyleSheet("font-size: 14px; color: #333; font-weight: bold;");
    mainLayout->addWidget(createSettingItem("昵称", nickLabel));

    addSeparator(mainLayout);

    // 个签
    QLabel *sigLabel = new QLabel(m_signature.isEmpty() ? "这个人很懒，什么都没写" : m_signature);
    sigLabel->setStyleSheet("font-size: 14px; color: #666;");
    mainLayout->addWidget(createSettingItem("个签", sigLabel));

    addSeparator(mainLayout);

    // 性别
    QString genderText = m_gender == 1 ? "男" : (m_gender == 2 ? "女" : "未知");
    QLabel *genderLabel = new QLabel(genderText);
    genderLabel->setStyleSheet("font-size: 14px; color: #333;");
    mainLayout->addWidget(createSettingItem("性别", genderLabel));

    addSeparator(mainLayout);

    // 生日
    QLabel *birthLabel = new QLabel(m_birthday.isEmpty() ? "未填写" : m_birthday);
    birthLabel->setStyleSheet("font-size: 14px; color: #333;");
    mainLayout->addWidget(createSettingItem("生日", birthLabel));

    addSeparator(mainLayout);

    // 地区
    QLabel *regionLabel = new QLabel(m_region.isEmpty() ? "未填写" : m_region);
    regionLabel->setStyleSheet("font-size: 14px; color: #333;");
    mainLayout->addWidget(createSettingItem("地区", regionLabel));

    addSeparator(mainLayout);

    // 用户名
    QLabel *userLabel = new QLabel(m_username);
    userLabel->setStyleSheet("font-size: 14px; color: #999;");
    QWidget *userItem = createSettingItem("用户名", userLabel);
    userItem->setStyleSheet("background-color: #f0f0f0;");
    mainLayout->addWidget(userItem);

    addSeparator(mainLayout);

    // 角色
    QLabel *roleLabel = new QLabel(m_role == "doctor" ? "医生" : "患者");
    roleLabel->setStyleSheet("font-size: 14px; color: #999;");
    QWidget *roleItem = createSettingItem("角色", roleLabel);
    roleItem->setStyleSheet("background-color: #f0f0f0;");
    mainLayout->addWidget(roleItem);

    // 按钮区
    mainLayout->addSpacing(20);
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(16, 0, 16, 0);
    btnLayout->addStretch();

    m_sendMsgBtn = new QPushButton("发送消息");
    m_sendMsgBtn->setStyleSheet(
        "QPushButton { background-color: #07c160; color: white; border: none; "
        "padding: 10px 24px; border-radius: 6px; font-size: 14px; }"
        "QPushButton:hover { background-color: #06ad56; }"
    );
    connect(m_sendMsgBtn, &QPushButton::clicked, this, &ProfileDialog::onSendMsgClicked);
    btnLayout->addWidget(m_sendMsgBtn);

    m_addFriendBtn = new QPushButton("加好友");
    m_addFriendBtn->setStyleSheet(
        "QPushButton { background-color: #f0f0f0; color: #333; border: 1px solid #ddd; "
        "padding: 10px 24px; border-radius: 6px; font-size: 14px; }"
        "QPushButton:hover { background-color: #e0e0e0; }"
    );
    connect(m_addFriendBtn, &QPushButton::clicked, this, &ProfileDialog::onAddFriendClicked);
    btnLayout->addWidget(m_addFriendBtn);

    mainLayout->addLayout(btnLayout);
}

void ProfileDialog::applyStyles()
{
    setStyleSheet(
        // 全局
        "QWidget { background-color: #f5f5f5; font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif; }"

        // 输入框
        "QLineEdit { background: transparent; border: none; padding: 4px 2px; font-size: 14px; color: #333; }"
        "QLineEdit:focus { border-bottom: 1px solid #07c160; }"
        "QLineEdit:disabled { color: #999; }"

        // 下拉框 — 主体
        "QComboBox { background: transparent; border: none; padding: 4px 2px; font-size: 14px; color: #333; }"
        "QComboBox:focus { border-bottom: 1px solid #07c160; }"

        // 下拉框 — 箭头按钮
        "QComboBox::drop-down { border: none; width: 24px; subcontrol-origin: padding; subcontrol-position: center right; }"
        "QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; "
        "  border-top: 5px solid #999; margin-right: 4px; }"
        "QComboBox::down-arrow:hover { border-top-color: #07c160; }"

        // 下拉框 — 弹出列表（关键：强制白色背景）
        "QComboBox QAbstractItemView { background-color: #ffffff; border: 1px solid #e0e0e0; "
        "  selection-background-color: #f0f0f0; selection-color: #333; color: #333; "
        "  outline: none; padding: 4px; }"
        "QComboBox QAbstractItemView::item { padding: 6px 12px; }"
        "QComboBox QAbstractItemView::item:hover { background-color: #f5f5f5; }"

        // 日期编辑框
        "QDateEdit { background: transparent; border: none; padding: 4px 2px; font-size: 14px; color: #333; }"
        "QDateEdit:focus { border-bottom: 1px solid #07c160; }"
        "QDateEdit::drop-down { border: none; width: 24px; subcontrol-origin: padding; subcontrol-position: center right; }"
        "QDateEdit::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; "
        "  border-top: 5px solid #999; margin-right: 4px; }"
        "QDateEdit::down-arrow:hover { border-top-color: #07c160; }"

        // 日历弹窗（关键：强制白色主题）
        "QCalendarWidget { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 4px; }"

        // 日历 — 顶部导航栏
        "QCalendarWidget QWidget#qt_calendar_navigationbar { background-color: #ffffff; }"
        "QCalendarWidget QToolButton { background-color: #ffffff; color: #333; border: none; "
        "  border-radius: 4px; padding: 4px 8px; font-size: 13px; }"
        "QCalendarWidget QToolButton:hover { background-color: #f0f0f0; }"
        "QCalendarWidget QToolButton::menu-indicator { image: none; }"

        // 日历 — 月份/年份菜单
        "QCalendarWidget QMenu { background-color: #ffffff; border: 1px solid #e0e0e0; color: #333; }"
        "QCalendarWidget QMenu::item:selected { background-color: #f0f0f0; }"

        // 日历 — 星期标题行
        "QCalendarWidget QWidget { background-color: #ffffff; }"
        "QCalendarWidget QTableView { background-color: #ffffff; gridline-color: #f0f0f0; "
        "  selection-background-color: #07c160; selection-color: #ffffff; "
        "  alternate-background-color: #ffffff; color: #333; outline: none; }"
        "QCalendarWidget QTableView::item:hover { background-color: #f5f5f5; color: #333; }"
        "QCalendarWidget QTableView::item:selected { background-color: #07c160; color: #ffffff; border-radius: 4px; }"

        // 日历 — 非本月日期
        "QCalendarWidget QTableView::item:disabled { color: #ccc; }"

        // 滚动区域
        "QScrollArea { background-color: #f5f5f5; border: none; }"
        "QScrollBar:vertical { background: #f5f5f5; width: 6px; margin: 0; border: none; }"
        "QScrollBar::handle:vertical { background: #ccc; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #aaa; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );
}

void ProfileDialog::updateAvatarDisplay()
{
    QPixmap avatar;
    if (m_mode == SelfProfile && m_avatarChanged && !m_newAvatarData.isEmpty()) {
        avatar = AvatarCropper::roundAvatar(m_newAvatarData, 100);
    } else if (!m_avatarData.isEmpty()) {
        avatar = AvatarCropper::roundAvatar(m_avatarData, 100);
    } else {
        avatar = AvatarCropper::defaultAvatar(m_nickname, 100);
    }
    m_avatarLabel->setPixmap(avatar);
}

void ProfileDialog::onAvatarClicked()
{
    QPixmap cropped = AvatarCropper::selectAndCrop(this, Constants::AVATAR_DISPLAY_SIZE);
    if (cropped.isNull())
        return;

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

void ProfileDialog::onNickTextChanged(const QString &text)
{
    m_nickCounter->setText(QString("%1/36").arg(text.length()));
}

void ProfileDialog::onSigTextChanged(const QString &text)
{
    m_sigCounter->setText(QString("%1/80").arg(text.length()));
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

    QString newSig = m_sigEdit->text().trimmed();
    int newGender = m_genderCombo->currentData().toInt();
    QString newBirthday = m_birthdayEdit->date().toString("yyyy-MM-dd");
    QString newRegion = m_regionEdit->text().trimmed();

    emit profileSaved(newNick, avatarToSave, newSig, newGender, newBirthday, newRegion);
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

void ProfileDialog::onChangePasswordClicked()
{
    emit changePasswordRequested();
}
