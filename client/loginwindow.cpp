#include "loginwindow.h"
#include "chatclient.h"
#include "ui/forgotpassworddialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QTimer>
#include <QCryptographicHash>

// ============================================================
// 构造
// ============================================================

LoginWindow::LoginWindow(ChatClient *client, QWidget *parent)
    : QWidget(parent), m_client(client),
      m_currentHost("127.0.0.1"), m_currentPort(9527)
{
    setWindowTitle("远程问诊系统");
    setFixedSize(420, 580);

    setupUI();
    applyStyles();

    connect(m_client, &ChatClient::connected, this, &LoginWindow::onConnected);
    connect(m_client, &ChatClient::connectionFailed, this, &LoginWindow::onConnectionFailed);
    connect(m_client, &ChatClient::authResult, this, &LoginWindow::onAuthResult);

    m_client->connectToServer(m_currentHost, m_currentPort);
}

// ============================================================
// UI 构建
// ============================================================

void LoginWindow::setupUI()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget;

    // ======================== 登录页 ========================
    QWidget *loginPage = new QWidget;
    QVBoxLayout *lp = new QVBoxLayout(loginPage);
    lp->setSpacing(14);
    lp->setContentsMargins(40, 50, 40, 30);

    QLabel *title1 = new QLabel("远程问诊系统");
    title1->setAlignment(Qt::AlignCenter);
    title1->setObjectName("titleLabel");
    lp->addWidget(title1);

    QLabel *sub1 = new QLabel("MedChat v0.7.8");
    sub1->setAlignment(Qt::AlignCenter);
    sub1->setObjectName("subtitleLabel");
    lp->addWidget(sub1);

    lp->addSpacing(20);

    m_loginUserEdit = new QLineEdit;
    m_loginUserEdit->setPlaceholderText("用户名");
    m_loginUserEdit->setObjectName("inputField");
    lp->addWidget(m_loginUserEdit);

    m_loginPassEdit = new QLineEdit;
    m_loginPassEdit->setPlaceholderText("密码");
    m_loginPassEdit->setEchoMode(QLineEdit::Password);
    m_loginPassEdit->setObjectName("inputField");
    lp->addWidget(m_loginPassEdit);

    // 忘记密码链接（右对齐）
    QHBoxLayout *forgotLayout = new QHBoxLayout;
    forgotLayout->addStretch();
    m_forgotPassBtn = new QPushButton("忘记密码？");
    m_forgotPassBtn->setObjectName("linkBtn");
    m_forgotPassBtn->setFlat(true);
    m_forgotPassBtn->setCursor(Qt::PointingHandCursor);
    connect(m_forgotPassBtn, &QPushButton::clicked, this, &LoginWindow::onForgotPasswordClicked);
    forgotLayout->addWidget(m_forgotPassBtn);
    lp->addLayout(forgotLayout);

    m_loginBtn = new QPushButton("登 录");
    m_loginBtn->setObjectName("primaryBtn");
    m_loginBtn->setFixedHeight(44);
    lp->addWidget(m_loginBtn);

    m_loginStatusLabel = new QLabel("正在连接服务器...");
    m_loginStatusLabel->setAlignment(Qt::AlignCenter);
    m_loginStatusLabel->setObjectName("statusLabel");
    m_loginStatusLabel->setWordWrap(true);
    lp->addWidget(m_loginStatusLabel);

    lp->addStretch();

    // 底部链接行
    QHBoxLayout *loginBottom = new QHBoxLayout;
    QPushButton *goRegBtn = new QPushButton("没有账号？去注册");
    goRegBtn->setObjectName("linkBtn");
    goRegBtn->setFlat(true);
    goRegBtn->setCursor(Qt::PointingHandCursor);
    connect(goRegBtn, &QPushButton::clicked, this, &LoginWindow::showRegisterPage);

    m_loginSwitchBtn = new QPushButton("切换服务器");
    m_loginSwitchBtn->setObjectName("linkBtn");
    m_loginSwitchBtn->setFlat(true);
    m_loginSwitchBtn->setCursor(Qt::PointingHandCursor);
    connect(m_loginSwitchBtn, &QPushButton::clicked, this, &LoginWindow::onSwitchServerClicked);

    loginBottom->addWidget(goRegBtn);
    loginBottom->addStretch();
    loginBottom->addWidget(m_loginSwitchBtn);
    lp->addLayout(loginBottom);

    m_stack->addWidget(loginPage);

    // ======================== 注册页 ========================
    QWidget *regPage = new QWidget;
    QVBoxLayout *rp = new QVBoxLayout(regPage);
    rp->setSpacing(12);
    rp->setContentsMargins(40, 40, 40, 30);

    QLabel *title2 = new QLabel("创建新账号");
    title2->setAlignment(Qt::AlignCenter);
    title2->setObjectName("titleLabel");
    rp->addWidget(title2);

    rp->addSpacing(16);

    m_regUserEdit = new QLineEdit;
    m_regUserEdit->setPlaceholderText("用户名（登录凭证）");
    m_regUserEdit->setObjectName("inputField");
    rp->addWidget(m_regUserEdit);

    m_regNickEdit = new QLineEdit;
    m_regNickEdit->setPlaceholderText("昵称（显示名称）");
    m_regNickEdit->setObjectName("inputField");
    rp->addWidget(m_regNickEdit);

    m_regPassEdit = new QLineEdit;
    m_regPassEdit->setPlaceholderText("密码");
    m_regPassEdit->setEchoMode(QLineEdit::Password);
    m_regPassEdit->setObjectName("inputField");
    rp->addWidget(m_regPassEdit);

    m_regPassConfirmEdit = new QLineEdit;
    m_regPassConfirmEdit->setPlaceholderText("确认密码");
    m_regPassConfirmEdit->setEchoMode(QLineEdit::Password);
    m_regPassConfirmEdit->setObjectName("inputField");
    rp->addWidget(m_regPassConfirmEdit);

    m_regSecQuestionEdit = new QLineEdit;
    m_regSecQuestionEdit->setPlaceholderText("密保问题（如：我的小学名字？）");
    m_regSecQuestionEdit->setObjectName("inputField");
    rp->addWidget(m_regSecQuestionEdit);

    m_regSecAnswerEdit = new QLineEdit;
    m_regSecAnswerEdit->setPlaceholderText("密保答案");
    m_regSecAnswerEdit->setObjectName("inputField");
    rp->addWidget(m_regSecAnswerEdit);

    m_regRoleCombo = new QComboBox;
    m_regRoleCombo->addItem("我是医生", QString("doctor"));
    m_regRoleCombo->addItem("我是患者", QString("patient"));
    m_regRoleCombo->setObjectName("inputField");
    m_regRoleCombo->setFixedHeight(40);
    rp->addWidget(m_regRoleCombo);

    m_regBtn = new QPushButton("注 册");
    m_regBtn->setObjectName("primaryBtn");
    m_regBtn->setFixedHeight(44);
    rp->addWidget(m_regBtn);

    m_regStatusLabel = new QLabel("");
    m_regStatusLabel->setAlignment(Qt::AlignCenter);
    m_regStatusLabel->setObjectName("statusLabel");
    m_regStatusLabel->setWordWrap(true);
    rp->addWidget(m_regStatusLabel);

    rp->addStretch();

    // 底部链接行
    QHBoxLayout *regBottom = new QHBoxLayout;
    QPushButton *goLoginBtn = new QPushButton("已有账号？去登录");
    goLoginBtn->setObjectName("linkBtn");
    goLoginBtn->setFlat(true);
    goLoginBtn->setCursor(Qt::PointingHandCursor);
    connect(goLoginBtn, &QPushButton::clicked, this, &LoginWindow::showLoginPage);

    m_regSwitchBtn = new QPushButton("切换服务器");
    m_regSwitchBtn->setObjectName("linkBtn");
    m_regSwitchBtn->setFlat(true);
    m_regSwitchBtn->setCursor(Qt::PointingHandCursor);
    connect(m_regSwitchBtn, &QPushButton::clicked, this, &LoginWindow::onSwitchServerClicked);

    regBottom->addWidget(goLoginBtn);
    regBottom->addStretch();
    regBottom->addWidget(m_regSwitchBtn);
    rp->addLayout(regBottom);

    m_stack->addWidget(regPage);

    // 默认显示登录页
    m_stack->setCurrentIndex(0);
    root->addWidget(m_stack);

    // 信号连接
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(m_regBtn,   &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
}

