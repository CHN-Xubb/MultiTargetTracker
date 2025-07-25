/**
 * @file Service.h
 * @brief 服务类的头文件
 * @details 定义了Service类，负责管理应用程序的核心服务功能
 * @author xubb
 * @date 20250711
 */

#ifndef SERVICE_H
#define SERVICE_H

#include "qtservice.h"
#include <QObject>
#include <QThread>
#include "Worker.h"

/**
 * @brief 健康检查服务器的前向声明
 */
class HealthCheckServer;

/**
 * @brief 服务类，负责管理应用的核心功能
 * @details 继承自QtService<QCoreApplication>和QObject，提供应用程序的服务管理功能
 */
class Service : public QtService<QCoreApplication>, public QObject
{
public:
    /**
     * @brief 构造函数
     * @param argc 命令行参数数量
     * @param argv 命令行参数数组
     */
    explicit Service(int argc, char **argv);

    /**
     * @brief 析构函数
     */
    ~Service() override;

    /**
     * @brief 获取工作线程的最后心跳时间
     * @return 最后一次心跳的时间
     */
    QDateTime getLastWorkerHeartbeat() const;

    /**
     * @brief 检查工作线程是否正在运行
     * @return 如果工作线程正在运行则返回true，否则返回false
     */
    bool isWorkerThreadRunning() const;

public slots:
    /**
     * @brief 工作线程心跳响应槽函数
     * @param lastHeartbeat 心跳时间
     */
    void onWorkerHeartbeat(const QDateTime& lastHeartbeat);


protected:
    /**
     * @brief 启动服务
     * @details 重写父类的start方法，实现服务的启动逻辑
     */
    void start()override;

    /**
     * @brief 停止服务
     * @details 重写父类的stop方法，实现服务的停止逻辑
     */
    void stop()override;

    /**
     * @brief 暂停服务
     * @details 重写父类的pause方法，目前为空实现
     */
    void pause()override{}

    /**
     * @brief 恢复服务
     * @details 重写父类的resume方法，目前为空实现
     */
    void resume()override{}

private:
    /**
     * @brief 初始化配置
     * @details 读取并设置应用程序的配置信息
     */
    void initConfig();

    /**
     * @brief 初始化日志系统
     * @details 设置日志级别、输出方式等
     */
    void initLogging();

    /**
     * @brief 初始化工作线程
     * @details 创建并配置工作线程和工作对象
     */
    void initWorkerThread();

    /**
     * @brief 工作线程对象
     */
    QThread m_workerThread;

    /**
     * @brief 工作对象指针
     */
    Worker* m_worker;

    /**
     * @brief 健康检查服务器指针
     */
    HealthCheckServer* m_healthCheckServer;

    /**
     * @brief 工作线程最后心跳时间
     */
    QDateTime m_lastWorkerHeartbeat;

    /**
     * @brief 心跳互斥锁
     * @details 用于保护m_lastWorkerHeartbeat的线程安全访问
     */
    mutable QMutex m_heartbeatMutex;

    /**
     * @brief 服务运行状态标志
     * @details 用于跟踪服务是否正在运行
     */
    bool m_isServiceRunning;
};

#endif // SERVICE_H
