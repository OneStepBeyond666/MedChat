#include "changepassworddialog.h"
#include "client/chatclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

ChangePasswordDialog::ChangePasswordDialog(ChatClient *client, QWidget *parent)
    : QDialog(parent), m_client(client)
{
    setWindowTitle("修改密码");
    setFixedSize(360, 300);
    setupUI();
    applyStyles();

    connect(m_client, &ChatClient::changePasswordResult, this, &ChangePasswordDialog::onChangePasswordResult);
    connect(m_client, &ChatClient::kicked, this, &ChangePasswordDialog::onKicked);
}

void ChangePasswordDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 20, 30, 20);
    mainLayout->setSpacing(12);

    QLabel *title = new QLabel("修改密码");
    title->setAlignment(Qt::AlignCenter);
    title->setObjectName("titleLabel");
    mainLayout->addWidget(title);

    mainLayout->addSpacing(10);

    m_oldPassEdit = new QLineEdit;
    m_oldPassEdit->setPlaceholderText("当前密码");
    m_oldPassEdit->setEchoMode(QLineEdit::Password);
    m_oldPassEdit->setObjectName("inputField");
    mainLayout->addWidget(m_oldPassEdit);

    m_newPassEdit = new QLineEdit;
    m_newPassEdit->setPlaceholderText("新密码");
    m_newPassEdit->setEchoMode(QLineEdit::Password);
    m_newPassEdit->setObjectName("inputField");
    mainLayout->addWidget(m_newPassEdit);

    m_confirmPassEdit = new QLineEdit;
    m_confirmPassEdit->setPlaceholderText("确认新密码");
    m_confirmPassEdit->setEchoMode(QLineEdit::Password);
    m_confirmPassEdit->setObjectName("inputField");
    mainLayout->addWidget(m_confirmPassEdit);

    m_statusLabel = new QLabel;
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setObjectName("statusLabel");
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout;
    m_cancelBtn = new QPushButton("取消");
    m_cancelBtn->setObjectName("secondaryBtn");
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    m_changeBtn = new QPushButton("确认修改");
    m_changeBtn->setObjectName("primaryBtn");
    connect(m_changeBtn, &QPushButton::clicked, this, &ChangePasswordDialog::onChangeClicked);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_changeBtn);
    mainLayout->addLayout(btnLayout);
}

void ChangePasswordDialog::applyStyles()
{
    setStyleSheet(
        "QWidget { background-color: #ffffff; }"
        "#titleLabel { font-size: 20px; font-weight: bold; color: #333; }"
        "#inputField { padding: 10px 12px; border: 1px solid #ddd; border-radius: 6px; "
        "  font-size: 14px; background: white; }"
        "#inputField:focus { border-color: #07c160; }"
        "#primaryBtn { background-color: #07c160; color: white; border: none; border-radius: 6px; "
        "  padding: 10px 24px; font-size: 14px; font-weight: bold; }"
        "#primaryBtn:hover { background-color: #06ad56; }"
        "#primaryBtn:disabled { background-color: #a0d8b8; }"
        "#secondaryBtn { background-color: #f0f0f0; color: #333; border: 1px solid #ddd; "
        "  padding: 10px 24px; border-radius: 6px; font-size: 14px; }"
        "#secondaryBtn:hover { background-color: #e0e0e0; }"
        "#statusLabel { color: #e74c3c; font-size: 12px; }"
    );
}

void ChangePasswordDialog::onChangeClicked()
{
    QString oldPass = m_oldPassEdit->text();
    QString newPass = m_newPassEdit->text();
    QString confirm = m_confirmPassEdit->text();

    if (oldPass.isEmpty() || newPass.isEmpty()) {
        m_statusLabel->setText("请填写完整");
        return;
    }
    if (newPass != confirm) {
        m_statusLabel->setText("两次输入的新密码不一致");
        return;
    }

    m_changeBtn->setEnabled(false);
    m_statusLabel->setText("正在修改...");
    m_statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    m_client->sendChangePassword(oldPass, newPass);
}

void ChangePasswordDialog::onChangePasswordResult(bool success, const QString &message)
{
    m_changeBtn->setEnabled(true);
    if (success) {
        m_statusLabel->setStyleSheet("color: #07c160; font-size: 12px;");
        m_statusLabel->setText(message + "，请重新登录");
        // 服务端会发送 kicked 消息并断开连接，这里等待 kicked 信号
    } else {
        m_statusLabel->setStyleSheet("color: #e74c3c; font-size: 12px;");
        m_statusLabel->setText(message);
    }
}

void ChangePasswordDialog::onKicked(const QString &reason)
{
    Q_UNUSED(reason)
    QMessageBox::information(this, "修改成功", "密码已修改，请使用新密码重新登录。");
    accept();
}