// ============================================================
// 样式
// ============================================================

void LoginWindow::applyStyles()
{
    setStyleSheet(
        "QWidget { background-color: #f5f5f5; }"
        "#titleLabel { font-size: 22px; font-weight: bold; color: #333; }"
        "#subtitleLabel { font-size: 13px; color: #999; }"
        "#inputField { padding: 8px 12px; border: 1px solid #ddd; border-radius: 6px; "
        "  font-size: 14px; background: white; }"
        "#inputField:focus { border-color: #07c160; }"
        "#primaryBtn { background-color: #07c160; color: white; border: none; border-radius: 6px; "
        "  font-size: 15px; font-weight: bold; }"
        "#primaryBtn:hover { background-color: #06ad56; }"
        "#primaryBtn:pressed { background-color: #059a4c; }"
        "#primaryBtn:disabled { background-color: #a0d8b8; }"
        "#statusLabel { color: #999; font-size: 12px; }"
        "#linkBtn { color: #07c160; font-size: 13px; }"
        "#linkBtn:hover { color: #06ad56; }"
    );
}

// ============================================================
// 页面切换
// ============================================================

void LoginWindow::showLoginPage()
{
    m_stack->setCurrentIndex(0);
    m_loginStatusLabel->setText("");
}

void LoginWindow::showRegisterPage()
{
    m_stack->setCurrentIndex(1);
    m_regStatusLabel->setText("");
}

