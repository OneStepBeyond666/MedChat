#include "clienthandler.h"
#include "common/protocol.h"
#include <QDebug>

ClientHandler::ClientHandler(QTcpSocket *socket, ChatServer *server, QObject *parent)
    : QObject(parent), m_socket(socket), m_server(server)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);
}

ClientHandler::~ClientHandler()
{
    // socket is deleted by Qt's parent-child system
}

void ClientHandler::sendMessage(const QJsonObject &obj)
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(Protocol::encode(obj));
        m_socket->flush();
    }
}

void ClientHandler::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    bool ok = false;
    QJsonObject obj;
    while (!(obj = Protocol::decode(m_buffer, ok)).isEmpty() && ok) {
        emit messageReceived(this, obj);
    }
}

void ClientHandler::onDisconnected()
{
    qDebug() << "[ClientHandler] Client disconnected:" << m_username;
    emit disconnected(this);
}
