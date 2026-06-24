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

signals:
    void contactSelected(const QString &username);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    void setupUI();
    void applyStyles();

    QListWidget *m_listWidget;
    QMap<QString, ContactInfo> m_contacts;
    QString m_filterText;
};

#endif // CONTACTLISTWIDGET_H
