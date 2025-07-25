/**
 * @file HealthCheckServer.cpp
 * @brief 健康检查服务器实现文件
 * @details 实现了HealthCheckServer类的各种功能，包括HTTP服务和健康状态检查
 * @author xubb
 * @date 20250711
 */

#include "HealthCheckServer.h"
#include "Service.h"
#include <QTcpSocket>
#include <QDateTime>
#include <QCoreApplication>
#include "nlohmann/json.hpp"
#include <string>

// 定义统一的日志宏
#define LOG_DEBUG(msg) qDebug() << "[HealthCheckServer::" << __FUNCTION__ << "] " << msg
#define LOG_INFO(msg) qInfo() << "[HealthCheckServer::" << __FUNCTION__ << "] " << msg
#define LOG_WARN(msg) qWarning() << "[HealthCheckServer::" << __FUNCTION__ << "] " << msg
#define LOG_ERROR(msg) qCritical() << "[HealthCheckServer::" << __FUNCTION__ << "] " << msg

// 函数跟踪宏
#define LOG_FUNCTION_BEGIN() LOG_DEBUG("开始")
#define LOG_FUNCTION_END() LOG_DEBUG("结束")

using json = nlohmann::json;

/**
 * @brief 构造函数
 * @param service 服务对象指针
 * @param parent 父对象指针
 * @details 初始化健康检查服务器，创建TCP服务器并设置信号槽连接
 */
HealthCheckServer::HealthCheckServer(Service* service, QObject *parent)
    : QObject(parent), m_service(service)
{
    LOG_FUNCTION_BEGIN();

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &HealthCheckServer::onNewConnection);

    LOG_INFO("健康检查服务器已创建");
    LOG_FUNCTION_END();
}

/**
 * @brief 析构函数
 */
HealthCheckServer::~HealthCheckServer()
{
    LOG_FUNCTION_BEGIN();
    LOG_INFO("健康检查服务器已销毁");
    LOG_FUNCTION_END();
}

/**
 * @brief 启动监听
 * @param port 监听端口号
 * @return 是否成功启动
 * @details 在指定端口上启动HTTP服务
 */
bool HealthCheckServer::startListen(quint16 port)
{
    LOG_FUNCTION_BEGIN();

    if (!m_server) {
        LOG_ERROR("TCP服务器对象为空，无法启动监听");
        return false;
    }

    bool success = m_server->listen(QHostAddress::Any, port);

    if (success) {
        LOG_INFO("成功在端口 " + QString::number(port) + " 上启动监听");
    } else {
        LOG_ERROR("无法在端口 " + QString::number(port) + " 上启动监听: " + m_server->errorString());
    }

    LOG_FUNCTION_END();
    return success;
}

/**
 * @brief 停止监听
 * @details 关闭HTTP服务
 */
void HealthCheckServer::stopListen()
{
    LOG_FUNCTION_BEGIN();

    if (m_server) {
        m_server->close();
        LOG_INFO("服务器已停止监听");
    } else {
        LOG_WARN("TCP服务器对象为空，无需停止");
    }

    LOG_FUNCTION_END();
}

/**
 * @brief 获取健康状态
 * @return 包含健康状态的JSON字符串
 * @details 收集服务的各项指标并生成健康状态报告
 */
std::string HealthCheckServer::getHealthStatus()
{
    LOG_FUNCTION_BEGIN();

    json status;
    status["serviceName"] = QCoreApplication::applicationName().toStdString();
    status["version"] = QCoreApplication::applicationVersion().toStdString();
    status["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();

    bool isHealthy = false;
    json details;

    if (m_service) {
        if (m_service->isWorkerThreadRunning()) {
            // 检查工作线程心跳是否在30秒内
            qint64 secsSinceLastHeartbeat = m_service->getLastWorkerHeartbeat().secsTo(QDateTime::currentDateTimeUtc());

            LOG_DEBUG("上次心跳距现在: " + QString::number(secsSinceLastHeartbeat) + " 秒");

            if (secsSinceLastHeartbeat < 30) {
                isHealthy = true;
                details["workerThread"] = "Running and healthy";
                details["lastHeartbeat"] = m_service->getLastWorkerHeartbeat().toString(Qt::ISODate).toStdString();
                details["secsSinceLastHeartbeat"] = secsSinceLastHeartbeat;

                LOG_DEBUG("工作线程状态: 正常运行");
            } else {
                details["workerThread"] = "Running but stuck (no heartbeat)";
                details["lastHeartbeat"] = m_service->getLastWorkerHeartbeat().toString(Qt::ISODate).toStdString();
                details["secsSinceLastHeartbeat"] = secsSinceLastHeartbeat;

                LOG_WARN("工作线程状态: 运行但无心跳");
            }
        } else {
            details["workerThread"] = "Stopped or unavailable";
            LOG_WARN("工作线程状态: 已停止或不可用");
        }
    } else {
        details["service"] = "Unavailable";
        LOG_ERROR("服务对象为空，无法获取健康状态");
    }

    status["healthy"] = isHealthy;
    status["details"] = details;

    std::string result = status.dump();
    LOG_DEBUG("生成的健康状态报告: " + QString::fromStdString(result));

    LOG_FUNCTION_END();
    return result;
}

/**
 * @brief 新连接处理槽函数
 * @details 处理新的TCP连接请求
 */
void HealthCheckServer::onNewConnection()
{
    LOG_FUNCTION_BEGIN();

    QTcpSocket *socket = m_server->nextPendingConnection();
    if (socket) {
        LOG_INFO("接受来自 " + socket->peerAddress().toString() + ":" +
                 QString::number(socket->peerPort()) + " 的新连接");

        connect(socket, &QTcpSocket::readyRead, this, &HealthCheckServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &HealthCheckServer::onDisconnected);
    } else {
        LOG_WARN("无效的连接请求");
    }

    LOG_FUNCTION_END();
}

/**
 * @brief 数据可读处理槽函数
 * @details 处理客户端发送的HTTP请求并返回健康状态
 */
void HealthCheckServer::onReadyRead()
{
    LOG_FUNCTION_BEGIN();

    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        LOG_DEBUG("收到来自 " + socket->peerAddress().toString() + ":" +
                  QString::number(socket->peerPort()) + " 的请求");

        // 获取健康状态
        std::string status_str = getHealthStatus();
        QByteArray response_body = QByteArray::fromStdString(status_str);

        // 构造HTTP响应
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
        socket->write(response_body);

        LOG_INFO("已发送健康状态响应，大小: " + QString::number(response_body.size()) + " 字节");

        // 发送完成后主动断开连接
        socket->disconnectFromHost();
    } else {
        LOG_ERROR("无效的socket对象");
    }

    LOG_FUNCTION_END();
}

/**
 * @brief 连接断开处理槽函数
 * @details 清理断开的连接资源
 */
void HealthCheckServer::onDisconnected()
{
    LOG_FUNCTION_BEGIN();

    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        LOG_INFO("连接已断开: " + socket->peerAddress().toString() + ":" +
                 QString::number(socket->peerPort()));

        socket->deleteLater();
    }

    LOG_FUNCTION_END();
}