// ============================================================
// 状态消息
// ============================================================

void LoginWindow::showStatusMessage(const QString &msg, bool isError)
{
    // 根据当前页面选择对应的 status label
    QLabel *label = (m_stack->currentIndex() == 0) ? m_loginStatusLabel : m_regStatusLabel;
    label->setText(msg);
    label->setStyleSheet(isError ? "color: #e74c3c; font-size: 12px;"
                                 : "color: #07c160; font-size: 12px;");
}

void LoginWindow::setButtonsEnabled(bool enabled)
{
    m_loginBtn->setEnabled(enabled);
    m_regBtn->setEnabled(enabled);
}

// ============================================================
// 服务器切换
// ============================================================

void LoginWindow::onSwitchServerClicked()
{
    bool ok = false;
    QString host = QInputDialog::getText(this, "切换服务器",
        QString("服务器地址（当前: %1）:").arg(m_currentHost),
        QLineEdit::Normal, m_currentHost, &ok);
    if (!ok || host.trimmed().isEmpty()) return;
    host = host.trimmed();

    int port = QInputDialog::getInt(this, "切换服务器",
        "端口号:", m_currentPort, 1, 65535, 1, &ok);
    if (!ok) return;

    reconnectTo(host, static_cast<quint16>(port));
}

void LoginWindow::reconnectTo(const QString &host, quint16 port)
{
    m_client->disconnectFromServer();
    m_currentHost = host;
    m_currentPort = port;
    showStatusMessage(QString("正在连接 %1:%2 ...").arg(host).arg(port), false);
    QTimer::singleShot(200, this, [this]() {
        m_client->connectToServer(m_currentHost, m_currentPort);
    });
}

// ============================================================
// 网络回调
// ============================================================

void LoginWindow::onConnected()
{
    showStatusMessage(QString("已连接 %1:%2").arg(m_currentHost).arg(m_currentPort), false);
}

void LoginWindow::onConnectionFailed(const QString &error)
{
    showStatusMessage("连接失败: " + error, true);
}

// ============================================================
// 登录
// ============================================================

void LoginWindow::onLoginClicked()
{
    QString username = m_loginUserEdit->text().trimmed();
    QString password = m_loginPassEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
        showStatusMessage("请输入用户名和密码", true);
        return;
    }
    setButtonsEnabled(false);
    showStatusMessage("正在登录...", false);
    m_client->sendLogin(username, password);
}

// ============================================================
// 注册
// ============================================================

void LoginWindow::onRegisterClicked()
{
    QString username = m_regUserEdit->text().trimmed();
    QString nickname = m_regNickEdit->text().trimmed();
    QString password = m_regPassEdit->text();
    QString confirm  = m_regPassConfirmEdit->text();
    QString role     = m_regRoleCombo->currentData().toString();
    QString secQuestion = m_regSecQuestionEdit->text().trimmed();
    QString secAnswer   = m_regSecAnswerEdit->text().trimmed();

    if (username.isEmpty() || password.isEmpty()) {
        showStatusMessage("用户名和密码不能为空", true);
        return;
    }
    if (secQuestion.isEmpty() || secAnswer.isEmpty()) {
        showStatusMessage("请设置密保问题和答案", true);
        return;
    }
    if (nickname.isEmpty())
        nickname = username;  // 昵称默认等于用户名
    if (password != confirm) {
        showStatusMessage("两次输入的密码不一致", true);
        return;
    }

    // 密保答案预处理：小写 + 去空格，然后 SHA256
    QString answerProcessed = secAnswer.toLower().trimmed();
    QString answerHash = QString::fromLatin1(
        QCryptographicHash::hash(answerProcessed.toUtf8(), QCryptographicHash::Sha256).toHex()
    );

    setButtonsEnabled(false);
    showStatusMessage("正在注册...", false);
    m_client->sendRegister(username, password, role, nickname, secQuestion, answerHash);
}

// ============================================================
// 忘记密码
// ============================================================

void LoginWindow::onForgotPasswordClicked()
{
    ForgotPasswordDialog dlg(m_client, this);
    dlg.exec();
}

// ============================================================
// 认证结果
// ============================================================

void LoginWindow::onAuthResult(bool success, const QString &message, const QString &role, const QByteArray &avatarData)
{
    setButtonsEnabled(true);
    if (success) {
        // 登录成功 → 使用登录页的用户名；注册成功 → 使用注册页的用户名
        QString username = (m_stack->currentIndex() == 0)
            ? m_loginUserEdit->text().trimmed()
            : m_regUserEdit->text().trimmed();
        emit loginSuccess(username, role, m_client->myNickname(), avatarData);
    } else {
        showStatusMessage(message, true);
    }
}
