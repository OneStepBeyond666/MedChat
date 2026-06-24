#include "forgotpassworddialog.h"
#include "client/chatclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCryptographicHash>
#include <QTimer>

ForgotPasswordDialog::ForgotPasswordDialog(ChatClient *client, QWidget *parent)
    : QDialog(parent), m_client(client)
{
    setWindowTitle("忘记密码");
    setFixedSize(400, 320);
    setupUI();
    applyStyles();

    connect(m_client, &ChatClient::secQuestionReceived, this, &ForgotPasswordDialog::onSecQuestionReceived);
    connect(m_client, &ChatClient::resetPasswordResult, this, &ForgotPasswordDialog::onResetPasswordResult);
}

void ForgotPasswordDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 20, 30, 20);
    mainLayout->setSpacing(12);

    m_stack = new QStackedWidget;

    // ======================== Step 1: 输入用户名 ========================
    QWidget *page1 = new QWidget;
    QVBoxLayout *p1 = new QVBoxLayout(page1);
    p1->setSpacing(12);
    p1->setContentsMargins(0, 10, 0, 0);

    QLabel *title1 = new QLabel("找回密码");
    title1->setAlignment(Qt::AlignCenter);
    title1->setObjectName("titleLabel");
    p1->addWidget(title1);

    p1->addSpacing(10);

    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setPlaceholderText("请输入用户名");
    m_usernameEdit->setObjectName("inputField");
    p1->addWidget(m_usernameEdit);

    m_statusLabel1 = new QLabel;
    m_statusLabel1->setAlignment(Qt::AlignCenter);
    m_statusLabel1->setObjectName("statusLabel");
    p1->addWidget(m_statusLabel1);

    p1->addStretch();

    QHBoxLayout *btnLayout1 = new QHBoxLayout;
    QPushButton *cancelBtn1 = new QPushButton("取消");
    cancelBtn1->setObjectName("secondaryBtn");
    connect(cancelBtn1, &QPushButton::clicked, this, &QDialog::reject);
    m_nextBtn1 = new QPushButton("下一步");
    m_nextBtn1->setObjectName("primaryBtn");
    connect(m_nextBtn1, &QPushButton::clicked, this, &ForgotPasswordDialog::onRequestQuestion);
    btnLayout1->addWidget(cancelBtn1);
    btnLayout1->addWidget(m_nextBtn1);
    p1->addLayout(btnLayout1);

    m_stack->addWidget(page1);

    // ======================== Step 2: 密保答案 ========================
    QWidget *page2 = new QWidget;
    QVBoxLayout *p2 = new QVBoxLayout(page2);
    p2->setSpacing(12);
    p2->setContentsMargins(0, 10, 0, 0);

    QLabel *title2 = new QLabel("验证密保");
    title2->setAlignment(Qt::AlignCenter);
    title2->setObjectName("titleLabel");
    p2->addWidget(title2);

    p2->addSpacing(10);

    QLabel *qLabel = new QLabel("密保问题：");
    p2->addWidget(qLabel);

    m_questionLabel = new QLabel;
    m_questionLabel->setWordWrap(true);
    m_questionLabel->setStyleSheet("font-weight: bold; color: #333;");
    p2->addWidget(m_questionLabel);

    m_answerEdit = new QLineEdit;
    m_answerEdit->setPlaceholderText("请输入密保答案");
    m_answerEdit->setObjectName("inputField");
    p2->addWidget(m_answerEdit);

    m_statusLabel2 = new QLabel;
    m_statusLabel2->setAlignment(Qt::AlignCenter);
    m_statusLabel2->setObjectName("statusLabel");
    p2->addWidget(m_statusLabel2);

    p2->addStretch();

    QHBoxLayout *btnLayout2 = new QHBoxLayout;
    QPushButton *backBtn2 = new QPushButton("上一步");
    backBtn2->setObjectName("secondaryBtn");
    connect(backBtn2, &QPushButton::clicked, [this]() { m_stack->setCurrentIndex(0); });
    m_nextBtn2 = new QPushButton("下一步");
    m_nextBtn2->setObjectName("primaryBtn");
    connect(m_nextBtn2, &QPushButton::clicked, this, &ForgotPasswordDialog::onVerifyAnswer);
    btnLayout2->addWidget(backBtn2);
    btnLayout2->addWidget(m_nextBtn2);
    p2->addLayout(btnLayout2);

    m_stack->addWidget(page2);

    // ======================== Step 3: 新密码 ========================
    QWidget *page3 = new QWidget;
    QVBoxLayout *p3 = new QVBoxLayout(page3);
    p3->setSpacing(12);
    p3->setContentsMargins(0, 10, 0, 0);

    QLabel *title3 = new QLabel("设置新密码");
    title3->setAlignment(Qt::AlignCenter);
    title3->setObjectName("titleLabel");
    p3->addWidget(title3);

    p3->addSpacing(10);

    m_newPassEdit = new QLineEdit;
    m_newPassEdit->setPlaceholderText("新密码");
    m_newPassEdit->setEchoMode(QLineEdit::Password);
    m_newPassEdit->setObjectName("inputField");
    p3->addWidget(m_newPassEdit);

    m_confirmPassEdit = new QLineEdit;
    m_confirmPassEdit->setPlaceholderText("确认新密码");
    m_confirmPassEdit->setEchoMode(QLineEdit::Password);
    m_confirmPassEdit->setObjectName("inputField");
    p3->addWidget(m_confirmPassEdit);

    m_statusLabel3 = new QLabel;
    m_statusLabel3->setAlignment(Qt::AlignCenter);
    m_statusLabel3->setObjectName("statusLabel");
    p3->addWidget(m_statusLabel3);

    p3->addStretch();

    QHBoxLayout *btnLayout3 = new QHBoxLayout;
    QPushButton *backBtn3 = new QPushButton("上一步");
    backBtn3->setObjectName("secondaryBtn");
    connect(backBtn3, &QPushButton::clicked, [this]() { m_stack->setCurrentIndex(1); });
    m_resetBtn = new QPushButton("重置密码");
    m_resetBtn->setObjectName("primaryBtn");
    connect(m_resetBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onResetPassword);
    btnLayout3->addWidget(backBtn3);
    btnLayout3->addWidget(m_resetBtn);
    p3->addLayout(btnLayout3);

    m_stack->addWidget(page3);

    mainLayout->addWidget(m_stack);
}

