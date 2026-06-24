#ifndef PROFILEDIALOG_H
#define PROFILEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QByteArray>

class ProfileDialog : public QDialog
{
    Q_OBJECT
public:
    enum Mode { SelfProfile, OtherProfile };

    explicit ProfileDialog(Mode mode,
                           const QString &username,
                           const QString &nickname,
                           const QString &role,
                           const QByteArray &avatarData,
                           QWidget *parent = nullptr);

signals:
    void profileSaved(const QString &nickname, const QByteArray &avatarData);
    void sendMessageRequested(const QString &username);
    void addFriendRequested(const QString &username);

private slots:
    void onAvatarClicked();
    void onSaveClicked();
    void onSendMsgClicked();
    void onAddFriendClicked();

private:
    void setupSelfUI();
    void setupOtherUI();
    void updateAvatarDisplay();

    Mode m_mode;
    QString m_username;
    QString m_nickname;
    QString m_role;
    QByteArray m_avatarData;
    QByteArray m_newAvatarData;  // 用户选择的新头像（SelfProfile 模式）
    bool m_avatarChanged = false;

    QLabel *m_avatarLabel;
    QLineEdit *m_nickEdit;       // SelfProfile only
    QLabel *m_nickLabel;         // OtherProfile only
    QLabel *m_roleLabel;
    QLabel *m_userLabel;
    QPushButton *m_saveBtn;      // SelfProfile only
    QPushButton *m_cancelBtn;
    QPushButton *m_sendMsgBtn;   // OtherProfile only
    QPushButton *m_addFriendBtn; // OtherProfile only
};

#endif // PROFILEDIALOG_H
