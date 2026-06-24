#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QMap>
#include <QString>

class ClientHandler;
class UserManager;
class ServerDB;

class ChatServer : public QObject
{
    Q_OBJECT
public:
    explicit ChatServer(quint16 port = 9527, QObject *parent = nullptr);
    ~ChatServer();

    bool start();
    void stop();

    UserManager* userManager() const { return m_userManager; }

private slots:
    void onNewConnection();
    void onMessageReceived(ClientHandler *handler, const QJsonObject &msg);
    void onClientDisconnected(ClientHandler *handler);

private:
    void handleRegister(ClientHandler *handler, const QJsonObject &msg);
    void handleLogin(ClientHandler *handler, const QJsonObject &msg);
    void handleMessage(ClientHandler *handler, const QJsonObject &msg);
    void handleFileOffer(ClientHandler *handler, const QJsonObject &msg);
    void handleFileAccept(ClientHandler *handler, const QJsonObject &msg);
    void handleFileData(ClientHandler *handler, const QJsonObject &msg);
    void handleFileEnd(ClientHandler *handler, const QJsonObject &msg);
    void handleFileReject(ClientHandler *handler, const QJsonObject &msg);
    void handleOfflineSyncAck(ClientHandler *handler, const QJsonObject &msg);
    void handleUpdateProfile(ClientHandler *handler, const QJsonObject &msg);
    void handleFriendRequest(ClientHandler *handler, const QJsonObject &msg);
    void handleFriendResponse(ClientHandler *handler, const QJsonObject &msg);
    void handleGetSecQuestion(ClientHandler *handler, const QJsonObject &msg);
    void handleResetPassword(ClientHandler *handler, const QJsonObject &msg);
    void handleChangePassword(ClientHandler *handler, const QJsonObject &msg);

    void broadcastOnlineStatus();
    void sendContactList(ClientHandler *handler);
    void deliverPendingFriendRequests(ClientHandler *handler);
    ClientHandler* findClient(const QString &username) const;

    QTcpServer *m_tcpServer;
    ServerDB *m_serverDB;
    UserManager *m_userManager;
    QList<ClientHandler*> m_clients;
    quint16 m_port;
};

#endif // CHATSERVER_H
