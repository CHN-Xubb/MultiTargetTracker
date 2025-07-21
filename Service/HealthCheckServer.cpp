#include "HealthCheckServer.h"
#include "Service.h"
#include <QTcpSocket>
#include <QDateTime>
#include <QCoreApplication>
#include "nlohmann/json.hpp"
#include <string>

using json = nlohmann::json;

HealthCheckServer::HealthCheckServer(Service* service, QObject *parent)
    : QObject(parent), m_service(service)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &HealthCheckServer::onNewConnection);
}

HealthCheckServer::~HealthCheckServer() {}

bool HealthCheckServer::startListen(quint16 port)
{
    if (!m_server) return false;
    return m_server->listen(QHostAddress::Any, port);
}

void HealthCheckServer::stopListen()
{
    if (m_server) {
        m_server->close();
    }
}

std::string HealthCheckServer::getHealthStatus()
{
    json status; // 使用 nlohmann::json 对象
    status["serviceName"] = QCoreApplication::applicationName().toStdString();
    status["version"] = QCoreApplication::applicationVersion().toStdString();
    status["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();

    bool isHealthy = false;
    json details; // 使用 nlohmann::json 对象存储细节

    if (m_service) {
        Worker* worker = m_service->getWorker();
        if (worker && m_service->isWorkerThreadRunning()) {
            // 检查工作线程心跳是否在30秒内
            qint64 secsSinceLastHeartbeat = worker->getLastHeartbeat().secsTo(QDateTime::currentDateTimeUtc());
            if (secsSinceLastHeartbeat < 30) {
                isHealthy = true;
                details["workerThread"] = "运行中且健康";
                details["lastHeartbeat"] = worker->getLastHeartbeat().toString(Qt::ISODate).toStdString();
                details["secsSinceLastHeartbeat"] = secsSinceLastHeartbeat;
            } else {
                details["workerThread"] = "运行中但卡住 (无心跳)";
                details["lastHeartbeat"] = worker->getLastHeartbeat().toString(Qt::ISODate).toStdString();
                details["secsSinceLastHeartbeat"] = secsSinceLastHeartbeat;
            }
        } else {
            details["workerThread"] = "已停止或不可用";
        }
    } else {
        details["service"] = "不可用";
    }

    status["healthy"] = isHealthy;
    status["details"] = details;

    // 使用 dump() 方法将 json 对象序列化为 std::string
    // nlohmann/json 默认处理 UTF-8，能正确保留中文字符
    return status.dump();
}

void HealthCheckServer::onNewConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    if (socket) {
        connect(socket, &QTcpSocket::readyRead, this, &HealthCheckServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &HealthCheckServer::onDisconnected);
    }
}

void HealthCheckServer::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        std::string status_str = getHealthStatus();
        QByteArray response_body = QByteArray::fromStdString(status_str);

        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
        socket->write(response_body);
        socket->disconnectFromHost();
    }
}

void HealthCheckServer::onDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socket->deleteLater();
    }
}
