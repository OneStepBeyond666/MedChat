#ifndef CONTACTLISTWIDGET_H
#define CONTACTLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QMap>
#include "client/chatclient.h"

class ContactListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ContactListWidget(QWidget *parent = nullptr);

    void updateContacts(const QMap<QString, ContactInfo> &contacts);
    QString selectedContact() const;
    void clearSelection();
    void filter(const QString &text);
    void setFriendRequestCount(int count);

signals:
    void contactSelected(const QString &username);
    void friendRequestEntryClicked();

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    void setupUI();
    void applyStyles();
    void rebuildList();

    /// 获取字符串的分组字母 (A-Z 或 #)
    static QChar groupLetter(const QString &str);

    QListWidget *m_listWidget;
    QMap<QString, ContactInfo> m_contacts;
    QString m_filterText;
    int m_friendRequestCount = 0;
};

#endif // CONTACTLISTWIDGET_H
