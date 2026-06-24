#ifndef ADDFRIENDDIALOG_H
#define ADDFRIENDDIALOG_H

#include <QDialog>

class QLineEdit;
class QLabel;

class AddFriendDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddFriendDialog(QWidget *parent = nullptr);

    QString targetUsername() const;

signals:
    void friendRequestSent(const QString &username);

private slots:
    void onSendClicked();

private:
    QLineEdit *m_usernameEdit;
    QLabel *m_statusLabel;
};

#endif // ADDFRIENDDIALOG_H
