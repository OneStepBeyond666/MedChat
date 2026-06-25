#ifndef FORWARDDIALOG_H
#define FORWARDDIALOG_H

#include <QDialog>
#include <QMap>
#include <QStringList>
#include <QByteArray>
#include <QListWidgetItem>
#include "client/chatclient.h"   // ContactInfo (完整定义，QMap 需要)

class QLineEdit;
class QListWidget;
class QLabel;
class QPushButton;

class ForwardDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ForwardDialog(const QMap<QString, ContactInfo> &contacts,
                           const QString &preview,
                           QWidget *parent = nullptr);

    QStringList selectedUsernames() const;

private slots:
    void onSearchTextChanged(const QString &text);
    void onItemClicked(QListWidgetItem *item);
    void onSendClicked();

private:
    void setupUI(const QString &preview);
    void applyStyles();
    void updateSelectionCount();

    QLineEdit *m_searchEdit;
    QListWidget *m_listWidget;
    QLabel *m_previewLabel;
    QLabel *m_countLabel;
    QPushButton *m_sendBtn;

    // username -> 是否选中
    QMap<QString, bool> m_selectionMap;
    // 保留联系人原始顺序
    QStringList m_allUsernames;
    // 联系人信息（用于头像和昵称显示）
    QMap<QString, ContactInfo> m_contacts;
};

#endif // FORWARDDIALOG_H
