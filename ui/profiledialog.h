#ifndef PROFILEDIALOG_H
#define PROFILEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QDateEdit>
#include <QByteArray>
#include <QVBoxLayout>

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
                           const QString &signature = QString(),
                           int gender = 0,
                           const QString &birthday = QString(),
                           const QString &region = QString(),
                           QWidget *parent = nullptr);

signals:
    void profileSaved(const QString &nickname, const QByteArray &avatarData,
                      const QString &signature, int gender,
                      const QString &birthday, const QString &region);
    void sendMessageRequested(const QString &username);
    void addFriendRequested(const QString &username);
    void changePasswordRequested();  // 修改密码请求

private slots:
    void onAvatarClicked();
    void onSaveClicked();
    void onSendMsgClicked();
    void onAddFriendClicked();
    void onChangePasswordClicked();
    void onNickTextChanged(const QString &text);
    void onSigTextChanged(const QString &text);

private:
    void setupSelfUI(QVBoxLayout *layout);
    void setupOtherUI(QVBoxLayout *layout);
    void updateAvatarDisplay();
    void applyStyles();
    void forceCalendarLightPalette();
    QWidget* createGroupTitle(const QString &title);
    QWidget* createSettingItem(const QString &label, QWidget *widget, QWidget *extra = nullptr);
    QWidget* createClickableItem(const QString &label, const QString &value);
    void addSeparator(QVBoxLayout *layout);
    bool eventFilter(QObject *obj, QEvent *event) override;

    Mode m_mode;
    QString m_username;
    QString m_nickname;
    QString m_role;
    QByteArray m_avatarData;
    QByteArray m_newAvatarData;
    bool m_avatarChanged = false;

    // 新增字段
    QString m_signature;
    int m_gender;
    QString m_birthday;
    QString m_region;

    // UI
    QLabel *m_avatarLabel;
    QLineEdit *m_nickEdit;
    QLabel *m_nickCounter;
    QLineEdit *m_sigEdit;
    QLabel *m_sigCounter;
    QComboBox *m_genderCombo;
    QDateEdit *m_birthdayEdit;
    QLineEdit *m_regionEdit;
    QLabel *m_roleLabel;
    QLabel *m_userLabel;
    QPushButton *m_saveBtn;
    QPushButton *m_sendMsgBtn;
    QPushButton *m_addFriendBtn;
};

#endif // PROFILEDIALOG_H
