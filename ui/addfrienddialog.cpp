#include "addfrienddialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

AddFriendDialog::AddFriendDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("添加朋友");
    resize(360, 200);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    QLabel *titleLabel = new QLabel("输入对方用户名：");
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #333;");
    mainLayout->addWidget(titleLabel);

    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setPlaceholderText("请输入用户名...");
    m_usernameEdit->setStyleSheet(
        "QLineEdit { border: 1px solid #ccc; border-radius: 4px; "
        "padding: 6px 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #07c160; }"
    );
    mainLayout->addWidget(m_usernameEdit);

    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet("font-size: 12px; color: #999;");
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    QPushButton *cancelBtn = new QPushButton("取消");
    cancelBtn->setStyleSheet(
        "QPushButton { background: #e0e0e0; border: none; border-radius: 4px; "
        "padding: 6px 20px; font-size: 13px; color: #333; }"
        "QPushButton:hover { background: #d0d0d0; }"
    );
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    QPushButton *sendBtn = new QPushButton("发送请求");
    sendBtn->setObjectName("sendBtn");
    sendBtn->setStyleSheet(
        "#sendBtn { background: #07c160; border: none; border-radius: 4px; "
        "padding: 6px 20px; font-size: 13px; color: white; }"
        "#sendBtn:hover { background: #06ad56; }"
        "#sendBtn:pressed { background: #059a4c; }"
    );
    connect(sendBtn, &QPushButton::clicked, this, &AddFriendDialog::onSendClicked);
    btnLayout->addWidget(sendBtn);

    mainLayout->addLayout(btnLayout);

    connect(m_usernameEdit, &QLineEdit::returnPressed, this, &AddFriendDialog::onSendClicked);
}

QString AddFriendDialog::targetUsername() const
{
    return m_usernameEdit->text().trimmed();
}

void AddFriendDialog::onSendClicked()
{
    QString username = targetUsername();
    if (username.isEmpty()) {
        m_statusLabel->setStyleSheet("font-size: 12px; color: #F55545;");
        m_statusLabel->setText("用户名不能为空");
        return;
    }
    emit friendRequestSent(username);
    m_statusLabel->setStyleSheet("font-size: 12px; color: #07c160;");
    m_statusLabel->setText("好友请求已发送！");
    m_usernameEdit->clear();
}
