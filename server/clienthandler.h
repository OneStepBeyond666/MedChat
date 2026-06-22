#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>

class ChatServer;

class ClientHandler : public QObject
{
    Q_OBJECT
public:
    explicit ClientHandler(QTcpSocket *socket, ChatServer *server, QObject *parent = nullptr);
    ~ClientHandler();

    QString username() const { return m_username; }
    QString role() const { return m_role; }
    void setUsername(const QString &name) { m_username = name; }
    void setRole(const QString &role) { m_role = role; }

    void sendMessage(const QJsonObject &obj);
    QTcpSocket* socket() const { return m_socket; }

signals:
    void messageReceived(ClientHandler *handler, const QJsonObject &msg);
    void disconnected(ClientHandler *handler);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket *m_socket;
    ChatServer *m_server;
    QByteArray m_buffer;
    QString m_username;
    QString m_role;
};

#endif // CLIENTHANDLER_H
