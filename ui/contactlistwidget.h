#ifndef CONTACTLISTWIDGET_H
#define CONTACTLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
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

signals:
    void contactSelected(const QString &username);

private slots:
    void onSearchTextChanged(const QString &text);
    void onItemClicked(QListWidgetItem *item);

private:
    void setupUI();
    void applyStyles();

    QLineEdit *m_searchEdit;
    QListWidget *m_listWidget;
    QMap<QString, ContactInfo> m_contacts;
};

#endif // CONTACTLISTWIDGET_H
