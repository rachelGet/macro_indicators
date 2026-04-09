#pragma once
#ifndef MINI_HTTP_SERVER_H
#define MINI_HTTP_SERVER_H

#include <QTcpServer>
#include <QJsonObject>
#include <functional>
#include <vector>

/**
 * Lightweight HTTP server using QTcpServer + QTcpSocket.
 * Replaces Qt6::HttpServer so we only need Qt6::Core + Qt6::Network.
 *
 * Supports GET and POST routes that return JSON responses.
 */
class MiniHttpServer : public QTcpServer
{
    Q_OBJECT

public:
    enum Method { GET, POST };

    using Handler = std::function<QJsonObject()>;

    explicit MiniHttpServer(QObject* parent = nullptr);

    void route(const QString& path, Method method, Handler handler);
    bool startListening(quint16 port);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onReadyRead();

private:
    struct Route {
        QString path;
        Method  method;
        Handler handler;
    };

    std::vector<Route> m_routes;
};

#endif // MINI_HTTP_SERVER_H
