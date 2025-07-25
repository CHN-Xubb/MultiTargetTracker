/**
 * @file MessageRelayManager.h
 * @brief 消息中继管理器头文件
 * @details 定义了MessageRelayManager类，实现应用程序内部和外部的消息通信
 * @author xubb
 * @date 20250711
 */

#ifndef MESSAGERELAYMANAGER_H
#define MESSAGERELAYMANAGER_H

#include <QObject>
#include <QString>
#include "SimulatorDataExport.h"

/**
 * @brief 消息中继管理器类
 * @details 实现消息的发布订阅机制，连接应用内各模块与外部系统的通信
 *          采用单例模式确保全局只有一个消息管理器实例
 */
class MessageRelayManager : public QObject, public ISimulatorDataListener
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return MessageRelayManager的引用
     * @details 线程安全的单例实现，确保全局唯一的消息管理器实例
     */
    static MessageRelayManager& getInstance();

public slots:
    /**
     * @brief 发送消息
     * @param data 消息数据（JSON字符串）
     * @details 将消息广播到订阅者，任何模块都可以调用此函数发布消息
     */
    void sendMessage(const std::string& data);

signals:
    /**
     * @brief 消息接收信号
     * @param data 接收到的消息数据
     * @details 当有新消息时发出此信号，感兴趣的模块可以连接此信号
     */
    void messageReceived(const std::string& data);

private:
    /**
     * @brief 模拟器数据接口指针
     */
    ISimulatorData *m_pSimData;

    /**
     * @brief 中继数据结构
     */
    SimulatorData m_relayData;

    /**
     * @brief 私有构造函数
     * @param parent 父对象指针
     * @details 保持单例模式，外部不能直接创建实例
     */
    MessageRelayManager(QObject *parent = nullptr);

    /**
     * @brief 私有析构函数
     */
    ~MessageRelayManager();

    /**
     * @brief 禁用拷贝构造函数
     */
    MessageRelayManager(const MessageRelayManager&) = delete;

    /**
     * @brief 禁用赋值操作符
     */
    MessageRelayManager& operator=(const MessageRelayManager&) = delete;

    /**
     * @brief 发送数据
     * @param data 要发送的模拟器数据
     * @return 是否发送成功
     * @details 通过模拟器数据接口发布消息
     */
    bool sendData(const SimulatorData &data);

    /**
     * @brief 消息数据处理函数
     * @param data 接收到的模拟器数据
     * @details 实现ISimulatorDataListener接口的回调方法，接收外部消息
     */
    void OnMsgData(SimulatorData data) override;
};

/**
 * @brief 全局消息管理器访问宏
 * @details 提供便捷的方式访问全局单例实例
 */
#define g_MessageManager MessageRelayManager::getInstance()

#endif // MESSAGERELAYMANAGER_H
