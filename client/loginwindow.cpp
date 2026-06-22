#include "loginwindow.h"
#include "chatclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>

LoginWindow::LoginWindow(ChatClient *client, QWidget *parent)
    : QWidget(parent), m_client(client),
      m_currentHost("127.0.0.1"), m_currentPort(9527)
{
    setWindowTitle("远程问诊系统");
    setFixedSize(420, 520);

    setupUI();
    applyStyles();

    connect(m_client, &ChatClient::connected, this, &LoginWindow::onConnected);
    connect(m_client, &ChatClient::connectionFailed, this, &LoginWindow::onConnectionFailed);
    connect(m_client, &ChatClient::authResult, this, &LoginWindow::onAuthResult);

    // 自动连接内嵌服务端（localhost）
    m_client->connectToServer(m_currentHost, m_currentPort);
}

void LoginWindow::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->setContentsMargins(40, 50, 40, 30);

    QLabel *logo = new QLabel(QStringLiteral("\u2695"));
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("font-size: 48px;");
    layout->addWidget(logo);

    m_titleLabel = new QLabel("远程问诊系统");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setObjectName("titleLabel");
    layout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel("MedChat v1.0");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setObjectName("subtitleLabel");
    layout->addWidget(m_subtitleLabel);

    layout->addSpacing(16);

    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setPlaceholderText("用户名");
    m_usernameEdit->setObjectName("inputField");
    layout->addWidget(m_usernameEdit);

    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setPlaceholderText("密码");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setObjectName("inputField");
    layout->addWidget(m_passwordEdit);

    m_roleCombo = new QComboBox;
    m_roleCombo->addItem("医生", QString("doctor"));
    m_roleCombo->addItem("患者", QString("patient"));
    m_roleCombo->setObjectName("inputField");
    m_roleCombo->setFixedHeight(40);
    layout->addWidget(m_roleCombo);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    m_loginBtn = new QPushButton("登录");
    m_loginBtn->setObjectName("primaryBtn");
    m_loginBtn->setFixedHeight(42);
    m_registerBtn = new QPushButton("注册");
    m_registerBtn->setObjectName("secondaryBtn");
    m_registerBtn->setFixedHeight(42);
    btnLayout->addWidget(m_loginBtn);
    btnLayout->addWidget(m_registerBtn);
    layout->addLayout(btnLayout);

    m_statusLabel = new QLabel("正在连接服务器...");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    layout->addStretch();

    // 底部：切换服务器按钮
    m_switchBtn = new QPushButton("切换服务器");
    m_switchBtn->setObjectName("linkBtn");
    m_switchBtn->setFlat(true);
    m_switchBtn->setCursor(Qt::PointingHandCursor);
    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_switchBtn);
    bottomLayout->addStretch();
    layout->addLayout(bottomLayout);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(m_registerBtn, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    connect(m_switchBtn, &QPushButton::clicked, this, &LoginWindow::onSwitchServerClicked);
}

void LoginWindow::applyStyles()
{
    setStyleSheet(
        "QWidget { background-color: #f5f5f5; }"
        "#titleLabel { font-size: 24px; font-weight: bold; color: #333; }"
        "#subtitleLabel { font-size: 13px; color: #999; }"
        "#inputField { padding: 8px 12px; border: 1px solid #ddd; border-radius: 6px; "
        "  font-size: 14px; background: white; }"
        "#inputField:focus { border-color: #07c160; }"
        "#primaryBtn { background-color: #07c160; color: white; border: none; border-radius: 6px; "
        "  font-size: 15px; font-weight: bold; }"
        "#primaryBtn:hover { background-color: #06ad56; }"
        "#primaryBtn:pressed { background-color: #059a4c; }"
        "#secondaryBtn { background-color: white; color: #07c160; border: 1px solid #07c160; "
        "  border-radius: 6px; font-size: 15px; }"
        "#secondaryBtn:hover { background-color: #e8f8ef; }"
        "#statusLabel { color: #999; font-size: 12px; }"
        "#linkBtn { color: #07c160; font-size: 13px; text-decoration: underline; }"
        "#linkBtn:hover { color: #06ad56; }"
    );
}

void LoginWindow::showStatusMessage(const QString &msg, bool isError)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(isError ? "color: #e74c3c; font-size: 12px;"
                                         : "color: #07c160; font-size: 12px;");
}

void LoginWindow::onSwitchServerClicked()
{
    bool ok = false;
    // 先输入 IP
    QString host = QInputDialog::getText(this, "切换服务器",
        QString("服务器地址（当前: %1）:").arg(m_currentHost),
        QLineEdit::Normal, m_currentHost, &ok);
    if (!ok || host.trimmed().isEmpty()) return;
    host = host.trimmed();

    // 再输入端口
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
    // 短暂延迟确保旧连接完全断开
    QTimer::singleShot(200, this, [this]() {
        m_client->connectToServer(m_currentHost, m_currentPort);
    });
}

void LoginWindow::onConnected()
{
    showStatusMessage(QString("已连接 %1:%2 — 请登录或注册").arg(m_currentHost).arg(m_currentPort), false);
}

void LoginWindow::onConnectionFailed(const QString &error)
{
    showStatusMessage("连接失败: " + error, true);
}

void LoginWindow::onLoginClicked()
{
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
        showStatusMessage("请输入用户名和密码", true);
        return;
    }
    m_loginBtn->setEnabled(false);
    m_registerBtn->setEnabled(false);
    showStatusMessage("正在登录...", false);
    m_client->sendLogin(username, password);
}

void LoginWindow::onRegisterClicked()
{
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
        showStatusMessage("请输入用户名和密码", true);
        return;
    }
    QString role = m_roleCombo->currentData().toString();
    m_loginBtn->setEnabled(false);
    m_registerBtn->setEnabled(false);
    showStatusMessage("正在注册...", false);
    m_client->sendRegister(username, password, role);
}

void LoginWindow::onAuthResult(bool success, const QString &message, const QString &role)
{
    m_loginBtn->setEnabled(true);
    m_registerBtn->setEnabled(true);
    if (success) {
        emit loginSuccess(m_usernameEdit->text().trimmed(), role);
    } else {
        showStatusMessage(message, true);
    }
}
