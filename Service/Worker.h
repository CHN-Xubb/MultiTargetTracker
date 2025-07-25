/**
 * @file Worker.h
 * @brief 工作线程类的头文件
 * @details 定义了Worker类，负责处理数据和管理跟踪算法
 * @author xubb
 * @date 20250711
 */

#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QMutex>
#include "TrackManager.h"
#include <memory>
#include <vector>
#include "DataStructures.h"

/**
 * @brief 工作线程类
 * @details 负责处理观测数据、更新跟踪状态并发送结果
 */
class Worker : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit Worker(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~Worker();

signals:
    /**
     * @brief 心跳信号
     * @param lastHeartbeat 最后一次心跳的时间戳
     * @details 定期发送以表明工作线程正在运行
     */
    void heartbeat(const QDateTime& lastHeartbeat);

public slots:
    /**
     * @brief 开始工作
     * @details 初始化定时器并开始主处理循环
     */
    void doWork();

    /**
     * @brief 停止工作
     * @details 停止定时器并清理资源
     */
    void stopWork();

private slots:
    /**
     * @brief 定时器超时处理函数
     * @details 处理缓冲区中的观测数据，更新跟踪状态并发送结果
     */
    void onTimeout();

    /**
     * @brief 消息接收处理函数
     * @param message 接收到的消息内容
     * @details 解析接收到的消息并添加到观测数据缓冲区
     */
    void onMessageReceived(const std::string& message);

private:
    /**
     * @brief 处理跟踪结果并发送JSON数据
     * @param tracks 当前活动的跟踪对象集合
     * @details 将跟踪结果转换为JSON格式并通过消息管理器发送
     */
    void processAndSendTracks(const std::vector<std::shared_ptr<Track>>& tracks);

private:
    /**
     * @brief 定时器对象
     * @details 用于定期处理数据
     */
    QTimer *m_timer;

    /**
     * @brief 运行状态标志
     */
    bool m_running;

    /**
     * @brief 处理间隔时间(毫秒)
     */
    int m_interval;

    /**
     * @brief 跟踪管理器
     * @details 使用智能指针管理TrackManager生命周期
     */
    std::unique_ptr<TrackManager> m_trackManager;

    /**
     * @brief 观测数据缓冲区
     */
    std::vector<Measurement> m_measurementBuffer;

    /**
     * @brief 缓冲区互斥锁
     * @details 保护观测数据缓冲区的线程安全访问
     */
    QMutex m_bufferMutex;

    /**
     * @brief 最后心跳时间
     */
    QDateTime m_lastHeartbeat;
};

#endif // WORKER_H
