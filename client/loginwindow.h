#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>

class ChatClient;

class LoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWindow(ChatClient *client, QWidget *parent = nullptr);

signals:
    void loginSuccess(const QString &username, const QString &role, const QString &nickname, const QByteArray &avatarData);

private slots:
    // 登录页
    void onLoginClicked();
    // 注册页
    void onRegisterClicked();
    // 忘记密码
    void onForgotPasswordClicked();
    // 网络回调
    void onConnected();
    void onConnectionFailed(const QString &error);
    void onAuthResult(bool success, const QString &message, const QString &role, const QByteArray &avatarData);
    // 页面切换
    void showLoginPage();
    void showRegisterPage();
    // 服务器
    void onSwitchServerClicked();

private:
    void setupUI();
    void applyStyles();
    void showStatusMessage(const QString &msg, bool isError = false);
    void reconnectTo(const QString &host, quint16 port);
    void setButtonsEnabled(bool enabled);

    ChatClient *m_client;

    QStackedWidget *m_stack;

    // --- 登录页控件 ---
    QLineEdit *m_loginUserEdit;
    QLineEdit *m_loginPassEdit;
    QPushButton *m_loginBtn;
    QLabel *m_loginStatusLabel;
    QPushButton *m_loginSwitchBtn;
    QPushButton *m_forgotPassBtn;  // 忘记密码

    // --- 注册页控件 ---
    QLineEdit *m_regUserEdit;
    QLineEdit *m_regNickEdit;
    QLineEdit *m_regPassEdit;
    QLineEdit *m_regPassConfirmEdit;
    QComboBox *m_regRoleCombo;
    QLineEdit *m_regSecQuestionEdit;  // 密保问题
    QLineEdit *m_regSecAnswerEdit;    // 密保答案
    QPushButton *m_regBtn;
    QLabel *m_regStatusLabel;
    QPushButton *m_regSwitchBtn;

    QString m_currentHost;
    quint16 m_currentPort;
};

#endif // LOGINWINDOW_H
