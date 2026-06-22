#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

class ChatClient;

class LoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWindow(ChatClient *client, QWidget *parent = nullptr);

signals:
    void loginSuccess(const QString &username, const QString &role);

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onConnected();
    void onConnectionFailed(const QString &error);
    void onAuthResult(bool success, const QString &message, const QString &role);
    void onSwitchServerClicked();

private:
    void setupUI();
    void applyStyles();
    void showStatusMessage(const QString &msg, bool isError = false);
    void reconnectTo(const QString &host, quint16 port);

    ChatClient *m_client;

    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QComboBox *m_roleCombo;
    QPushButton *m_loginBtn;
    QPushButton *m_registerBtn;
    QLabel *m_statusLabel;
    QPushButton *m_switchBtn;    // 切换服务器按钮

    QString m_currentHost;
    quint16 m_currentPort;
};

#endif // LOGINWINDOW_H
