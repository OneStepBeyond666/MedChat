#ifndef FORGOTPASSWORDDIALOG_H
#define FORGOTPASSWORDDIALOG_H

#include <QDialog>
#include <QStackedWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class ChatClient;

class ForgotPasswordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ForgotPasswordDialog(ChatClient *client, QWidget *parent = nullptr);

private slots:
    void onRequestQuestion();
    void onVerifyAnswer();
    void onResetPassword();
    void onSecQuestionReceived(bool success, const QString &question, const QString &error);
    void onResetPasswordResult(bool success, const QString &message);

private:
    void setupUI();
    void applyStyles();

    ChatClient *m_client;
    QStackedWidget *m_stack;

    // Step 1: 输入用户名
    QLineEdit *m_usernameEdit;
    QPushButton *m_nextBtn1;
    QLabel *m_statusLabel1;

    // Step 2: 显示问题 + 输入答案
    QLabel *m_questionLabel;
    QLineEdit *m_answerEdit;
    QPushButton *m_nextBtn2;
    QLabel *m_statusLabel2;

    // Step 3: 输入新密码
    QLineEdit *m_newPassEdit;
    QLineEdit *m_confirmPassEdit;
    QPushButton *m_resetBtn;
    QLabel *m_statusLabel3;

    QString m_targetUsername;
};

#endif // FORGOTPASSWORDDIALOG_H