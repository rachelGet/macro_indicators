#include "MiniHttpServer.h"

#include <QTcpSocket>
#include <QJsonDocument>

MiniHttpServer::MiniHttpServer(QObject* parent)
    : QTcpServer(parent) {}

void MiniHttpServer::route(const QString& path, Method method, Handler handler) {
    m_routes.push_back({path, method, std::move(handler)});
}

bool MiniHttpServer::startListening(quint16 port) {
    return listen(QHostAddress::Any, port);
}

void MiniHttpServer::incomingConnection(qintptr socketDescriptor) {
    auto* socket = new QTcpSocket(this);
    socket->setSocketDescriptor(socketDescriptor);
    connect(socket, &QTcpSocket::readyRead, this, &MiniHttpServer::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

void MiniHttpServer::onReadyRead() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray raw = socket->readAll();
    QString request = QString::fromUtf8(raw);

    // Parse first line: "GET /path HTTP/1.1"
    int lineEnd = request.indexOf("\r\n");
    if (lineEnd < 0) lineEnd = request.indexOf("\n");
    if (lineEnd < 0) { socket->disconnectFromHost(); return; }

    QString firstLine = request.left(lineEnd);
    QStringList parts = firstLine.split(' ');
    if (parts.size() < 2) { socket->disconnectFromHost(); return; }

    QString methodStr = parts[0].toUpper();
    QString path      = parts[1];

    Method method = (methodStr == "POST") ? POST : GET;

    // Find matching route
    for (const auto& r : m_routes) {
        if (r.path == path && r.method == method) {
            QJsonObject result = r.handler();
            QByteArray body = QJsonDocument(result).toJson(QJsonDocument::Compact);

            QByteArray response;
            response.append("HTTP/1.1 200 OK\r\n");
            response.append("Content-Type: application/json\r\n");
            response.append("Content-Length: " + QByteArray::number(body.size()) + "\r\n");
            response.append("Connection: close\r\n");
            response.append("\r\n");
            response.append(body);

            socket->write(response);
            socket->flush();
            socket->disconnectFromHost();
            return;
        }
    }

    // 404
    QByteArray body = R"({"error":"not found"})";
    QByteArray response;
    response.append("HTTP/1.1 404 Not Found\r\n");
    response.append("Content-Type: application/json\r\n");
    response.append("Content-Length: " + QByteArray::number(body.size()) + "\r\n");
    response.append("Connection: close\r\n");
    response.append("\r\n");
    response.append(body);

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}