void ForgotPasswordDialog::applyStyles()
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

void ForgotPasswordDialog::onRequestQuestion()
{
    QString username = m_usernameEdit->text().trimmed();
    if (username.isEmpty()) {
        m_statusLabel1->setText("请输入用户名");
        return;
    }
    m_targetUsername = username;
    m_nextBtn1->setEnabled(false);
    m_statusLabel1->setText("正在查询...");
    m_client->sendGetSecQuestion(username);
}

void ForgotPasswordDialog::onVerifyAnswer()
{
    QString answer = m_answerEdit->text().trimmed();
    if (answer.isEmpty()) {
        m_statusLabel2->setText("请输入密保答案");
        return;
    }
    m_stack->setCurrentIndex(2);
}

void ForgotPasswordDialog::onResetPassword()
{
    QString newPass = m_newPassEdit->text();
    QString confirm = m_confirmPassEdit->text();

    if (newPass.isEmpty()) {
        m_statusLabel3->setText("请输入新密码");
        return;
    }
    if (newPass != confirm) {
        m_statusLabel3->setText("两次输入的密码不一致");
        return;
    }

    // 对答案做预处理：小写 + 去空格，然后 SHA256
    QString answer = m_answerEdit->text().trimmed().toLower();
    QString answerHash = QString::fromLatin1(
        QCryptographicHash::hash(answer.toUtf8(), QCryptographicHash::Sha256).toHex()
    );

    m_resetBtn->setEnabled(false);
    m_statusLabel3->setText("正在重置...");
    m_client->sendResetPassword(m_targetUsername, answerHash, newPass);
}

void ForgotPasswordDialog::onSecQuestionReceived(bool success, const QString &question, const QString &error)
{
    m_nextBtn1->setEnabled(true);
    if (success) {
        m_questionLabel->setText(question);
        m_statusLabel1->setText("");
        m_stack->setCurrentIndex(1);
    } else {
        m_statusLabel1->setText(error);
    }
}

void ForgotPasswordDialog::onResetPasswordResult(bool success, const QString &message)
{
    m_resetBtn->setEnabled(true);
    if (success) {
        m_statusLabel3->setStyleSheet("color: #07c160; font-size: 12px;");
        m_statusLabel3->setText(message);
        QTimer::singleShot(1500, this, &QDialog::accept);
    } else {
        m_statusLabel3->setStyleSheet("color: #e74c3c; font-size: 12px;");
        m_statusLabel3->setText(message);
    }
}
