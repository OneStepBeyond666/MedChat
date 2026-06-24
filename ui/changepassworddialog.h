#ifndef CHANGEPASSWORDDIALOG_H
#define CHANGEPASSWORDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class ChatClient;

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ChangePasswordDialog(ChatClient *client, QWidget *parent = nullptr);

private slots:
    void onChangeClicked();
    void onChangePasswordResult(bool success, const QString &message);
    void onKicked(const QString &reason);

private:
    void setupUI();
    void applyStyles();

    ChatClient *m_client;

    QLineEdit *m_oldPassEdit;
    QLineEdit *m_newPassEdit;
    QLineEdit *m_confirmPassEdit;
    QPushButton *m_changeBtn;
    QPushButton *m_cancelBtn;
    QLabel *m_statusLabel;
};

#endif // CHANGEPASSWORDDIALOG_H
