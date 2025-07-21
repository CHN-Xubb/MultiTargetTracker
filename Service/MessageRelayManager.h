#ifndef MESSAGERELAYMANAGER_H
#define MESSAGERELAYMANAGER_H

#include <QObject>
#include <QString>
#include "SimulatorDataExport.h"

class MessageRelayManager : public QObject, public ISimulatorDataListener
{
    // Q_OBJECT 宏是使用信号和槽所必需的
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例的静态方法
     * @return MessageRelayManager 的引用
     */
    static MessageRelayManager& getInstance();

public slots:
    /**
     * @brief 发送（发布）一个消息。
     * 任何模块都可以调用此函数来广播一个事件。
     * @param data 伴随消息的数据。
     */
    void sendMessage(const std::string& data);

signals:
    /**
     * @brief 当有消息被发送时，会发出此信号。
     * 任何感兴趣的模块都可以连接到这个信号。
     * @param data 伴随消息的数据。
     */
    void messageReceived(const std::string& data);

private:

    ISimulatorData *m_pSimData;
    SimulatorData m_relayData;

    // 保持单例模式的私有构造和禁止拷贝
    MessageRelayManager(QObject *parent = nullptr);
    ~MessageRelayManager();
    MessageRelayManager(const MessageRelayManager&) = delete;
    MessageRelayManager& operator=(const MessageRelayManager&) = delete;

    bool sendData(const SimulatorData &data);

    void OnMsgData(SimulatorData data);
};

// 定义一个方便访问单例的宏
#define g_MessageManager MessageRelayManager::getInstance()

#endif // MESSAGERELAYMANAGER_H
