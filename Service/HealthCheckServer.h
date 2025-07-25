/**
 * @file HealthCheckServer.h
 * @brief 健康检查服务器头文件
 * @details 定义了HealthCheckServer类，提供HTTP接口监控服务健康状态
 * @author xubb
 * @date 20250711
 */

#ifndef HEALTHCHECKSERVER_H
#define HEALTHCHECKSERVER_H

#include <QObject>
#include <QTcpServer>

/**
 * @brief Service类前向声明
 * @details 避免循环引用
 */
class Service;

/**
 * @brief 健康检查服务器类
 * @details 提供HTTP接口，允许外部系统检查服务的健康状态
 */
class HealthCheckServer : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param service 服务对象指针
     * @param parent 父对象指针
     * @details 初始化健康检查服务器，需要Service指针以获取服务状态
     */
    explicit HealthCheckServer(Service* service, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~HealthCheckServer();

    /**
     * @brief 启动监听
     * @param port 监听端口号
     * @return 是否成功启动
     * @details 在指定端口上启动HTTP服务
     */
    bool startListen(quint16 port);

    /**
     * @brief 停止监听
     * @details 关闭HTTP服务
     */
    void stopListen();

private slots:
    /**
     * @brief 新连接处理槽函数
     * @details 处理新的TCP连接请求
     */
    void onNewConnection();

    /**
     * @brief 数据可读处理槽函数
     * @details 处理客户端发送的HTTP请求
     */
    void onReadyRead();

    /**
     * @brief 连接断开处理槽函数
     * @details 清理断开的连接资源
     */
    void onDisconnected();

private:
    /**
     * @brief 获取健康状态
     * @return 包含健康状态的JSON字符串
     * @details 收集服务的各项指标并生成健康状态报告
     */
    std::string getHealthStatus();

private:
    /**
     * @brief TCP服务器对象
     */
    QTcpServer* m_server;

    /**
     * @brief 服务对象指针
     * @details 用于获取服务的状态信息
     */
    Service* m_service;
};

#endif // HEALTHCHECKSERVER_H
